// Society: housing evolution, immigration/emigration, happiness, alerts, names.
#include <algorithm>
#include <string>

#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;
using data::HousingReq;

Sector World::sector_of(const Building& b) const {
  if (b.state == BState::Construction) return Sector::Construction;
  const auto& d = defs->b(b.def);
  switch (d.kind) {
    case BuildingKind::Farm: return Sector::Food;
    case BuildingKind::Storage:
    case BuildingKind::Market:
    case BuildingKind::TradePost: return Sector::Distribution;
    case BuildingKind::Service:
    case BuildingKind::Patrol:
    case BuildingKind::Water: return Sector::Services;
    case BuildingKind::Civic:
    case BuildingKind::Monument: return Sector::Government;
    case BuildingKind::Military: return Sector::Military;
    default: return d.category == "food" ? Sector::Food : d.category == "military" ? Sector::Military : Sector::Industry;
  }
}

void World::add_alert(AlertKind k, BuildingId b, u16 arg) {
  for (auto it = alerts.rbegin(); it != alerts.rend(); ++it) {
    if (now - it->t > kTicksPerDay) break;
    if (it->kind == k && it->b == b) {
      it->t = now;
      it->arg = arg;
      ++it->count;
      std::rotate(it.base() - 1, it.base(), alerts.end());  // now the newest: the feed stays in time order
      return;
    }
  }
  if (alerts.size() >= alerts.capacity() || alerts.size() >= 128) alerts.erase(alerts.begin());
  alerts.push_back(Alert{now, k, arg, b, 1});
}

u8 World::food_kinds(const Household& h) const {
  u8 n = 0;
  for (u16 v : h.fstock)
    if (v > 0) ++n;
  if (n == 0 && h.food > 0) n = 1;
  return n;
}

bool World::requirement_met(const Building& b, const HousingReq& r) const {
  switch (r.kind) {
    case HousingReq::Service:
      for (ServiceId s : r.services)
        if (services.ready(s.v) && services.cost_ticks(s.v, b.entrance) <= defs->services[s.v].range) return true;
      return false;
    case HousingReq::Patrol:
      for (ServiceId s : r.services)
        if (services.ready(s.v) && services.cost_ticks(s.v, b.entrance) <= kPatrolReachTicks) return true;
      return false;
    case HousingReq::FoodKinds: {
      u32 sum = 0, n = 0;
      for (u32 hi = b.first_household; hi != kNoIndex; hi = households.h[hi].next_in_house) {
        sum += food_kinds(households.h[hi]);
        ++n;
      }
      return n > 0 && static_cast<i32>(sum / n) >= r.value;
    }
    case HousingReq::Food:
    case HousingReq::Goods:
      for (GoodId g : r.goods) {
        bool found = false;
        const i8 fs = good_food_slot[g.v], gs = good_hh_slot[g.v];
        for (u32 hi = b.first_household; hi != kNoIndex && !found; hi = households.h[hi].next_in_house) {
          const Household& hh = households.h[hi];
          if (fs >= 0 && hh.fstock[fs] > 0) found = true;
          if (gs >= 0 && hh.goods[gs] > 0) found = true;
        }
        if (!found) return false;
      }
      return true;
    case HousingReq::Desirability: return static_cast<i32>(b.desirability_seen) - 128 >= r.value;
    case HousingReq::Sewer: return (b.flags & 0x10) != 0;
    case HousingReq::PipedWater: return (b.flags & 0x20) != 0;
  }
  return false;
}

int World::first_unmet(const Building& b, u8 tier) const {
  const auto* t = defs->tier(defs->b(b.def).house_line, tier);
  if (!t) return 0;
  for (size_t k = 0; k < t->reqs.size(); ++k)
    if (!requirement_met(b, t->reqs[k])) return static_cast<int>(k);
  return -1;
}

