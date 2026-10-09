// Trade: land caravans walk from the map edge to a trade post and back; sea routes dock a ship at
// a harbor. Exchanges follow the player's per-good import/export settings and partner yearly caps.
#include <algorithm>

#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

namespace {
constexpr u16 kPerVisit = 120;  // max units of one good per visit
}

void World::earn(i64 d, econ::Money m) {
  if (d <= 0) return;
  econ.treasury += d;
  econ.money.add(m, d);
}

void World::spend(i64 d, econ::Money m) {
  if (d <= 0) return;
  econ.treasury -= d;
  econ.money.add(m, d);
}

bool World::pay(i64 d, econ::Money m) {
  if (d <= 0) return true;
  if (econ.treasury < d) return false;
  spend(d, m);
  return true;
}

u32 World::trade_price(u32 p, GoodId g, bool buying) const {
  const auto& pd = defs->partners[p];
  const auto& rs = routes[p];
  const u32 base = static_cast<u32>(defs->goods[g.v].base_price) * 10;
  const auto& list = buying ? pd.sells : pd.buys;
  u32 cap = 1;
  for (const auto& ga : list)
    if (ga.good == g) cap = std::max<u32>(1, ga.amount);
  const u32 done = buying ? rs.bought[g.v] : rs.sold[g.v];
  const u32 sat = std::min<u32>(100, done * 100 / cap);  // % of yearly appetite used
  // Partners pay less as their appetite fills; sell dearer as their surplus runs out.
  return buying ? base * (120 + sat * 40 / 100) / 100 : base * (100 - sat * 40 / 100) / 100;
}

bool World::open_route_buys(GoodId g, bool sea) const {
  for (u32 p = 0; p < routes.size(); ++p) {
    if (!routes[p].open || defs->partners[p].sea != sea) continue;
    for (const auto& ga : defs->partners[p].buys)
      if (ga.good == g && routes[p].sold[g.v] < ga.amount) return true;
  }
  return false;
}

BuildingId World::nearest_trade_post(bool sea, i32 near) const {
  BuildingId best;
  int best_d = 1 << 30;
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.state != BState::Active || b.inv == econ::kNoInv || b.entrance < 0) return;
    const auto& d = defs->b(b.def);
    if (d.kind != BuildingKind::TradePost) return;
    const bool is_harbor = d.needs_water_adjacent;
    if (is_harbor != sea) return;
    if (!sea && near >= 0 && !nav.connected(near, b.entrance)) return;
    const int dd = near >= 0 ? std::abs(near % map.w - b.entrance % map.w) + std::abs(near / map.w - b.entrance / map.w) : 0;
    if (dd < best_d) {
      best_d = dd;
      best = id;
    }
  });
  return best;
}

void World::trade_exchange(u32 p, BuildingId post) {
  if (!buildings.alive(post) || buildings[post].inv == econ::kNoInv) return;
  const u32 inv = buildings[post].inv;
  const auto& pd = defs->partners[p];
  auto& rs = routes[p];
  // Exports: the city sells what the partner buys, while city stock stays above the export level.
  for (const auto& ga : pd.buys) {
    const GoodId g = ga.good;
    if (trade_mode[g.v] != 2) continue;
    const u32 stock = econ.city_stock[g.v];
    if (stock <= trade_level[g.v]) continue;
    const u32 cap_left = ga.amount > rs.sold[g.v] ? ga.amount - rs.sold[g.v] : 0;
    const u16 q = static_cast<u16>(std::min<u32>({econ.available(inv, g), cap_left, kPerVisit, stock - trade_level[g.v]}));
    if (q == 0) continue;
    const u32 unit = trade_price(p, g, false) * (effect(Effect::TradePrices) ? 6 : 5) / 5;
    econ.take(inv, g, q);
    earn(static_cast<i64>(unit) * q / 10, econ::Money::Exports);
    rs.sold[g.v] = static_cast<u16>(rs.sold[g.v] + q);
    econ.exported_today[g.v] += q;
  }
  // Imports: the city buys what the partner sells, while city stock is below the import level.
  for (const auto& ga : pd.sells) {
    const GoodId g = ga.good;
    if (trade_mode[g.v] != 1) continue;
    const u32 stock = econ.city_stock[g.v] + econ.amt(inv, g);
    if (stock >= trade_level[g.v]) continue;
    const u32 cap_left = ga.amount > rs.bought[g.v] ? ga.amount - rs.bought[g.v] : 0;
    u16 q = static_cast<u16>(std::min<u32>({econ.free_space(inv), cap_left, kPerVisit, trade_level[g.v] - stock}));
    const u32 unit = trade_price(p, g, true);
    if (unit > 0 && econ.treasury > 0) q = static_cast<u16>(std::min<i64>(q, econ.treasury * 10 / unit));
    if (q == 0) continue;
    spend(static_cast<i64>(unit) * q / 10, econ::Money::Imports);
    econ.add(inv, g, q);
    rs.bought[g.v] = static_cast<u16>(rs.bought[g.v] + q);
    econ.imported_today[g.v] += q;
  }
}

void World::trade_daily() {
  if (day_of_year(now) == 0 && hour_of_day(now) == 0)
    for (auto& r : routes) {
      std::fill(r.bought.begin(), r.bought.end(), u16{0});
      std::fill(r.sold.begin(), r.sold.end(), u16{0});
    }
  for (u32 p = 0; p < routes.size(); ++p) {
    auto& rs = routes[p];
    if (!rs.open || now < rs.next_visit) continue;
    if (effect(Effect::NoTrade)) {
      rs.next_visit = now + kTicksPerDay;
      continue;
    }
    const auto& pd = defs->partners[p];
    rs.next_visit = now + static_cast<Tick>(pd.interval_days) * kTicksPerDay * (tech(Tech::Cursus) ? 7 : 10) / 10;
    if (pd.sea) {
      BuildingId harbor = nearest_trade_post(true, -1);
      if (!harbor.valid()) continue;
      trade_exchange(p, harbor);
      rs.ship_until = now + kTicksPerDay / 2;
      rs.ship_at = harbor;
      continue;
    }
    if (entry_tile < 0) continue;
    BuildingId post = nearest_trade_post(false, entry_tile);
    if (!post.valid()) continue;
    // A caravan of three traders walks in; the first to arrive trades.
    rs.caravan_traded = 0;
    for (int k = 0; k < 3; ++k) {
      CitizenId c = spawn_citizen(CitizenClass::Eques, BuildingId{}, HouseholdId{}, 30, entry_tile);
      if (!c) break;
      const u32 i = c.index();
      citizens.flags[i] |= kFlagVisitor;
      citizens.task[i] = p;  // partner index
      citizens.reason[i] = Reason::None;
      ++visitors;
      ++citizens.token[i];
      request_walk(i, entry_tile, post, buildings[post].entrance, Activity::Trade);
    }
  }
}

}  // namespace urbs::sim
