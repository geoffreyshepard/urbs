// Lifecycle (aging, death, marriage, births, inheritance), traits, skills, life goals, slavery,
// manumission, crime and riots. Runs inside the staggered daily citizen pass.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

bool World::has_trait(u32 i, const char* id) const {
  for (int k = 0; k < kMaxTraits; ++k) {
    const u8 t = citizens.traits[static_cast<size_t>(i) * kMaxTraits + k];
    if (t < defs->traits.size() && defs->traits[t].id == id) return true;
  }
  return false;
}

void World::assign_traits(u32 i, u32 pa, u32 pb) {
  auto& C = citizens;
  rng::Stream& r = stream(rng::StreamId::Citizens);
  const u32 nt = static_cast<u32>(defs->traits.size());
  u8* tr = &C.traits[static_cast<size_t>(i) * kMaxTraits];
  for (int k = 0; k < kMaxTraits; ++k) tr[k] = 0xFF;
  if (nt == 0) return;
  const u32 count = 1 + r.below(3);
  for (u32 k = 0; k < count; ++k) {
    u8 t = static_cast<u8>(r.below(nt));
    // Children inherit a parent's trait 30% of the time.
    const u32 parent = r.chance(0.5f) ? pa : pb;
    if (parent != kNoIndex && r.chance(0.3f)) {
      const u8 pt = C.traits[static_cast<size_t>(parent) * kMaxTraits + r.below(kMaxTraits)];
      if (pt < nt) t = pt;
    }
    bool dup = false;
    for (u32 m = 0; m < k; ++m)
      if (tr[m] == t) dup = true;
    // Opposites never combine.
    auto id = [&](u8 x) { return x < nt ? defs->traits[x].id.c_str() : ""; };
    for (u32 m = 0; m < k; ++m) {
      const char* a = id(tr[m]);
      const char* b = id(t);
      auto pair = [&](const char* x, const char* y) {
        return (std::strcmp(a, x) == 0 && std::strcmp(b, y) == 0) || (std::strcmp(a, y) == 0 && std::strcmp(b, x) == 0);
      };
      if (pair("lazy", "industrious") || pair("cheerful", "gloomy") || pair("sickly", "hardy") || pair("honest", "brawler"))
        dup = true;
    }
    if (!dup) tr[k] = t;
  }
  // Skills start low; adults arrive with some experience.
  const u32 age = C.age_years(i, now);
  for (int k = 0; k < kSkillCount; ++k)
    C.skills[static_cast<size_t>(i) * kSkillCount + k] = static_cast<u8>(age >= 16 ? 10 + r.below(40) : 0);
}

void World::assign_goal(u32 i) {
  auto& C = citizens;
  rng::Stream& r = stream(rng::StreamId::Citizens);
  Goal g = Goal::None;
  switch (C.cls[i]) {
    case CitizenClass::Slave: g = Goal::Freedom; break;
    case CitizenClass::Patrician: g = Goal::Prestige; break;
    default: {
      const Goal opts[] = {Goal::BetterHome, Goal::Marry, Goal::Save, Goal::Children};
      g = opts[r.below(4)];
      if (g == Goal::Marry && C.spouse[i].valid()) g = Goal::Children;
      break;
    }
  }
  C.goal[i] = static_cast<u8>(g);
  C.goal_since[i] = static_cast<u16>(day_index(now));
  C.goal_progress[i] = 0;
  if (g == Goal::BetterHome && C.home[i].valid() && buildings.alive(C.home[i])) C.goal_progress[i] = buildings[C.home[i]].tier;
}