BuildingId World::find_vacant_house(i32 near, u16 space, u8 line) const {
  BuildingId best;
  int best_d = 1 << 30;
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.state != BState::Active || b.entrance < 0) return;
    const auto& d = defs->b(b.def);
    if (d.kind != BuildingKind::House || d.house_line != line) return;
    if (b.capacity < b.residents + space) return;
    if (near >= 0 && !nav.connected(near, b.entrance)) return;
    int dd = near >= 0 ? std::abs(near % map.w - b.entrance % map.w) + std::abs(near / map.w - b.entrance / map.w) : 0;
    // Prefer filling partly occupied houses first, then distance.
    if (b.residents > 0) dd -= 30;
    if (dd < best_d) {
      best_d = dd;
      best = id;
    }
  });
  return best;
}

namespace {
void unlink_household(World& w, HouseholdId id) {
  Household& h = w.households[id];
  if (h.home.valid() && w.buildings.alive(h.home)) {
    Building& b = w.buildings[h.home];
    u32* p = &b.first_household;
    while (*p != kNoIndex) {
      if (*p == id.index()) {
        *p = h.next_in_house;
        break;
      }
      p = &w.households.h[*p].next_in_house;
    }
  }
  h.home = BuildingId{};
  h.next_in_house = kNoIndex;
}
}  // namespace

void World::start_emigration(HouseholdId id) {
  Household& h = households[id];
  if (h.leaving) return;
  h.leaving = 1;
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i) || citizens.household[i] != id) continue;
    if (citizens.work[i].valid()) release_worker(i);
    if (citizens.activity[i] == Activity::Walk) continue;  // will re-decide on arrival
    ++citizens.token[i];
    schedule_citizen(i, now + 1 + (i % 20), CitizenEvent::Decide);
  }
}

void World::finish_leave(u32 i) {
  auto& C = citizens;
  if (C.flags[i] & kFlagVisitor) {
    if (visitors > 0) --visitors;
    despawn_citizen(C.ids.id_at(i));
    return;
  }
  HouseholdId hh = C.household[i];
  ++emigrants_today;
  ++emigrants_total;
  despawn_citizen(C.ids.id_at(i));
  if (hh.valid() && households.ids.alive(hh) && households[hh].members == 0) {
    unlink_household(*this, hh);
    households.ids.destroy(hh);
  }
}

