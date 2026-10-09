// Ratings, favor with Rome, requests, loans and debt, mission goals, unlocks, narrative messages.
#include <algorithm>

#include "core/alloc_track.hpp"
#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

void World::change_favor(i32 delta) { favor = std::clamp(favor + delta, 0, 100); }

bool World::research(u16 t) {
  if (t >= defs->techniques.size() || tech_known[t] || scientia < defs->techniques[t].cost) return false;
  scientia -= defs->techniques[t].cost;
  tech_known[t] = 1;
  static const char* kIds[] = {"crop_rotation", "water_mills", "hypocaust", "roman_concrete", "arches", "insula_codes",
                               "legion_drill", "medicine", "cursus", "latin_schools"};
  for (int k = 0; k < static_cast<int>(Tech::Count); ++k)
    if (defs->techniques[t].id == kIds[k]) tech_flags[k] = 1;
  return true;
}

u32 World::count_active(BuildingDefId d) const {
  u32 n = 0;
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.def == d && b.state == BState::Active) ++n;
  });
  return n;
}

void World::unlock_up_to(u8 m) {
  mission_number = m;
  unlocked.assign(defs->buildings.size(), 0);
  for (size_t k = 0; k < defs->buildings.size(); ++k) unlocked[k] = defs->buildings[k].unlock <= m ? 1 : 0;
}

void World::compute_ratings() {
  // Culture: share of housing covered by worship, entertainment and education, weighted.
  u32 houses = 0, rel = 0, ent = 0, edu = 0, tier_sum = 0, tier_max = 0;
  auto covered = [&](const Building& b, data::NeedKind k) {
    for (ServiceId s : need_services[static_cast<int>(k)])
      if (services.ready(s.v) && services.cost_ticks(s.v, b.entrance) <= defs->services[s.v].range) return true;
    return false;
  };
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state != BState::Active || b.residents == 0) return;
    const auto& d = defs->b(b.def);
    if (d.kind != BuildingKind::House) return;
    houses += b.residents;
    if (covered(b, data::NeedKind::Religion)) rel += b.residents;
    if (covered(b, data::NeedKind::Entertainment)) ent += b.residents;
    if (covered(b, data::NeedKind::Education)) edu += b.residents;
    tier_sum += static_cast<u32>(b.tier + d.house_line * 2) * b.residents;
    tier_max = std::max<u32>(tier_max, defs->max_tier(d.house_line) + d.house_line * 2);
  });
  const u32 hp = std::max<u32>(1, houses);
  ratings.culture = static_cast<u8>(std::min<u32>(100, (rel * 40 + ent * 35 + edu * 25) / hp));
  // Prosperity: housing quality, employment, treasury health.
  u32 adults = 0, employed = 0;
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i) || (citizens.flags[i] & kFlagVisitor)) continue;
    const u32 a = citizens.age_years(i, now);
    if (a < 14 || a >= 60 || citizens.cls[i] == CitizenClass::Patrician) continue;
    ++adults;
    if (citizens.work[i].valid()) ++employed;
  }
  const i32 housing = static_cast<i32>(tier_sum * 100 / (hp * std::max<u32>(1, tier_max)));
  const i32 employ = adults ? static_cast<i32>(employed * 100 / adults) : 0;
  const i32 money = econ.treasury >= 0 ? std::min<i32>(100, static_cast<i32>(econ.treasury / std::max<i64>(1, population() / 2 + 1)))
                                       : -50;
  ratings.prosperity = static_cast<u8>(std::clamp(housing * 55 / 100 + employ * 30 / 100 + money * 15 / 100, 0, 100));
  // Security: crime, recent fires/collapses, defence against raids.
  const i32 crime = static_cast<i32>(std::min<u32>(40, crimes_yday * 4000 / std::max<u32>(1, population())));
  u32 ruins = 0;
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state == BState::Ruin || b.state == BState::Burning) ++ruins;
  });
  u32 soldiers = 0, enemies = 0;
  for (const auto& u : army.units)
    if (u.alive) (u.side == 0 ? soldiers : enemies) += u.count;
  i32 sec = 80 - crime - static_cast<i32>(std::min<u32>(30, ruins * 3)) - static_cast<i32>(std::min<u32>(40, enemies / 10));
  sec += static_cast<i32>(std::min<u32>(20, soldiers * 200 / std::max<u32>(1, population())));
  ratings.security = static_cast<u8>(std::clamp(sec, 0, 100));
  ratings.favor = static_cast<u8>(favor);
}

