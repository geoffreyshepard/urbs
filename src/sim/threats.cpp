// Threats: weather, fire, collapse, disease, servile revolt, natural disasters.
#include <algorithm>
#include <cmath>

#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

bool World::patrol_covers(const char* service, const Building& b) const {
  ServiceId s = defs->service(service);
  if (!s.valid() || !services.ready(s.v) || b.entrance < 0) return false;
  if (services.cost_ticks(s.v, b.entrance) > 110) return false;
  BuildingId p = services.provider(s.v, b.entrance);
  return p.valid() && buildings.alive(p) && buildings[p].employed > 0;
}

void World::weather_daily() {
  rng::Stream& r = stream(rng::StreamId::Threats);
  const Season s = season(now);
  const f32 roll = r.unit();
  switch (s) {
    case Season::Spring: weather = roll < 0.35f ? Weather::Rain : roll < 0.38f ? Weather::Storm : Weather::Clear; break;
    case Season::Summer: weather = roll < 0.35f ? Weather::Heat : roll < 0.45f ? Weather::Rain : Weather::Clear; break;
    case Season::Autumn: weather = roll < 0.3f ? Weather::Rain : roll < 0.36f ? Weather::Storm : Weather::Clear; break;
    case Season::Winter: weather = roll < 0.25f ? Weather::Rain : roll < 0.35f ? Weather::Snow : Weather::Clear; break;
  }
  // A hot spell can turn into drought.
  if (weather == Weather::Heat && !effect(Effect::Drought) && r.chance(0.06f)) {
    set_effect(Effect::Drought, kTicksPerDay * 4);
    add_alert(AlertKind::Event, BuildingId{}, 1000);
  }
  if (weather == Weather::Storm) {
    // Storms damage a few roofs and endanger docked ships.
    buildings.for_each([&](BuildingId, Building& b) {
      if (b.state == BState::Active && r.chance(0.04f)) b.damage = static_cast<u8>(std::min(255, b.damage + 30));
    });
    for (auto& rs : routes)
      if (rs.ship_until > now && !effect(Effect::SeaTrade) && r.chance(0.3f)) rs.ship_until = now;
  }
}

void World::ignite(BuildingId id) {
  if (!buildings.alive(id)) return;
  Building& b = buildings[id];
  if (b.state != BState::Active && b.state != BState::Construction) return;
  b.state = BState::Burning;
  b.burn = static_cast<u8>(4 + stream(rng::StreamId::Threats).below(5));
  b.fire_risk = 0;
  ++fires_total;
  add_alert(AlertKind::Fire, id);
}

void World::ruin(BuildingId id) {
  Building& b = buildings[id];
  const bool collapsed = b.state != BState::Burning;
  b.state = BState::Ruin;
  b.burn = 0;
  b.damage = 255;
  // Occupants lose their home or job; the ruin blocks the site until demolished.
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i)) continue;
    if (citizens.work[i] == id) release_worker(i);
    if (citizens.home[i] == id) {
      citizens.home[i] = BuildingId{};
      citizens.remember(i, static_cast<u32>(day_index(now)),
                        static_cast<u8>(collapsed ? Memory::Sick : Memory::SurvivedFire), 0);
      ++citizens.token[i];
      schedule_citizen(i, now + 1 + (i % 20), CitizenEvent::Decide);
    }
    if (citizens.loc[i] == id) {
      ++citizens.token[i];
      schedule_citizen(i, now + 1, CitizenEvent::Decide);
    }
  }
  b.residents = 0;
  b.employed = 0;
  b.present = 0;
  b.jobs = 0;
  b.visitors = 0;
  // Households living here are now homeless.
  for (u32 hi = b.first_household; hi != kNoIndex;) {
    Household& h = households.h[hi];
    const u32 next = h.next_in_house;
    h.home = BuildingId{};
    h.next_in_house = kNoIndex;
    hi = next;
  }
  b.first_household = kNoIndex;
  for (u32 t = 0; t < econ.task_ids.high_water(); ++t)
    if (econ.task_ids.alive_index(t) && (econ.tasks[t].src == id || econ.tasks[t].dst == id)) cancel_task(t, true);
  if (b.inv != econ::kNoInv) {
    econ.destroy_inventory(b.inv);
    b.inv = econ::kNoInv;
  }
  if (collapsed) {
    ++collapses_total;
    add_alert(AlertKind::Collapse, id);
  }
  providers_dirty_ = true;
  desir_dirty = true;
  water_dirty = true;
}