void World::house_daily(BuildingId id) {
  Building& b = buildings[id];
  const auto& d = defs->b(b.def);
  if (b.state != BState::Active || d.kind != BuildingKind::House) return;
  disease_daily(id);
  if (d.house_line > 1) return;
  const u8 line = d.house_line;
  // Household mood: emigration of persistently unhappy households.
  for (u32 hi = b.first_household; hi != kNoIndex;) {
    Household& h = households.h[hi];
    const u32 next = h.next_in_house;
    // Taxes. The tributum (10% a day of savings above 20) is levied everywhere, so hoarded money returns
    // to the treasury and wages balance against taxes + sales. The wage tax needs a forum's clerks.
    {
      ServiceId forum = defs->service("forum");
      const bool clerks = forum.valid() && services.ready(forum.v) &&
                          services.cost_ticks(forum.v, b.entrance) <= defs->services[forum.v].range;
      const i32 wealth = h.savings > 20 ? (h.savings - 20) / 10 : 0;
      const i32 wage_tax = clerks ? (h.income_today * policy.tax_rate[static_cast<int>(h.cls) % 6] + 50) / 100 : 0;
      const i32 tax = std::min<i32>(h.savings, wage_tax + wealth);
      h.savings -= tax;
      earn(tax, econ::Money::Taxes);
      h.income_today = 0;
    }
    // Household goods wear out: one unit of each held good every eight days.
    if ((day_index(now) + hi) % 8 == 0)
      for (size_t k = 0; k < hh_goods.size() && k < 8; ++k)
        if (h.goods[k] > 0) {
          --h.goods[k];
          ++econ.consumed_today[hh_goods[k].v];
        }
    const u32 sum = h.happy_sum, n = h.happy_n;
    h.happy_sum = 0;
    h.happy_n = 0;
    if (n > 0 && sum / n < 55) {
      if (++h.unhappy_days >= 4 && stream(rng::StreamId::Immigration).chance(0.25f)) start_emigration(households.ids.id_at(hi));
    } else {
      h.unhappy_days = 0;
    }
    hi = next;
  }
  b.desirability_seen = static_cast<u8>(std::clamp(128 + building_desirability(b), 0, 255));
  if (b.residents == 0) {
    b.tier = 0;
    b.extra = 0;
    if (const auto* t = defs->tier(line, 0)) b.capacity = std::max(b.capacity, t->capacity);
    return;
  }
  u8 t = b.tier;
  // Devolve after three consecutive failed checks of the current tier (a day's hiccup is forgiven).
  const int unmet_cur = t > 0 ? first_unmet(b, t) : -1;
  u8 counter = b.flags & 0x3;
  if (unmet_cur >= 0) {
    counter = static_cast<u8>(std::min(3, counter + 1));
    if (counter >= 3) {
      --t;
      counter = 0;
      add_alert(AlertKind::HouseDevolved, id, static_cast<u16>(unmet_cur));
    }
  } else {
    counter = 0;
  }
  b.flags = static_cast<u8>((b.flags & ~0x3) | counter);
  const u8 cap_tier = std::min<u8>(defs->max_tier(line), line == 0 ? policy.max_insula_tier : 255);
  b.extra = 0;
  if (t == b.tier && t < cap_tier) {
    const int un = first_unmet(b, static_cast<u8>(t + 1));
    if (un < 0) ++t;
    else b.extra = static_cast<u32>(un + 1);  // 1-based index of the first unmet requirement of the next tier
  }
  b.tier = t;
  if (const auto* td = defs->tier(line, t)) {
    // Footprint scaling: tier capacity is per base footprint.
    b.capacity = td->capacity;
  }
  // Overcrowded after devolving: the newest households must find another home.
  while (b.residents > b.capacity && b.first_household != kNoIndex) {
    HouseholdId hid = households.ids.id_at(b.first_household);
    Household& h = households[hid];
    for (u32 i = 0; i < citizens.ids.high_water(); ++i)
      if (citizens.ids.alive_index(i) && citizens.household[i] == hid) {
        citizens.home[i] = BuildingId{};
        if (b.residents > 0) --b.residents;
        ++citizens.token[i];
        schedule_citizen(i, now + 1 + (i % 30), CitizenEvent::Decide);
      }
    unlink_household(*this, hid);
    (void)h;
  }
}