void World::move_to_household(u32 i, HouseholdId to) {
  auto& C = citizens;
  const HouseholdId from = C.household[i];
  if (from == to) return;
  if (from.valid() && households.ids.alive(from)) {
    Household& f = households[from];
    if (f.members > 0) --f.members;
    if (f.head == C.ids.id_at(i)) f.head = CitizenId{};
    if (f.shopper == C.ids.id_at(i)) f.shopper = CitizenId{};
    if (f.fetcher == C.ids.id_at(i)) f.fetcher = CitizenId{};
  }
  if (C.home[i].valid() && buildings.alive(C.home[i]) && buildings[C.home[i]].residents > 0) --buildings[C.home[i]].residents;
  C.household[i] = to;
  Household& t = households[to];
  ++t.members;
  if (!t.head.valid()) t.head = C.ids.id_at(i);
  C.home[i] = t.home;
  if (t.home.valid() && buildings.alive(t.home)) ++buildings[t.home].residents;
  // Empty households are dissolved (savings follow the mover).
  if (from.valid() && households.ids.alive(from) && households[from].members == 0) {
    t.savings += households[from].savings;
    households[from].savings = 0;
    // unlink from its house list
    Household& f = households[from];
    if (f.home.valid() && buildings.alive(f.home)) {
      u32* p = &buildings[f.home].first_household;
      while (*p != kNoIndex) {
        if (*p == from.index()) {
          *p = f.next_in_house;
          break;
        }
        p = &households.h[*p].next_in_house;
      }
    }
    households.ids.destroy(from);
  }
}

void World::kill_citizen(u32 i) {
  auto& C = citizens;
  ++deaths_today;
  ++deaths_total;
  const CitizenId id = C.ids.id_at(i);
  if (C.spouse[i].valid() && citizens.alive(C.spouse[i])) {
    const u32 s = C.spouse[i].index();
    C.spouse[s] = CitizenId{};
    C.remember(s, static_cast<u32>(day_index(now)), static_cast<u8>(Memory::Widowed), 0);
    C.bonus[s] = -15;
    C.bonus_until[s] = static_cast<u16>(day_index(now) + 6);
  }
  const HouseholdId hh = C.household[i];
  despawn_citizen(id);
  if (hh.valid() && households.ids.alive(hh) && households[hh].members == 0) {
    // No heirs: the estate passes to the city.
    earn(std::max(0, households[hh].savings), econ::Money::Other);
    Household& h = households[hh];
    if (h.home.valid() && buildings.alive(h.home)) {
      u32* p = &buildings[h.home].first_household;
      while (*p != kNoIndex) {
        if (*p == hh.index()) {
          *p = h.next_in_house;
          break;
        }
        p = &households.h[*p].next_in_house;
      }
    }
    households.ids.destroy(hh);
  }
}

void World::marry(u32 a, u32 b) {
  auto& C = citizens;
  C.spouse[a] = C.ids.id_at(b);
  C.spouse[b] = C.ids.id_at(a);
  const u32 day = static_cast<u32>(day_index(now));
  C.remember(a, day, static_cast<u8>(Memory::Married), 0);
  C.remember(b, day, static_cast<u8>(Memory::Married), 0);
  ++marriages_today;
  // The couple sets up a new household if a house has room; otherwise the wife joins his.
  const HouseholdId hh_a = C.household[a];
  const bool a_with_parents = hh_a.valid() && households.ids.alive(hh_a) && households[hh_a].members > 2;
  const u8 line = C.cls[a] == CitizenClass::Patrician || C.cls[a] == CitizenClass::Eques ? 1 : 0;
  BuildingId nh;
  if (a_with_parents && C.home[a].valid()) nh = find_vacant_house(buildings[C.home[a]].entrance, 2, line);
  if (nh.valid()) {
    HouseholdId hh = create_household(nh, C.cls[a]);
    if (hh) {
      households[hh].nomen = C.name1[a];
      households[hh].savings = 20;
      if (hh_a.valid() && households.ids.alive(hh_a)) households[hh_a].savings -= std::min(20, households[hh_a].savings);
      move_to_household(a, hh);
      move_to_household(b, hh);
      ++C.token[a];
      ++C.token[b];
      schedule_citizen(a, now + 1, CitizenEvent::Decide);
      schedule_citizen(b, now + 1, CitizenEvent::Decide);
      return;
    }
  }
  if (hh_a.valid() && households.ids.alive(hh_a)) {
    move_to_household(b, hh_a);
    ++C.token[b];
    schedule_citizen(b, now + 1, CitizenEvent::Decide);
  }
}