void World::hazards_hourly(BuildingId id) {
  Building& b = buildings[id];
  const auto& d = defs->b(b.def);
  rng::Stream& r = stream(rng::StreamId::Threats);
  if (b.state == BState::Burning) {
    b.damage = static_cast<u8>(std::min(255, b.damage + 35));
    // Vigiles fight the fire; rain helps.
    f32 put_out = patrol_covers("vigiles", b) ? 0.35f : 0.03f;
    if (weather == Weather::Rain || weather == Weather::Snow) put_out += 0.15f;
    if (b.damage >= 255 || b.burn == 0) {
      ruin(id);
      return;
    }
    --b.burn;
    if (r.chance(put_out)) {
      b.state = BState::Active;
      b.burn = 0;
      return;
    }
    // Spread to neighbours.
    f32 spread = 0.12f;
    if (weather == Weather::Heat || effect(Effect::Drought)) spread *= 2.0f;
    if (weather == Weather::Rain) spread *= 0.4f;
    if (effect(Effect::Fireproof)) spread *= 0.3f;
    for (i32 ty = b.y - 1; ty <= b.y + b.h; ++ty)
      for (i32 tx = b.x - 1; tx <= b.x + b.w; ++tx) {
        if (!map.in_bounds(tx, ty)) continue;
        BuildingId nb = map.building[static_cast<size_t>(map.idx(tx, ty))];
        if (!nb.valid() || nb == id || !buildings.alive(nb)) continue;
        const auto& nd = defs->b(buildings[nb].def);
        if (nd.kind == BuildingKind::Decor || nd.kind == BuildingKind::Infrastructure) continue;
        if (buildings[nb].state == BState::Active && r.chance(spread)) ignite(nb);
      }
    return;
  }
  if (b.state != BState::Active) return;
  // Fire risk grows with the building's risk, summer heat and crowded insulae; vigiles reset it.
  i32 risk = d.fire_risk;
  if (d.kind == BuildingKind::House) risk += b.tier >= 3 ? 2 + (b.tier - 2) : 0;
  if (risk > 0) {
    f32 k = 1.0f;
    if (season(now) == Season::Summer) k *= 1.5f;
    if (weather == Weather::Heat || effect(Effect::Drought)) k *= 2.0f;
    if (weather == Weather::Rain || weather == Weather::Snow) k *= 0.3f;
    if (effect(Effect::Fireproof)) k *= 0.3f;
    if (effect(Effect::Hearth) && d.kind == BuildingKind::House) k *= 0.5f;
    if (patrol_covers("vigiles", b)) {
      b.fire_risk = static_cast<u8>(b.fire_risk / 4);
    } else {
      // About two weeks for an ordinary risky building to reach danger; kilns and smithies sooner.
      const f32 gain = static_cast<f32>(risk) * k / 4.0f;
      const int step = static_cast<int>(gain) + (r.chance(gain - std::floor(gain)) ? 1 : 0);
      b.fire_risk = static_cast<u8>(std::min(255, b.fire_risk + step));
    }
    if (b.fire_risk > 200 && r.chance(static_cast<f32>(b.fire_risk - 200) / 400.0f)) ignite(id);
  }
  // Structural wear: engineers repair it.
  i32 wear = d.collapse_risk;
  if (d.kind == BuildingKind::House && b.tier >= 3) wear += 2 * (b.tier - 2);
  if (patrol_covers("engineers", b)) {
    b.damage = static_cast<u8>(b.damage > 12 ? b.damage - 12 : 0);
  } else if (wear > 0 && r.chance(0.05f * static_cast<f32>(wear))) {
    ++b.damage;  // years for a plain house, weeks for a towering unmaintained insula
  }
  if (b.damage >= 250) ruin(id);
}