World::HappinessParts World::happiness_parts(u32 i) const {
  const auto& C = citizens;
  HappinessParts p;
  // 150 is "fine": below it the need hurts (down to `lo` at 0), above it helps a little (up to `hi`).
  auto lin = [](u8 v, int lo, int hi) {
    const int x = static_cast<int>(v);
    return static_cast<i16>(x >= 150 ? hi * (x - 150) / 105 : lo * (150 - x) / 150);
  };
  p.food = lin(C.need(i, Need::Food, now), -40, 10);
  p.water = lin(C.need(i, Need::Water, now), -30, 6);
  p.rest = lin(C.need(i, Need::Rest, now), -15, 5);
  p.hygiene = lin(C.need(i, Need::Hygiene, now), -12, 6);
  p.religion = lin(C.need(i, Need::Religion, now), -12, 6);
  p.entertainment = lin(C.need(i, Need::Entertainment, now), -10, 8);
  if (C.household[i].valid() && households.ids.alive(C.household[i])) {
    const u8 k = food_kinds(households[C.household[i]]);
    p.variety = static_cast<i16>(k == 0 ? -10 : k == 1 ? 0 : k == 2 ? 5 : 10);
  }
  if (C.home[i].valid() && buildings.alive(C.home[i])) p.housing = static_cast<i16>(buildings[C.home[i]].tier * 4);
  else p.housing = -25;
  const u32 age = C.age_years(i, now);
  if (age >= 14 && age < 60 && !C.work[i].valid() && C.cls[i] != CitizenClass::Patrician) p.employment = -8;
  // Workers compare the city's wage with the going rate in Rome (4 denarii a day).
  if (C.work[i].valid() && C.cls[i] != CitizenClass::Slave)
    p.employment = static_cast<i16>(std::clamp((static_cast<int>(policy.wage) - 4) * 3, -12, 9));
  p.taxes = static_cast<i16>(-static_cast<int>(policy.tax_rate[static_cast<int>(C.cls[i]) % 6]) / 2);
  if (C.bonus_until[i] > day_index(now)) p.events = C.bonus[i];
  {
    u32 sum = 0, n = 0;
    for (size_t g = 0; g < god_mood.size(); ++g)
      if (god_available[g]) {
        sum += god_mood[g];
        ++n;
      }
    if (n) p.gods = static_cast<i16>((static_cast<i32>(sum / n) - 50) / 5);
    if (effect(Effect::Happy)) p.gods = static_cast<i16>(p.gods + 6);
    if (effect(Effect::Discontent)) p.gods = static_cast<i16>(p.gods - 8);
  }
  p.safety = static_cast<i16>(-std::min<u32>(12, crimes_yday * 2000 / std::max<u32>(1, population())));
  if (C.cls[i] == CitizenClass::Slave) p.freedom = static_cast<i16>(policy.slave_treatment == 0 ? -25 : policy.slave_treatment == 1 ? -12 : -4);
  for (int k = 0; k < kMaxTraits; ++k) {
    const u8 t = C.traits[static_cast<size_t>(i) * kMaxTraits + k];
    if (t < defs->traits.size()) p.traits = static_cast<i16>(p.traits + defs->traits[t].happiness);
  }
  p.health = static_cast<i16>((static_cast<int>(C.health[i]) - 255) / 14);
  if (C.cls[i] == CitizenClass::Patrician || C.cls[i] == CitizenClass::Eques) {
    p.education = lin(C.need(i, Need::Education, now), -12, 6);
    // Prestige: the elite care about how desirable their neighbourhood is.
    if (C.home[i].valid() && buildings.alive(C.home[i]))
      p.prestige = static_cast<i16>(std::clamp((static_cast<int>(buildings[C.home[i]].desirability_seen) - 128) / 2, -15, 15));
  }
  return p;
}

void World::citizen_daily(u32 i) {
  if (citizens.flags[i] & kFlagVisitor) return;
  lifecycle_daily(i);
  if (!citizens.ids.alive_index(i)) return;
  // Wages (paid by the city) and taxes (collected where a forum covers the home).
  Household* hhp = citizens.household[i].valid() && households.ids.alive(citizens.household[i]) ? &households[citizens.household[i]] : nullptr;
  const bool worked = (citizens.flags[i] & kFlagWorked) != 0;
  citizens.flags[i] &= static_cast<u8>(~kFlagWorked);
  if (hhp && !hhp->city_slaves && worked && citizens.work[i].valid() && buildings.alive(citizens.work[i])) {
    const i32 wage = policy.wage;
    spend(wage, econ::Money::Wages);
    hhp->savings += wage;
    hhp->income_today = static_cast<u16>(std::min<i32>(60000, hhp->income_today + wage));
  }
  // Homeless and unhappy for four days: the family gives up on the city (houses check their own residents).
  if (hhp && !hhp->leaving && hhp->head == citizens.ids.id_at(i) && !citizens.home[i].valid()) {
    hhp->unhappy_days = citizens.happiness[i] < 70 ? static_cast<u8>(std::min(255, hhp->unhappy_days + 1)) : 0;
    if (hhp->unhappy_days >= 4 && stream(rng::StreamId::Immigration).chance(0.3f)) {
      start_emigration(citizens.household[i]);
      return;
    }
  }
  // Patricians live off their estates outside the city: income enters the household, not the treasury.
  if (hhp && citizens.cls[i] == CitizenClass::Patrician && citizens.age_years(i, now) >= 16) {
    hhp->savings += 12;
    hhp->income_today = static_cast<u16>(std::min<i32>(60000, hhp->income_today + 12));
  }
  // Natural recovery when not in a plague house.
  if (citizens.health[i] < 255 && !(citizens.home[i].valid() && buildings.alive(citizens.home[i]) &&
                                    buildings[citizens.home[i]].plague > 0))
    citizens.health[i] = static_cast<u8>(std::min(255, citizens.health[i] + 6));
  const i32 h = std::clamp(happiness_parts(i).total(), 0, 255);
  citizens.happiness[i] = static_cast<u8>(h);
  happy_sum_today += h;
  ++happy_count_today;
  if (citizens.household[i].valid() && households.ids.alive(citizens.household[i])) {
    Household& hh = households[citizens.household[i]];
    hh.happy_sum += static_cast<u32>(h);
    ++hh.happy_n;
  }
}