void World::give_birth(u32 m) {
  auto& C = citizens;
  const HouseholdId hh = C.household[m];
  if (!hh.valid() || !households.ids.alive(hh)) return;
  const BuildingId home = households[hh].home;
  CitizenId c = spawn_citizen(C.cls[m], home, hh, 0);
  if (!c) return;
  const u32 k = c.index();
  C.born[k] = static_cast<i64>(now);
  C.mother[k] = C.ids.id_at(m);
  C.father[k] = C.spouse[m];
  assign_traits(k, m, C.spouse[m].valid() ? C.spouse[m].index() : kNoIndex);
  C.goal[k] = 0;
  const u32 day = static_cast<u32>(day_index(now));
  C.remember(k, day, static_cast<u8>(Memory::Born), 0);
  C.remember(m, day, static_cast<u8>(Memory::Child), 0);
  C.bonus[m] = 10;
  C.bonus_until[m] = static_cast<u16>(day + 4);
  ++births_today;
  ++births_total;
}

void World::free_slave(u32 i) {
  auto& C = citizens;
  C.cls[i] = CitizenClass::Libertus;
  C.goal[i] = 0;
  const u32 day = static_cast<u32>(day_index(now));
  C.remember(i, day, static_cast<u8>(Memory::Freed), 0);
  C.bonus[i] = 25;
  C.bonus_until[i] = static_cast<u16>(day + 8);
  ++freed_today;
  // Freedmen set up their own household when there is room in plebeian housing.
  BuildingId h = find_vacant_house(C.home[i].valid() && buildings.alive(C.home[i]) ? buildings[C.home[i]].entrance : entry_tile, 1, 0);
  if (h.valid()) {
    HouseholdId hh = create_household(h, CitizenClass::Libertus);
    if (hh) {
      households[hh].savings = 25;
      move_to_household(i, hh);
      ++C.token[i];
      schedule_citizen(i, now + 1, CitizenEvent::Decide);
    }
  }
}

void World::buy_public_slaves(u32 count, bool paid) {
  BuildingDefId sq = defs->building("slave_quarters");
  for (u32 n = 0; n < count; ++n) {
    BuildingId q;
    buildings.for_each([&](BuildingId id, const Building& b) {
      if (!q.valid() && b.def == sq && b.state == BState::Active && b.residents < b.capacity) q = id;
    });
    if (!q.valid() || !ensure_entry_tile()) return;
    if (paid && !pay(120, econ::Money::Other)) return;
    // One city-slave household per quarters.
    HouseholdId hh;
    for (u32 hi = buildings[q].first_household; hi != kNoIndex; hi = households.h[hi].next_in_house)
      if (households.h[hi].city_slaves) hh = households.ids.id_at(hi);
    if (!hh) {
      hh = create_household(q, CitizenClass::Slave);
      if (!hh) return;
      households[hh].city_slaves = 1;
    }
    rng::Stream& r = stream(rng::StreamId::Citizens);
    CitizenId c = spawn_citizen(CitizenClass::Slave, q, hh, 16 + r.below(25), entry_tile);
    if (!c) return;
    assign_traits(c.index(), kNoIndex, kNoIndex);
    assign_goal(c.index());
  }
}