void World::disease_daily(BuildingId id) {
  Building& b = buildings[id];
  const auto& d = defs->b(b.def);
  if (b.state != BState::Active || d.kind != BuildingKind::House || b.residents == 0) return;
  rng::Stream& r = stream(rng::StreamId::Threats);
  const bool physician = requirement_met(b, req_physician_);
  if (b.plague > 0) {
    --b.plague;
    if (b.plague == 0) --plague_houses;
    // Residents fall ill; physicians help them recover (see decide: Healing).
    for (u32 i = 0; i < citizens.ids.high_water(); ++i)
      if (citizens.ids.alive_index(i) && citizens.home[i] == id && r.chance(physician ? 0.3f : 0.6f)) {
        citizens.health[i] = static_cast<u8>(std::max(30, citizens.health[i] - 60));
        citizens.remember(i, static_cast<u32>(day_index(now)), static_cast<u8>(Memory::Sick), 0);
      }
    // Spread to neighbouring houses.
    for (i32 ty = b.y - 3; ty <= b.y + b.h + 2; ++ty)
      for (i32 tx = b.x - 3; tx <= b.x + b.w + 2; ++tx) {
        if (!map.in_bounds(tx, ty)) continue;
        BuildingId nb = map.building[static_cast<size_t>(map.idx(tx, ty))];
        if (!nb.valid() || nb == id || !buildings.alive(nb)) continue;
        Building& n = buildings[nb];
        if (n.plague == 0 && defs->b(n.def).kind == BuildingKind::House && n.residents > 0 &&
            r.chance(physician ? 0.04f : 0.12f)) {
          n.plague = 5;
          ++plague_houses;
        }
      }
    return;
  }
  // Outbreak risk: marsh or bad neighbourhood, crowding, no sewer in insulae, no fountain water.
  f32 risk = 0.0004f;
  if (b.desirability_seen < 120) risk *= 2.0f;
  if (b.residents >= b.capacity && b.tier >= 2) risk *= 2.0f;
  if (b.tier >= 3 && !(b.flags & 0x10)) risk *= 2.0f;
  if (!requirement_met(b, req_fountain_)) risk *= 1.5f;
  if (physician) risk *= 0.4f;
  if (effect(Effect::Health)) risk *= 0.2f;
  if (r.chance(risk)) {
    b.plague = 5;
    ++plague_houses;
    add_alert(AlertKind::Disease, id);
  }
}

void World::revolt_check(bool forced) {
  u32 slaves = 0, happy = 0;
  for (u32 i = 0; i < citizens.ids.high_water(); ++i)
    if (citizens.ids.alive_index(i) && citizens.cls[i] == CitizenClass::Slave) {
      ++slaves;
      happy += citizens.happiness[i];
    }
  if (slaves == 0) return;
  const u32 pop = std::max<u32>(1, population());
  rng::Stream& r = stream(rng::StreamId::Threats);
  if (!forced) {
    if (slaves * 100 / pop < 15 || happy / slaves >= 60) return;
    if (!r.chance(policy.slave_treatment == 0 ? 0.25f : 0.08f)) return;
  }
  add_alert(AlertKind::Revolt, BuildingId{}, static_cast<u16>(slaves));
  // Rebels torch their workplaces and flee; guard patrols catch some of them.
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i) || citizens.cls[i] != CitizenClass::Slave) continue;
    if (!r.chance(0.7f)) continue;
    const BuildingId w = citizens.work[i];
    if (w.valid() && buildings.alive(w) && r.chance(0.3f)) ignite(w);
    const BuildingId h = citizens.home[i];
    const bool caught = h.valid() && buildings.alive(h) && patrol_covers("guard", buildings[h]) && r.chance(0.5f);
    if (caught) {
      citizens.happiness[i] = static_cast<u8>(citizens.happiness[i] / 2);
      continue;
    }
    ++emigrants_today;
    despawn_citizen(citizens.ids.id_at(i));
  }
}