void World::immigration_hourly() {
  if (!policy.immigration_open) return;
  const u32 hour = hour_of_day(now);
  if (hour < 6 || hour > 19) return;
  u32 free = 0, pop = population(), jobs_free = 0, unemployed = 0;
  BuildingId first_house;
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.state != BState::Active) return;
    const auto& d = defs->b(b.def);
    if (d.kind == BuildingKind::House && d.house_line == 0) {
      if (b.capacity > b.residents) free += b.capacity - b.residents;
      if (!first_house.valid()) first_house = id;
    }
    if (b.jobs > b.employed) jobs_free += b.jobs - b.employed;
  });
  for (u32 i = 0; i < citizens.ids.high_water(); ++i)
    if (citizens.ids.alive_index(i) && !(citizens.flags[i] & kFlagVisitor) && !citizens.work[i].valid() && citizens.age_years(i, now) >= 14 &&
        citizens.age_years(i, now) < 60)
      ++unemployed;
  u32 food_stock = 0;
  for (GoodId g : food_slots) food_stock += econ.city_stock[g.v];
  const u32 p = std::max<u32>(1, pop);
  i32 a = 20;
  a += static_cast<i32>(std::min<u32>(30, food_stock * 10 / p));
  if (pop > 20 && food_stock == 0) a -= 15;
  a += static_cast<i32>(std::min<u32>(30, jobs_free * 100 / p));
  a -= static_cast<i32>(std::min<u32>(40, unemployed * 100 / p));
  a += (static_cast<i32>(avg_happiness) - 128) / 2;
  appeal = std::clamp(a, -100, 100);
  if (appeal <= 0 || free < 3 || !first_house.valid()) return;
  if (!ensure_entry_tile()) return;
  const u32 arrived_before = immigrants_today;
  rng::Stream& r = stream(rng::StreamId::Immigration);
  // Expected households per hour: 0.2..1.5 by appeal, growing with the city; fractional via chance.
  const f32 rate = std::clamp(static_cast<f32>(appeal) / 60.0f, 0.2f, 1.5f) * (1.0f + static_cast<f32>(pop) / 2000.0f);
  int n = static_cast<int>(rate);
  if (r.chance(rate - static_cast<f32>(n))) ++n;
  for (int k = 0; k < n; ++k) {
    const u16 size = static_cast<u16>(2 + r.below(4));
    BuildingId house = find_vacant_house(entry_tile, size);
    if (!house.valid()) break;
    HouseholdId hh = create_household(house, CitizenClass::Pleb);
    if (!hh) break;
    households[hh].food = static_cast<u16>(size * 2);
    households[hh].savings = static_cast<i32>(size) * 15;
    households[hh].water = size;
    for (u16 m = 0; m < size; ++m) {
      const u32 age = m < 2 ? 18 + r.below(25) : r.below(16);
      CitizenId c = spawn_citizen(CitizenClass::Pleb, house, hh, age, entry_tile);
      if (!c) break;
      citizens.reason[c.index()] = Reason::Immigrating;
      ++immigrants_today;
      ++immigrants_total;
    }
  }
  // The elite arrive when there is a vacant domus and the city is pleasant enough.
  if (avg_happiness >= 115 && pop >= 150 && r.chance(0.5f)) {
    BuildingId domus = find_vacant_house(entry_tile, 3, 1);
    if (domus.valid()) {
      const CitizenClass cls = r.chance(0.5f) ? CitizenClass::Patrician : CitizenClass::Eques;
      HouseholdId hh = create_household(domus, cls);
      if (hh) {
        households[hh].savings = cls == CitizenClass::Patrician ? 600 : 300;
        households[hh].food = 12;
        households[hh].water = 6;
        const u16 size = static_cast<u16>(2 + r.below(2));
        for (u16 m = 0; m < size; ++m) {
          CitizenId c = spawn_citizen(cls, domus, hh, m < 2 ? 25 + r.below(30) : r.below(14), entry_tile);
          if (!c) break;
          citizens.reason[c.index()] = Reason::Immigrating;
          ++immigrants_today;
          ++immigrants_total;
        }
      }
    }
  }
  if (immigrants_today > arrived_before)  // a household that found no house is no arrival
    add_alert(AlertKind::Immigrants, BuildingId{}, static_cast<u16>(immigrants_today));
}

