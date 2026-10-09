// Citizen behaviour: utility-scored decisions at decision points, activities, arrivals.
// See docs/06-simulation-design.md section 5.
#include <algorithm>

#include "sim/world.hpp"

namespace urbs::sim {

using data::NeedKind;

namespace {
struct Candidate {
  Activity act = Activity::Home;
  BuildingId target;
  Reason reason = Reason::Idle;
  int score = 0;
};
}  // namespace

BuildingId World::best_service_for(NeedKind need, i32 from_tile, u16* cost_out) const {
  if (from_tile < 0 || need == NeedKind::None) return BuildingId{};
  BuildingId best;
  int best_score = -(1 << 30);
  for (ServiceId s : need_services[static_cast<int>(need)]) {
    if (!services.ready(s.v)) continue;
    const u16 cost = services.cost_ticks(s.v, from_tile);
    const auto& sd = defs->services[s.v];
    if (cost == 0xFFFF || cost > sd.range) continue;
    BuildingId b = services.provider(s.v, from_tile);
    if (!b.valid() || !buildings.alive(b) || buildings[b].state != BState::Active) continue;
    const int score = static_cast<int>(sd.quality) * 2 - static_cast<int>(cost);
    if (score > best_score) {
      best_score = score;
      best = b;
      if (cost_out) *cost_out = cost;
    }
  }
  return best;
}

void World::decide(u32 i) {
  auto& C = citizens;
  ++stats.decisions;
  // Leave the current activity.
  if (C.loc[i].valid() && buildings.alive(C.loc[i])) {
    Building& lb = buildings[C.loc[i]];
    if ((C.activity[i] == Activity::Work || C.activity[i] == Activity::Build) && lb.present > 0) {
      --lb.present;
      // Skill: what was added on arrival comes off, then experience grows.
      const int sk = skill_of_sector(sector_of(lb));
      u8& skill = C.skills[static_cast<size_t>(i) * kSkillCount + sk];
      const u32 contrib = static_cast<u32>(skill * trait_mul(i, &data::TraitDef::work));
      lb.skill_sum = lb.skill_sum > contrib ? lb.skill_sum - contrib : 0;
      const u8 cap = (C.flags[i] & kFlagSchooled) ? 255 : 140;
      const f32 grow = trait_mul(i, &data::TraitDef::skill_rate);
      if (skill < cap && stream(rng::StreamId::Decisions).chance(std::min(1.0f, grow * 0.8f))) ++skill;
      if (lb.present == 0) lb.skill_sum = 0;
    }
    if ((C.activity[i] == Activity::Visit || C.activity[i] == Activity::School) && lb.visitors > 0) --lb.visitors;
  }
  if (C.activity[i] == Activity::Leave) return;
  if (C.flags[i] & kFlagVisitor) {
    // Traders: after trading (or if lost), walk back to the edge and leave.
    C.reason[i] = Reason::None;
    request_walk(i, current_tile(i), BuildingId{}, entry_tile, Activity::Leave);
    return;
  }
  const u32 h = hour_of_day(now);
  const i32 here = current_tile(i);
  BuildingId home = C.home[i];
  Household* hh = C.household[i].valid() && households.ids.alive(C.household[i]) ? &households[C.household[i]] : nullptr;

  if (hh && hh->leaving) {
    C.activity[i] = Activity::Leave;
    C.reason[i] = Reason::Emigrating;
    request_walk(i, here, BuildingId{}, entry_tile, Activity::Leave);
    C.activity[i] = C.activity[i] == Activity::Walk ? Activity::Walk : Activity::Leave;
    return;
  }
  if (!home.valid() || !buildings.alive(home)) {
    // Homeless: look for space in a house, otherwise wander and eventually leave.
    const u16 members = hh ? hh->members : 1;
    BuildingId h2 = find_vacant_house(here >= 0 ? here : entry_tile, members);
    if (h2.valid() && hh) {
      for (u32 k = 0; k < C.ids.high_water(); ++k)
        if (C.ids.alive_index(k) && C.household[k] == C.household[i]) {
          C.home[k] = h2;
          ++buildings[h2].residents;
        }
      hh->home = h2;
      hh->next_in_house = buildings[h2].first_household;
      buildings[h2].first_household = C.household[i].index();
      C.reason[i] = Reason::Homeless;
      request_walk(i, here, h2, buildings[h2].entrance, Activity::Home);
      return;
    }
    C.activity[i] = Activity::Wander;
    C.reason[i] = Reason::Homeless;
    schedule_citizen(i, now + kTicksPerHour * 3, CitizenEvent::Decide);
    return;
  }

  const u32 age = C.age_years(i, now);
  const bool adult = age >= 14;
  const i32 home_tile = buildings[home].entrance;
  const u8 food = C.need(i, Need::Food, now), water = C.need(i, Need::Water, now), rest = C.need(i, Need::Rest, now);
  const bool has_food = hh && household_food(*hh) > 0;
  const bool has_water = hh && hh->water > 0;

  Candidate best{Activity::Home, home, adult ? (C.work[i].valid() ? Reason::Idle : Reason::Unemployed) : Reason::Child, 50};
  auto consider = [&](Activity a, BuildingId t, Reason r, int score) {
    if (score > best.score && t.valid() && buildings.alive(t)) best = {a, t, r, score};
  };

  if (h >= 22 || h < 5) consider(Activity::Sleep, home, Reason::Night, 1000);
  if (rest < 50) consider(Activity::Sleep, home, Reason::Tired, 900);
  if (water < 110) {
    if (has_water) {
      consider(Activity::Home, home, Reason::Thirsty, 950);
    } else {
      BuildingId well = best_service_for(NeedKind::Water, here, nullptr);
      if (well.valid()) consider(Activity::Water, well, Reason::FetchWater, 940);
      else consider(Activity::Home, home, Reason::NoWater, 120);
    }
  }
  if (food < 90) consider(Activity::Home, home, has_food ? Reason::Hungry : Reason::NoFood, has_food ? 920 : 110);

  const bool shift = h >= 7 && h < 17 && h != 12;
  BuildingId job = C.work[i];
  if (adult && job.valid() && buildings.alive(job) && shift) {
    const Building& jb = buildings[job];
    if (jb.state == BState::Construction) consider(Activity::Build, job, Reason::Building, 800);
    else consider(Activity::Work, job, Reason::Shift, 800);
  }
  if (adult && job.valid() && h == 12) consider(Activity::Home, home, Reason::Midday, 300);
  // Children go to school in the morning.
  if (age >= 6 && age < 14 && h >= 8 && h < 12) {
    ServiceId school = defs->service("school");
    if (school.valid() && services.ready(school.v) &&
        services.cost_ticks(school.v, home_tile) <= defs->services[school.v].range) {
      BuildingId sb = services.provider(school.v, home_tile);
      if (sb.valid()) consider(Activity::School, sb, Reason::School, 760);
    }
  }

  if (adult && hh && h >= 7 && h < 20) {
    if (household_food(*hh) < hh->members * 2u && (!hh->shopper.valid() || hh->shopper == C.ids.id_at(i))) {
      ServiceId market = defs->service("market");
      if (market.valid() && services.ready(market.v)) {
        const u16 cost = services.cost_ticks(market.v, home_tile);
        BuildingId m = services.provider(market.v, home_tile);
        if (m.valid() && cost <= defs->services[market.v].range) consider(Activity::Shop, m, Reason::BuyFood, 700);
      }
    }
    if (hh->water < hh->members && h < 20 && (!hh->fetcher.valid() || hh->fetcher == C.ids.id_at(i))) {
      BuildingId well = best_service_for(NeedKind::Water, home_tile, nullptr);
      if (well.valid()) consider(Activity::Water, well, Reason::FetchWater, 650);
    }
  }
  // Leisure: worship, baths, entertainment in free time.
  if (h >= 7 && h < 22) {
    struct L {
      Need need;
      NeedKind kind;
      Reason reason;
    };
    static constexpr L kLeisure[] = {{Need::Religion, NeedKind::Religion, Reason::Worship},
                                     {Need::Hygiene, NeedKind::Hygiene, Reason::Bathe},
                                     {Need::Entertainment, NeedKind::Entertainment, Reason::Entertainment},
                                     {Need::Education, NeedKind::Education, Reason::School}};
    for (const L& l : kLeisure) {
      const u8 v = C.need(i, l.need, now);
      if (v >= 170) continue;
      if (!adult && l.need == Need::Entertainment) continue;
      if (l.need == Need::Education && (!adult || (C.cls[i] != CitizenClass::Patrician && C.cls[i] != CitizenClass::Eques)))
        continue;
      BuildingId s = best_service_for(l.kind, home_tile, nullptr);
      if (s.valid()) consider(Activity::Visit, s, l.reason, 400 + (170 - v));
    }
  }

  C.reason[i] = best.reason;
  if (best.act == Activity::Shop && hh) hh->shopper = C.ids.id_at(i);
  if (best.act == Activity::Water && hh && best.reason == Reason::FetchWater) hh->fetcher = C.ids.id_at(i);
  if (best.act == Activity::Work && is_porter_job(best.target) && claim_task(i)) {
    C.reason[i] = Reason::Hauling;
    return;
  }
  if (C.loc[i] == best.target) {
    start_activity(i, best.act);
    return;
  }
  request_walk(i, here, best.target, buildings[best.target].entrance, best.act);
}

void World::start_activity(u32 i, Activity a) {
  auto& C = citizens;
  C.rebase_needs(i, now);
  C.activity[i] = a;
  C.next[i] = Activity::None;
  const size_t nb = static_cast<size_t>(i) * kNeedCount;
  auto rate = [&](Need n, i8 r) { C.need_rate[nb + static_cast<size_t>(n)] = r; };
  rate(Need::Food, -5);
  rate(Need::Water, -8);
  rate(Need::Rest, -7);
  rate(Need::Hygiene, -4);
  rate(Need::Religion, -2);
  rate(Need::Entertainment, -3);
  const bool upper = C.cls[i] == CitizenClass::Patrician || C.cls[i] == CitizenClass::Eques;
  rate(Need::Education, upper ? static_cast<i8>(-2 * trait_mul(i, &data::TraitDef::education_rate)) : 0);
  rate(Need::Religion, static_cast<i8>(-2 * trait_mul(i, &data::TraitDef::religion_rate)));
  rate(Need::Hygiene, static_cast<i8>(-4 * trait_mul(i, &data::TraitDef::hygiene_rate)));
  rate(Need::Entertainment, static_cast<i8>(-3 * trait_mul(i, &data::TraitDef::entertainment_rate)));
  const u32 h = hour_of_day(now);
  Tick end = now + kTicksPerHour;
  const i32 j = jitter(i);
  auto at_hour = [&](u32 hour) { return static_cast<Tick>(static_cast<i64>(next_hour(now, hour)) + j); };
  Household* hh = C.household[i].valid() && households.ids.alive(C.household[i]) ? &households[C.household[i]] : nullptr;
  switch (a) {
    case Activity::Sleep:
      rate(Need::Rest, 40);
      rate(Need::Food, -3);
      rate(Need::Water, -3);
      end = (h < 5 || h >= 22) ? at_hour(5) : now + kTicksPerHour * 3;
      break;
    case Activity::Work:
    case Activity::Build: {
      if (C.loc[i].valid() && buildings.alive(C.loc[i])) {
        Building& wb = buildings[C.loc[i]];
        ++wb.present;
        const int sk = skill_of_sector(sector_of(wb));
        wb.skill_sum += static_cast<u32>(C.skills[static_cast<size_t>(i) * kSkillCount + sk] *
                                         trait_mul(i, &data::TraitDef::work));
      }
      rate(Need::Rest, -9);
      rate(Need::Food, -7);
      end = h < 12 ? at_hour(12) : at_hour(17);
      if (a == Activity::Work && is_porter_job(C.work[i])) end = std::min<Tick>(end, now + kTicksPerHour / 2);
      break;
    }
    case Activity::Haul:
      rate(Need::Rest, -9);
      rate(Need::Food, -7);
      haul_step(i);
      return;
    case Activity::Trade: {
      // The first trader of the caravan to arrive does the exchange for this visit.
      const u32 p = C.task[i];
      if (p < routes.size() && !routes[p].caravan_traded && C.loc[i].valid()) {
        trade_exchange(p, C.loc[i]);
        routes[p].caravan_traded = 1;
      }
      end = now + kTicksPerHour;
      break;
    }
    case Activity::School: {
      Building& b = buildings[C.loc[i]];
      ++b.visitors;
      C.flags[i] |= kFlagSchooled;
      end = at_hour(12);
      break;
    }
    case Activity::Shop:
      if (hh) {
        household_shop(i, C.loc[i]);
        hh->shopper = CitizenId{};
      }
      end = now + kTicksPerHour / 2;
      break;
    case Activity::Water:
      if (hh) {
        hh->water = static_cast<u16>(std::min<u32>(hh->water + hh->members * 3u, 60u));
        hh->fetcher = CitizenId{};
      }
      C.set_need(i, Need::Water, 255);
      C.carrying[i] = 0;
      end = now + 10;
      break;
    case Activity::Visit: {
      Building& b = buildings[C.loc[i]];
      const auto& bd = defs->b(b.def);
      const auto& sd = defs->services[bd.service.v];
      Need n = Need::Count;
      switch (sd.need) {
        case NeedKind::Religion: n = Need::Religion; break;
        case NeedKind::Hygiene: n = Need::Hygiene; break;
        case NeedKind::Entertainment: n = Need::Entertainment; break;
        case NeedKind::Education: n = Need::Education; break;
        default: break;
      }
      if (b.visitors >= bd.service_capacity && bd.service_capacity > 0) {
        // Full: go home; the need is nudged up so the citizen doesn't come straight back.
        C.reason[i] = Reason::ServiceFull;
        C.activity[i] = Activity::Home;
        if (n != Need::Count) C.set_need(i, n, static_cast<u8>(std::min(255, C.need(i, n, now) + 60)));
        end = now + 1;
        break;
      }
      ++b.visitors;
      ++b.served_today;
      // A visit always lifts the need above the "wants to go" threshold; better services last longer.
      if (n != Need::Count) C.set_need(i, n, std::max<u8>(C.need(i, n, now), static_cast<u8>(175 + sd.quality / 3)));
      end = now + std::max<u16>(5, sd.visit_ticks);
      break;
    }
    case Activity::Home: {
      if (C.need(i, Need::Food, now) < 180) household_eat(i);
      if (hh && hh->water > 0 && C.need(i, Need::Water, now) < 200) {
        --hh->water;
        C.set_need(i, Need::Water, 255);
      }
      C.carrying[i] = 0;
      if (h < 5 || h >= 22) end = at_hour(5);
      else if (h < 7) end = at_hour(7);
      else end = std::min<Tick>(now + kTicksPerHour * 2, h < 12 ? at_hour(12) : h < 13 ? at_hour(13) : h < 17 ? at_hour(17) : at_hour(22));
      break;
    }
    default: break;
  }
  if (end <= now) end = now + 1;
  C.activity_end[i] = end;
  schedule_citizen(i, end, CitizenEvent::Decide);
}

void World::arrive(u32 i) {
  auto& C = citizens;
  ++stats.arrivals;
  if (C.path[i] != path::kNoHandle) {
    paths.store().unref(C.path[i]);
    C.path[i] = path::kNoHandle;
    if (walking > 0) --walking;
  }
  if (C.next[i] == Activity::Leave) {
    finish_leave(i);
    return;
  }
  C.loc[i] = C.dest[i];
  if (!C.loc[i].valid() || !buildings.alive(C.loc[i])) {
    C.loc[i] = BuildingId{};
    C.activity[i] = Activity::Wander;
    schedule_citizen(i, now + 1, CitizenEvent::Decide);
    return;
  }
  start_activity(i, C.next[i]);
}

}  // namespace urbs::sim
