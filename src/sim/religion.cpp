// Gods: moods fed by temples, worship visits, offerings and festivals; blessings and wrath.
#include <algorithm>
#include <cstring>

#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

void World::hold_festival(u8 god, i32 size) {
  if (god >= god_mood.size() || !god_available[god]) return;
  size = std::clamp(size, 1, 3);
  const i64 cost = static_cast<i64>(population()) * size / 2 + 100 * size;
  if (!pay(cost, econ::Money::Festivals)) return;
  // Feast days double the effect.
  const bool feast = day_of_year(now) == defs->gods[god].festival_day;
  god_mood[god] = static_cast<u8>(std::min(100, god_mood[god] + (feast ? 2 : 1) * 15 * size));
  festival_until = now + kTicksPerDay * static_cast<Tick>(size);
  festival_god = god;
  const u16 day = static_cast<u16>(day_index(now));
  for (u32 i = 0; i < citizens.ids.high_water(); ++i)
    if (citizens.ids.alive_index(i)) {
      citizens.bonus[i] = static_cast<i8>(std::max<int>(citizens.bonus[i], 4 + 3 * size));
      citizens.bonus_until[i] = static_cast<u16>(day + 2 + size);
    }
  add_alert(AlertKind::Event, BuildingId{}, god);
}

void World::god_blessing(u8 g) {
  const std::string& b = defs->gods[g].blessing;
  const Tick season = kTicksPerDay * kDaysPerSeason;
  if (b == "favor") set_effect(Effect::Favor, season);
  else if (b == "births") set_effect(Effect::Births, season);
  else if (b == "morale") set_effect(Effect::Morale, season);
  else if (b == "happiness") set_effect(Effect::Happy, season);
  else if (b == "harvest") set_effect(Effect::Harvest, season);
  else if (b == "sea_trade") set_effect(Effect::SeaTrade, season);
  else if (b == "trade_prices") set_effect(Effect::TradePrices, season);
  else if (b == "workshops") set_effect(Effect::Workshops, season);
  else if (b == "fireproof") set_effect(Effect::Fireproof, season);
  else if (b == "hearth") set_effect(Effect::Hearth, season);
  else if (b == "health") set_effect(Effect::Health, season);
  add_alert(AlertKind::GodAngry, BuildingId{}, static_cast<u16>(0x100 | g));  // 0x100 = blessing
}

void World::god_wrath(u8 g) {
  const std::string& w = defs->gods[g].wrath;
  const Tick season = kTicksPerDay * kDaysPerSeason;
  rng::Stream& r = stream(rng::StreamId::Religion);
  auto random_building = [&](bool houses_only) {
    BuildingId pick;
    u32 seen = 0;
    buildings.for_each([&](BuildingId id, const Building& b) {
      if (b.state != BState::Active) return;
      if (houses_only && defs->b(b.def).kind != BuildingKind::House) return;
      ++seen;
      if (r.below(seen) == 0) pick = id;  // reservoir sampling
    });
    return pick;
  };
  if (w == "lightning") {
    if (BuildingId b = random_building(false)) ignite(b);
  } else if (w == "barren") {
    set_effect(Effect::Barren, season);
  } else if (w == "desertion") {
    set_effect(Effect::Desertion, season);
  } else if (w == "discontent") {
    set_effect(Effect::Discontent, season);
  } else if (w == "blight") {
    trigger(TriggerId::Blight, 0, 0, 0);
  } else if (w == "earthquake") {
    trigger(TriggerId::Earthquake, 0, 0, 0);
  } else if (w == "no_trade") {
    set_effect(Effect::NoTrade, season);
  } else if (w == "accidents") {
    buildings.for_each([&](BuildingId, Building& b) {
      if (b.state == BState::Active && defs->b(b.def).recipe.valid() && r.chance(0.2f))
        b.damage = static_cast<u8>(std::min(255, b.damage + 80));
    });
  } else if (w == "great_fire") {
    for (int k = 0; k < 4; ++k)
      if (BuildingId b = random_building(false)) ignite(b);
  } else if (w == "house_fires") {
    for (int k = 0; k < 3; ++k)
      if (BuildingId b = random_building(true)) ignite(b);
  } else if (w == "plague") {
    trigger(TriggerId::Plague, -1, -1, 0);
  }
  add_alert(AlertKind::GodAngry, BuildingId{}, g);
}

void World::religion_daily() {
  if (god_mood.empty()) return;
  const u32 n = static_cast<u32>(god_mood.size());
  // Temples dedicated to each god (and shrines, which honour all gods a little).
  std::vector<u32>& temples = scratch_u32_;
  temples.assign(n, 0);
  u32 shrines = 0;
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state != BState::Active) return;
    const auto& d = defs->b(b.def);
    if (!d.service.valid() || defs->services[d.service.v].need != data::NeedKind::Religion) return;
    if (d.id == "shrine") {
      ++shrines;
      return;
    }
    if (d.id == "pantheon") {
      for (u32 g = 0; g < n; ++g) temples[g] += 4;
      return;
    }
    const u32 weight = d.id == "temple" ? 1 : d.id == "great_temple" ? 3 : 6;
    if (b.extra < n) temples[b.extra] += weight;
  });
  const u32 pop = std::max<u32>(1, population());
  rng::Stream& r = stream(rng::StreamId::Religion);
  for (u32 g = 0; g < n; ++g) {
    if (!god_available[g]) continue;
    // Target mood: one temple per ~300 citizens is ideal; worship visits and shrines help.
    const u32 want = std::max<u32>(1, pop / 300);
    i32 target = 15 + static_cast<i32>(std::min<u32>(55, temples[g] * 55 / want));
    target += static_cast<i32>(std::min<u32>(10, shrines * 30 / std::max<u32>(1, pop / 10 + 1)));
    target += static_cast<i32>(std::min<u32>(20, god_offerings[g] * 100 / pop));
    target = std::clamp(target, 0, 100);
    i32 m = god_mood[g];
    m += (target > m) ? std::min(3, target - m) : -std::min(2, m - target);
    god_mood[g] = static_cast<u8>(std::clamp(m, 0, 100));
    god_offerings[g] = 0;
    if (pop < 60) continue;  // the gods ignore hamlets
    if (god_mood[g] >= 85 && r.chance(0.05f)) {
      god_blessing(static_cast<u8>(g));
      god_mood[g] = 70;
    } else if (god_mood[g] <= 15 && r.chance(0.08f)) {
      god_wrath(static_cast<u8>(g));
      god_mood[g] = 30;
    }
  }
}

}  // namespace urbs::sim