bool World::ensure_entry_tile() {
  // Keep the current entry while any house can be reached from it; otherwise find a new one for the first house.
  BuildingId first_house;
  bool reachable = false;
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.state != BState::Active || defs->b(b.def).kind != BuildingKind::House || b.entrance < 0) return;
    if (!first_house.valid()) first_house = id;
    if (!reachable && entry_tile >= 0 && nav.connected(entry_tile, b.entrance)) reachable = true;
  });
  if (!first_house.valid() || reachable) return entry_tile >= 0;
  const i32 he = buildings[first_house].entrance;
  // Walkable map-edge tile closest to the first house, in the same region; roads preferred.
  int best_d = 1 << 30;
  entry_tile = -1;
  for (i32 k = 0; k < map.w * 2 + map.h * 2; ++k) {
    i32 x, y;
    if (k < map.w) x = k, y = 0;
    else if (k < map.w * 2) x = k - map.w, y = map.h - 1;
    else if (k < map.w * 2 + map.h) x = 0, y = k - map.w * 2;
    else x = map.w - 1, y = k - map.w * 2 - map.h;
    const i32 t = map.idx(x, y);
    if (!nav.connected(t, he)) continue;
    const int dd = std::abs(x - he % map.w) + std::abs(y - he / map.w) - (map.road[static_cast<size_t>(t)] != Road::None ? 1000 : 0);
    if (dd < best_d) {
      best_d = dd;
      entry_tile = t;
    }
  }
  return entry_tile >= 0;
}

std::string World::citizen_name(u32 i) const {
  const auto& N = defs->names;
  const auto& C = citizens;
  if (C.cls[i] == CitizenClass::Slave) return N.slave[C.name0[i] % N.slave.size()];
  std::string nomen = N.nomina[C.name1[i] % N.nomina.size()];
  std::string cog = N.cognomina[C.name2[i] % N.cognomina.size()];
  if (C.sex[i] == 1) {
    auto fem = [](std::string s) {
      if (s.size() > 2 && s.substr(s.size() - 2) == "us") s = s.substr(0, s.size() - 2) + "a";
      return s;
    };
    return fem(nomen) + " " + fem(cog);
  }
  return N.praenomina[C.name0[i] % N.praenomina.size()] + " " + nomen + " " + cog;
}

}  // namespace urbs::sim