void World::trigger(TriggerId t, i32 x, i32 y, i32 value) {
  rng::Stream& r = stream(rng::StreamId::Threats);
  auto at = [&]() -> BuildingId {
    if (map.in_bounds(x, y)) return map.building[static_cast<size_t>(map.idx(x, y))];
    BuildingId pick;
    u32 seen = 0;
    buildings.for_each([&](BuildingId id, const Building& b) {
      if (b.state != BState::Active) return;
      ++seen;
      if (r.below(seen) == 0) pick = id;
    });
    return pick;
  };
  switch (t) {
    case TriggerId::Fire:
      if (BuildingId b = at(); b.valid()) ignite(b);
      break;
    case TriggerId::Collapse:
      if (BuildingId b = at(); b.valid() && buildings[b].state == BState::Active) ruin(b);
      break;
    case TriggerId::Plague: {
      BuildingId b;
      if (map.in_bounds(x, y)) b = map.building[static_cast<size_t>(map.idx(x, y))];
      if (!b.valid()) {
        u32 seen = 0;
        buildings.for_each([&](BuildingId id, const Building& bb) {
          if (bb.state != BState::Active || defs->b(bb.def).kind != BuildingKind::House || bb.residents == 0) return;
          ++seen;
          if (r.below(seen) == 0) b = id;
        });
      }
      if (b.valid() && buildings[b].plague == 0) {
        buildings[b].plague = 6;
        ++plague_houses;
        add_alert(AlertKind::Disease, b);
      }
      break;
    }
    case TriggerId::Riot: {
      const u8 h = avg_happiness;
      avg_happiness = 0;
      const u8 tax = policy.tax_rate[2];
      policy.tax_rate[2] = 50;
      riot_check();
      riot_check();
      riot_check();
      avg_happiness = h;
      policy.tax_rate[2] = tax;
      break;
    }
    case TriggerId::Revolt: revolt_check(true); break;
    case TriggerId::Flood:
      // Low ground near water floods: damage and lost farm work.
      buildings.for_each([&](BuildingId, Building& b) {
        if (b.state != BState::Active) return;
        bool wet = false;
        for (i32 ty = b.y - 3; ty < b.y + b.h + 3 && !wet; ++ty)
          for (i32 tx = b.x - 3; tx < b.x + b.w + 3 && !wet; ++tx)
            if (map.in_bounds(tx, ty) && terrain_water(map.terrain[static_cast<size_t>(map.idx(tx, ty))]) &&
                map.height[static_cast<size_t>(map.idx(b.x, b.y))] <= 2)
              wet = true;
        if (!wet) return;
        b.damage = static_cast<u8>(std::min(255, b.damage + 90));
        b.work_progress = 0;
      });
      add_alert(AlertKind::Event, BuildingId{}, 1001);
      break;
    case TriggerId::Earthquake: {
      std::vector<BuildingId>& doomed = scratch_ids_;
      doomed.clear();
      buildings.for_each([&](BuildingId id, Building& b) {
        if (b.state != BState::Active) return;
        b.damage = static_cast<u8>(std::min(255, b.damage + 40 + static_cast<int>(r.below(140))));
        if (b.damage >= 250) doomed.push_back(id);
      });
      for (BuildingId id : doomed) ruin(id);
      add_alert(AlertKind::Event, BuildingId{}, 1002);
      break;
    }
    case TriggerId::Drought:
      set_effect(Effect::Drought, kTicksPerDay * static_cast<Tick>(std::max(2, value ? value : 4)));
      add_alert(AlertKind::Event, BuildingId{}, 1000);
      break;
    case TriggerId::Storm:
      weather = Weather::Storm;
      weather_daily();
      weather = Weather::Storm;
      break;
    case TriggerId::Blight:
      set_effect(Effect::Blight, kTicksPerDay * kDaysPerSeason);
      buildings.for_each([&](BuildingId, Building& b) {
        if (defs->b(b.def).kind == BuildingKind::Farm) b.work_progress = 0;
      });
      add_alert(AlertKind::Event, BuildingId{}, 1003);
      break;
    case TriggerId::Blessing:
      if (value >= 0 && static_cast<size_t>(value) < god_mood.size()) god_blessing(static_cast<u8>(value));
      break;
    case TriggerId::Raid: spawn_raid(UnitDefId{}, static_cast<u16>(std::max(1, value)), -1); break;
    case TriggerId::Wrath:
      if (value >= 0 && static_cast<size_t>(value) < god_mood.size()) god_wrath(static_cast<u8>(value));
      break;
    default: break;
  }
}

}  // namespace urbs::sim