u16 World::add_request(u8 kind, GoodId good, u16 amount, u16 days, i32 reward, i8 fw, i8 fl) {
  for (auto& r : requests)
    if (!r.active) {
      r = Request{1, kind, good, amount, now + static_cast<Tick>(days) * kTicksPerDay, reward, fw, fl, next_request_id++};
      add_alert(AlertKind::Request, BuildingId{}, r.id);
      return r.id;
    }
  return 0;
}

void World::set_difficulty(u8 d) {
  const u8 old = difficulty;
  difficulty = std::min<u8>(d, 2);
  if (now == 0 && old != difficulty) {
    // Undo the previous scaling, then apply the new one (Otium x1.5, Imperium x0.75).
    auto scale = [](u8 k) { return k == 0 ? 150 : k == 2 ? 75 : 100; };
    econ.treasury = econ.treasury * 100 / scale(old) * scale(difficulty) / 100;
  }
}

u32 World::stored_amount(GoodId g) const {
  u32 n = 0;
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.inv == econ::kNoInv || b.state != BState::Active) return;
    const auto k = defs->b(b.def).kind;
    if (k == BuildingKind::Storage || k == BuildingKind::TradePost) n += econ.available(b.inv, g);
  });
  return n;
}

bool World::fulfill_request(u16 id) {
  for (auto& r : requests) {
    if (!r.active || r.id != id) continue;
    if (r.kind == 0) {
      // Goods leave from storage buildings.
      if (stored_amount(r.good) < r.amount) return false;
      u32 left = r.amount;
      buildings.for_each([&](BuildingId, const Building& b) {
        if (left == 0 || b.inv == econ::kNoInv || b.state != BState::Active) return;
        const auto k = defs->b(b.def).kind;
        if (k != BuildingKind::Storage && k != BuildingKind::TradePost) return;
        left -= econ.take(b.inv, r.good, static_cast<u16>(std::min<u32>(left, econ.available(b.inv, r.good))));
      });
      if (left > 0) return false;
      econ.city_stock[r.good.v] -= std::min<u32>(econ.city_stock[r.good.v], r.amount);
    } else if (r.kind == 1) {
      if (!pay(r.amount, econ::Money::Other)) return false;
    } else {
      // Troops: a city unit with enough soldiers leaves for Rome.
      u16 best = military::kNoUnit;
      for (u16 u = 0; u < military::kMaxUnits; ++u)
        if (army.units[u].alive && army.units[u].side == 0 && army.units[u].count >= r.amount / 2 &&
            army.units[u].order != military::Order::Away)
          best = u;
      if (best == military::kNoUnit) return false;
      disband_unit(best);
    }
    r.active = 0;
    ++requests_fulfilled;
    change_favor(r.favor_win);
    earn(r.reward, econ::Money::Tribute);
    if (script) {
      core::alloc::AllowScope lua;  // scripts run only at event points
      script->on_request(r.id, true);
    }
    return true;
  }
  return false;
}

void World::take_loan(i64 amount) {
  if (amount <= 0 || loan.remaining > 0) return;
  econ.treasury += amount;
  econ.money.add(econ::Money::Tribute, amount);
  loan.remaining = amount * 6 / 5;               // 20% interest
  loan.installment = loan.remaining / 32 + 1;   // repaid over two years
  change_favor(-5);
}