void World::lifecycle_daily(u32 i) {
  auto& C = citizens;
  rng::Stream& r = stream(rng::StreamId::Lifecycle);
  const u32 age = C.age_years(i, now);
  const u32 day = static_cast<u32>(day_index(now));

  // Death: rises steeply after 40; sickness and traits shift it.
  {
    const f32 L = trait_mul(i, &data::TraitDef::lifespan);
    f32 p = 0.00002f;
    if (age < 5) p += 0.0002f;
    if (age > 40) {
      const f32 x = (static_cast<f32>(age) / L - 40.0f) / 10.0f;
      if (x > 0) p += 0.0003f * x * x * x;
    }
    p *= 1.0f + static_cast<f32>(255 - C.health[i]) / 50.0f;
    if (r.chance(p)) {
      kill_citizen(i);
      return;
    }
  }

  // Life goals.
  if (age >= 16 && C.goal[i] == 0) assign_goal(i);
  if (C.goal[i] != 0) {
    bool done = false;
    const HouseholdId hh = C.household[i];
    switch (static_cast<Goal>(C.goal[i])) {
      case Goal::BetterHome:
        done = C.home[i].valid() && buildings.alive(C.home[i]) && buildings[C.home[i]].tier > C.goal_progress[i];
        break;
      case Goal::Marry: done = C.spouse[i].valid(); break;
      case Goal::Save: done = hh.valid() && households.ids.alive(hh) && households[hh].savings >= 200; break;
      case Goal::Children: done = hh.valid() && households.ids.alive(hh) && households[hh].members >= 4; break;
      case Goal::Freedom:
        if (C.cls[i] != CitizenClass::Slave) {
          done = true;
        } else {
          ++C.goal_progress[i];
          const u16 needed = policy.manumission == 2 ? 64 : 128;  // days of service (4 or 8 years)
          if (policy.manumission > 0 && C.goal_progress[i] >= needed &&
              r.chance(0.02f * trait_mul(i, &data::TraitDef::goal_rate))) {
            free_slave(i);
            return;
          }
        }
        break;
      case Goal::Prestige: done = C.home[i].valid() && buildings.alive(C.home[i]) && buildings[C.home[i]].tier >= 2; break;
      default: break;
    }
    if (done) {
      C.remember(i, day, static_cast<u8>(Memory::GoalAchieved), C.goal[i]);
      C.bonus[i] = 12;
      C.bonus_until[i] = static_cast<u16>(day + 4);
      C.goal[i] = 0;
    } else if (day - C.goal_since[i] > 48 && has_trait(i, "ambitious") && day % 8 == i % 8) {
      C.bonus[i] = -6;  // frustrated ambition
      C.bonus_until[i] = static_cast<u16>(day + 2);
    }
  }

  // Marriage: men initiate; search a small random sample for a compatible partner.
  if (C.sex[i] == 0 && !C.spouse[i].valid() && age >= 18 && age <= 45 && C.cls[i] != CitizenClass::Slave &&
      r.chance(0.012f)) {
    for (int tries = 0; tries < 40; ++tries) {
      const u32 j = r.below(std::max<u32>(1, C.ids.high_water()));
      if (!C.ids.alive_index(j) || C.sex[j] != 1 || C.spouse[j].valid() || (C.flags[j] & kFlagVisitor)) continue;
      const u32 aj = C.age_years(j, now);
      if (aj < 16 || aj > 40 || std::abs(static_cast<int>(aj) - static_cast<int>(age)) > 15) continue;
      const bool upper_i = C.cls[i] == CitizenClass::Patrician || C.cls[i] == CitizenClass::Eques;
      const bool upper_j = C.cls[j] == CitizenClass::Patrician || C.cls[j] == CitizenClass::Eques;
      if (upper_i != upper_j || C.cls[j] == CitizenClass::Slave) continue;
      if (C.household[j] == C.household[i]) continue;
      if (C.mother[j] == C.mother[i] && C.mother[i].valid()) continue;
      marry(i, j);
      break;
    }
  }

  // Births.
  if (C.sex[i] == 1 && C.spouse[i].valid() && age >= 16 && age <= 42 && C.home[i].valid() && buildings.alive(C.home[i])) {
    const Building& h = buildings[C.home[i]];
    const f32 room = h.residents < h.capacity + 2 ? 1.0f : 0.2f;
    const f32 mood = std::clamp(static_cast<f32>(C.happiness[i]) / 140.0f, 0.2f, 1.4f);
    const f32 juno = effect(Effect::Births) ? 1.5f : effect(Effect::Barren) ? 0.3f : 1.0f;
    if (r.chance(0.0075f * room * mood * juno * trait_mul(i, &data::TraitDef::fertility))) give_birth(i);
  }

  // Slaves: escapes under bad conditions.
  if (C.cls[i] == CitizenClass::Slave && C.happiness[i] < 60) {
    const f32 p = policy.slave_treatment == 0 ? 0.003f : policy.slave_treatment == 1 ? 0.0015f : 0.0005f;
    if (r.chance(p)) {
      ++emigrants_today;
      despawn_citizen(C.ids.id_at(i));
      return;
    }
  }

  // Crime: idle, poor, unhappy citizens may steal from a neighbour.
  if (age >= 14 && C.cls[i] != CitizenClass::Slave) {
    const HouseholdId hh = C.household[i];
    const bool poor = hh.valid() && households.ids.alive(hh) && households[hh].savings < 10;
    f32 p = 0.0004f * trait_mul(i, &data::TraitDef::crime);
    if (!C.work[i].valid()) p *= 3.0f;
    if (C.happiness[i] < 80) p *= 3.0f;
    if (poor) p *= 2.0f;
    ServiceId guard = defs->service("guard");
    const bool watched = guard.valid() && C.home[i].valid() && buildings.alive(C.home[i]) && services.ready(guard.v) &&
                         services.cost_ticks(guard.v, buildings[C.home[i]].entrance) <= kPatrolReachTicks;
    if (watched) p *= 0.3f;
    p *= static_cast<f32>(100 - crime_cut_) / 100.0f;
    if (r.chance(p)) {
      ++crimes_today;
      // Victim: a random household in a random house.
      const u32 hi = r.below(std::max<u32>(1, households.ids.high_water()));
      if (households.ids.alive_index(hi) && households.h[hi].savings > 0 && households.ids.id_at(hi) != hh) {
        const i32 stolen = std::min(households.h[hi].savings, 20);
        households.h[hi].savings -= stolen;
        if (hh.valid() && households.ids.alive(hh)) households[hh].savings += stolen;
        if (households.h[hi].head.valid() && citizens.alive(households.h[hi].head))
          C.remember(households.h[hi].head.index(), day, static_cast<u8>(Memory::Robbed), static_cast<u16>(stolen));
        if (households.h[hi].home.valid()) add_alert(AlertKind::Crime, households.h[hi].home);
      }
    }
  }

  // Private slaves: wealthy elite households buy domestic slaves.
  if ((C.cls[i] == CitizenClass::Patrician || C.cls[i] == CitizenClass::Eques) && C.household[i].valid() &&
      households.ids.alive(C.household[i]) && households[C.household[i]].head == C.ids.id_at(i)) {
    Household& h = households[C.household[i]];
    if (h.savings > 400 && h.home.valid() && buildings.alive(h.home) && buildings[h.home].residents < buildings[h.home].capacity &&
        r.chance(0.05f)) {
      h.savings -= 150;
      CitizenId s = spawn_citizen(CitizenClass::Slave, h.home, C.household[i], 16 + r.below(25));
      if (s) {
        assign_traits(s.index(), kNoIndex, kNoIndex);
        assign_goal(s.index());
      }
    }
  }
}

