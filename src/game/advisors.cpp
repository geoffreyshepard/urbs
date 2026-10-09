#include "game/advisors.hpp"

#include <algorithm>
#include <map>

#include "game/panels.hpp"

namespace urbs::game {

using data::BuildingKind;

std::vector<std::string> advisor_report(const sim::World& w, int a) {
  std::vector<std::string> out;
  const auto& D = *w.defs;
  switch (a) {
    case 1: {  // engineer: housing, water, hazards
      u32 tiers[8] = {}, houses = 0, damaged = 0, risky = 0;
      std::map<std::string, u32> blockers;
      w.buildings.for_each([&](BuildingId, const sim::Building& b) {
        const auto& d = D.b(b.def);
        if (b.state == sim::BState::Active && b.damage > 150) ++damaged;
        if (b.state == sim::BState::Active && b.fire_risk > 150) ++risky;
        if (d.kind != BuildingKind::House || b.residents == 0) return;
        ++houses;
        ++tiers[std::min<int>(b.tier, 7)];
        if (const auto* nt = D.tier(d.house_line, static_cast<u8>(b.tier + 1)))
          if (b.extra > 0 && b.extra - 1 < nt->reqs.size()) ++blockers[requirement_text(D, nt->reqs[b.extra - 1])];
      });
      out.push_back(std::to_string(houses) + " occupied houses.");
      if (!blockers.empty()) {
        auto top = std::max_element(blockers.begin(), blockers.end(), [](auto& x, auto& y) { return x.second < y.second; });
        out.push_back("Most houses could improve if they had " + top->first + " (" + std::to_string(top->second) + " houses).");
      }
      if (risky) out.push_back(std::to_string(risky) + " buildings are at risk of fire. Vigiles posts would protect them.");
      if (damaged) out.push_back(std::to_string(damaged) + " buildings are badly worn. Engineers' posts repair them.");
      if (w.plague_houses) out.push_back("Plague in " + std::to_string(w.plague_houses) + " houses! Physicians are needed.");
      if (out.size() == 1) out.push_back("The city stands firm.");
      break;
    }
    case 2: {  // quaestor: money
      const auto& L = w.econ.money;
      i64 net = 0;
      int top_in = -1, top_out = -1;
      for (int k = 0; k < econ::kMoneyCount; ++k) {
        const bool inc = econ::money_is_income(static_cast<econ::Money>(k));
        net += inc ? L.yesterday[k] : -L.yesterday[k];
        if (inc && (top_in < 0 || L.yesterday[k] > L.yesterday[top_in])) top_in = k;
        if (!inc && (top_out < 0 || L.yesterday[k] > L.yesterday[top_out])) top_out = k;
      }
      out.push_back("The treasury holds " + std::to_string(w.econ.treasury) + " denarii; yesterday we " +
                    (net >= 0 ? "gained " : "lost ") + std::to_string(net >= 0 ? net : -net) + ".");
      if (top_in >= 0 && L.yesterday[top_in] > 0)
        out.push_back(std::string("Our best source of income is ") + econ::money_name(static_cast<econ::Money>(top_in)) + ".");
      if (top_out >= 0 && L.yesterday[top_out] > 0)
        out.push_back(std::string("Our largest expense is ") + econ::money_name(static_cast<econ::Money>(top_out)) + ".");
      if (net < 0) out.push_back("Raise taxes, build forums to collect them, or export more goods.");
      if (w.loan.remaining > 0) out.push_back("We still owe Rome " + std::to_string(w.loan.remaining) + " denarii.");
      if (w.econ.treasury < 0) out.push_back("We are in debt! Rome will not tolerate this for long.");
      break;
    }
    case 3: {  // haruspex: gods
      int worst = -1, best = -1;
      for (size_t g = 0; g < w.god_mood.size(); ++g) {
        if (!w.god_available[g]) continue;
        if (worst < 0 || w.god_mood[g] < w.god_mood[static_cast<size_t>(worst)]) worst = static_cast<int>(g);
        if (best < 0 || w.god_mood[g] > w.god_mood[static_cast<size_t>(best)]) best = static_cast<int>(g);
      }
      if (worst >= 0) {
        const u8 m = w.god_mood[static_cast<size_t>(worst)];
        out.push_back(D.gods[static_cast<size_t>(worst)].name +
                      (m < 20 ? " is furious with us! Build a temple or hold a festival at once."
                              : m < 40 ? " is displeased. A temple in their honour would help." : " is content enough."));
      }
      if (best >= 0 && w.god_mood[static_cast<size_t>(best)] >= 80)
        out.push_back(D.gods[static_cast<size_t>(best)].name + " smiles upon the city.");
      for (size_t g = 0; g < D.gods.size(); ++g)
        if (w.god_available[g]) {
          const u32 doy = sim::day_of_year(w.now);
          const u32 days = (D.gods[g].festival_day + kDaysPerYear - doy) % kDaysPerYear;
          if (days <= 2) out.push_back("The feast of " + D.gods[g].name + " is " + (days == 0 ? "today" : "near") +
                                       ": a festival now pleases them doubly.");
        }
      out.push_back(std::string("The weather is ") + sim::weather_name(w.weather) + ".");
      break;
    }
    case 4: {  // legate: military
      u32 ours = 0, theirs = 0;
      for (const auto& u : w.army.units)
        if (u.alive) (u.side == 0 ? ours : theirs) += u.count;
      out.push_back("We have " + std::to_string(ours) + " soldiers under arms.");
      if (theirs) out.push_back("Enemies in the province: " + std::to_string(theirs) + " warriors! Muster the legions.");
      else out.push_back("No enemy is in sight.");
      out.push_back("Security is rated " + std::to_string(w.ratings.security) + ".");
      if (w.crimes_yday > 2) out.push_back("Crime is rising. Guard posts and jobs for the idle would help.");
      break;
    }
    case 5: {  // aedile: food, markets, morale
      u32 food = 0, eaten = 0;
      for (GoodId g : w.food_slots) {
        food += w.econ.city_stock[g.v];
        eaten += w.econ.consumed_yday[g.v];
      }
      out.push_back(eaten ? "We have about " + std::to_string(food / std::max<u32>(1, eaten)) + " days of food in store."
                          : "We have " + std::to_string(food) + " units of food in store.");
      out.push_back("The people's mood is " + std::to_string(w.avg_happiness) + " (128 is content).");
      u32 unemployed = 0, adults = 0;
      for (u32 i = 0; i < w.citizens.ids.high_water(); ++i) {
        if (!w.citizens.ids.alive_index(i) || (w.citizens.flags[i] & sim::kFlagVisitor)) continue;
        const u32 age = w.citizens.age_years(i, w.now);
        if (age < 14 || age >= 60) continue;
        ++adults;
        if (!w.citizens.work[i].valid()) ++unemployed;
      }
      if (adults && unemployed * 100 / adults > 15)
        out.push_back(std::to_string(unemployed * 100 / adults) + "% of adults have no work. Build workshops or farms.");
      out.push_back("Culture is rated " + std::to_string(w.ratings.culture) + ". Theaters, temples and schools raise it.");
      break;
    }
    default: break;
  }
  return out;
}

}  // namespace urbs::game