u16 World::post_message(u8 advisor, const std::string& title, const std::string& text,
                        const std::vector<std::string>& options, const std::string& callback) {
  core::alloc::AllowScope narrative;  // messages are rare, script-driven
  Message m;
  m.id = next_message_id++;
  m.t = now;
  m.advisor = advisor;
  m.title = title;
  m.text = text;
  m.options = options;
  m.callback = callback;
  if (messages.size() >= 64) messages.erase(messages.begin());
  messages.push_back(std::move(m));
  return messages.back().id;
}

void World::answer(u16 id, i32 option) {
  for (auto& m : messages) {
    if (m.id != id || m.answered) continue;
    m.answered = static_cast<u8>(std::max(1, option));
    if (script && !m.callback.empty()) {
      core::alloc::AllowScope lua;
      script->on_choice(m.callback, option);
    }
    return;
  }
}

bool World::goals_met() const {
  if (population() < goals.population) return false;
  if (ratings.culture < goals.culture || ratings.prosperity < goals.prosperity || ratings.security < goals.security ||
      ratings.favor < goals.favor)
    return false;
  if (requests_fulfilled < goals.requests) return false;
  if (mission_days < goals.survive_days) return false;
  for (const auto& g : goals.buildings)
    if (count_active(g.def) < g.count) return false;
  return true;
}

void World::governance_daily() {
  ++mission_days;
  // Scholars produce scientia: staff of schools, libraries and academies, plus readers.
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state != BState::Active) return;
    const auto& d = defs->b(b.def);
    if (!d.service.valid() || defs->services[d.service.v].need != data::NeedKind::Education) return;
    const u32 w = d.id == "school" ? 1 : d.id == "library" ? 2 : 3;
    scientia += w * b.employed + b.served_today / 10;
  });
  compute_ratings();
  // Favor drifts with the elite's mood and the city's reputation.
  u32 elite = 0, elite_happy = 0;
  for (u32 i = 0; i < citizens.ids.high_water(); ++i)
    if (citizens.ids.alive_index(i) && citizens.cls[i] == CitizenClass::Patrician) {
      ++elite;
      elite_happy += citizens.happiness[i];
    }
  if (elite > 0 && day_index(now) % 4 == 0) change_favor(elite_happy / elite >= 140 ? 1 : elite_happy / elite < 90 ? -1 : 0);
  if (effect(Effect::Favor) && day_index(now) % 2 == 0) change_favor(1);
  // Requests past their deadline fail.
  for (auto& r : requests) {
    if (!r.active || now < r.deadline) continue;
    r.active = 0;
    ++requests_failed;
    // Rome is more forgiving on Otium and less on Imperium.
    change_favor(static_cast<i32>(static_cast<f32>(r.favor_lose) * threat_scale()));
    add_alert(AlertKind::Request, BuildingId{}, static_cast<u16>(0x8000 | r.id));
    if (script) {
      core::alloc::AllowScope lua;
      script->on_request(r.id, false);
    }
  }
  // Loans are repaid daily; long debt angers Rome.
  if (loan.remaining > 0) {
    const i64 pay_now = std::min(loan.remaining, loan.installment);
    spend(pay_now, econ::Money::Other);
    loan.remaining -= pay_now;
  }
  if (econ.treasury < 0) {
    ++debt_days;
    if (debt_days % 4 == 0) change_favor(-2);
  } else {
    debt_days = 0;
  }
  ratings.favor = static_cast<u8>(favor);
  if (script) {
    core::alloc::AllowScope lua;
    script->on_day();
  }
  // Mission outcome.
  if (mission != MissionState::Running) return;
  if (goals.population > 0 && goals_met()) {
    mission = MissionState::Won;
    return;
  }
  if (difficulty > 0) {
    core::alloc::AllowScope once;  // the mission ends at most once; setting the reason may allocate
    if (goals.fail_on_favor && favor <= 0) {
      mission = MissionState::Lost;
      lost_reason = "Rome has lost all confidence in you.";
    } else if (difficulty == 2 && debt_days > goals.max_debt_days) {
      mission = MissionState::Lost;
      lost_reason = "The city has been in debt for too long.";
    }
  }
}

}  // namespace urbs::sim