void World::riot_check() {
  // At most one riot per day: needs low morale plus a trigger (hunger or heavy taxes).
  if (avg_happiness >= 78 || population() < 100) return;
  u32 hungry = 0, n = 0;
  for (u32 k = 0; k < households.ids.high_water(); ++k)
    if (households.ids.alive_index(k)) {
      ++n;
      if (household_food(households.h[k]) == 0) ++hungry;
    }
  const bool trigger = (n > 0 && hungry * 100 / n > 25) || policy.tax_rate[2] > 15;
  rng::Stream& r = stream(rng::StreamId::Threats);
  if (!trigger || !r.chance(0.35f)) return;
  // The mob gathers at the most prominent civic building or market and damages it.
  BuildingId target;
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (target.valid() || b.state != BState::Active) return;
    const auto k = defs->b(b.def).kind;
    if (k == BuildingKind::Civic || k == BuildingKind::Market) target = id;
  });
  if (!target.valid()) return;
  ++riots_total;
  ServiceId guard = defs->service("guard");
  Building& tb = buildings[target];
  const bool suppressed = guard.valid() && services.ready(guard.v) && services.cost_ticks(guard.v, tb.entrance) <= kPatrolReachTicks;
  if (suppressed) {
    add_alert(AlertKind::Riot, target, 1);
    return;
  }
  tb.damage = static_cast<u8>(std::min(255, tb.damage + 90));
  const i64 loot = std::max<i64>(0, econ.treasury / 20);
  spend(loot, econ::Money::Other);
  add_alert(AlertKind::Riot, target, 0);
}

}  // namespace urbs::sim
