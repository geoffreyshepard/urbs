#include "sim/autoplay.hpp"

#include <algorithm>
#include <map>

namespace urbs::sim {

using data::BuildingKind;
using data::HousingReq;

namespace {
constexpr i32 kSp = 9;  // road spacing: 8-tile blocks
}

void Autoplay::init(World& w) {
  // Origin: the road tile closest to the map centre (the end of the entry road), else the centre.
  i32 best = -1, bd = 1 << 30;
  for (i32 i = 0; i < w.map.w * w.map.h; ++i) {
    if (w.map.road[static_cast<size_t>(i)] == Road::None) continue;
    const i32 d = std::abs(i % w.map.w - w.map.w / 2) + std::abs(i / w.map.w - w.map.h / 2);
    if (d < bd) {
      bd = d;
      best = i;
    }
  }
  if (best < 0) best = w.map.idx(w.map.w / 2, w.map.h / 2);
  ox_ = best % w.map.w;
  oy_ = best / w.map.w;
  blocks_.clear();
  for (i32 by = -12; by <= 12; ++by)
    for (i32 bx = -12; bx <= 12; ++bx) {
      const i32 x = ox_ + bx * kSp + 1, y = oy_ + by * kSp + 1;
      if (x < 2 || y < 2 || x + 9 >= w.map.w || y + 9 >= w.map.h) continue;
      blocks_.push_back(Block{x, y, 0, 0});
    }
  std::stable_sort(blocks_.begin(), blocks_.end(), [&](const Block& a, const Block& b) {
    return std::abs(a.x - ox_) + std::abs(a.y - oy_) < std::abs(b.x - ox_) + std::abs(b.y - oy_);
  });
}

BuildingDefId Autoplay::first_unlocked(World& w, std::initializer_list<const char*> ids) const {
  for (const char* id : ids) {
    BuildingDefId d = w.defs->building(id);
    if (d.valid() && w.is_unlocked(d)) return d;
  }
  return BuildingDefId{};
}

u32 Autoplay::count(World& w, const char* id) const {
  BuildingDefId d = w.defs->building(id);
  if (!d.valid()) return 0;
  u32 n = 0;
  w.buildings.for_each([&](BuildingId, const Building& b) {
    if (b.def == d && b.state != BState::Ruin) ++n;
  });
  return n;
}

// Buildings (not ruins) whose recipe makes g.
static u32 producers_of(World& w, GoodId g) {
  u32 n = 0;
  w.buildings.for_each([&](BuildingId, const Building& b) {
    const auto& d = w.defs->b(b.def);
    if (b.state == BState::Ruin || !d.recipe.valid()) return;
    for (const auto& o : w.defs->recipes[d.recipe.v].outputs)
      if (o.good == g) ++n;
  });
  return n;
}

bool Autoplay::activate_block(World& w, i32 nx, i32 ny) {
  if (last_block_ != 0 && w.now - last_block_ < kTicksPerDay / 2) return false;
  last_block_ = w.now;
  // The nearest inactive, buildable block to (nx, ny), weighted toward the origin.
  Block* pick = nullptr;
  i32 bestd = 1 << 30;
  for (Block& b : blocks_) {
    if (b.active || b.bad) continue;
    u32 ok = 0;
    for (i32 y = b.y; y < b.y + 8; ++y)
      for (i32 x = b.x; x < b.x + 8; ++x) {
        const size_t i = static_cast<size_t>(w.map.idx(x, y));
        if (!terrain_water(w.map.terrain[i]) && !w.map.building[i].valid()) ++ok;
      }
    if (ok < 40) {
      b.bad = 1;
      continue;
    }
    const i32 d = std::abs(b.x - nx) + std::abs(b.y - ny) + (std::abs(b.x - ox_) + std::abs(b.y - oy_)) / 2;
    if (d < bestd) {
      bestd = d;
      pick = &b;
    }
  }
  if (!pick) return false;
  const i64 cost = 4 * 10;
  if (w.econ.treasury < cost + 100) return false;
  pick->active = 1;
  auto road = [&](i32 x0, i32 y0, i32 x1, i32 y1) {
    Command c;
    c.type = CmdType::PlaceRoad;
    c.def = static_cast<u16>(Road::Dirt);
    c.x0 = x0;
    c.y0 = y0;
    c.x1 = x1;
    c.y1 = y1;
    w.push(c);
  };
  road(pick->x - 1, pick->y - 1, pick->x + 8, pick->y - 1);
  road(pick->x - 1, pick->y + 8, pick->x + 8, pick->y + 8);
  road(pick->x - 1, pick->y - 1, pick->x - 1, pick->y + 8);
  road(pick->x + 8, pick->y - 1, pick->x + 8, pick->y + 8);
  last_ = "roads for a new block";
  return true;
}

bool Autoplay::place(World& w, BuildingDefId def, i32 nx, i32 ny, bool painted_block, i32 max_dist) {
  if (!def.valid() || !w.is_unlocked(def)) return false;
  const auto& d = w.defs->b(def);
  i32 bw = d.w, bh = d.h;
  if (d.painted) {
    bw = std::max<i32>(d.min_w, std::min<i32>(6, d.max_w));
    bh = std::max<i32>(d.min_h, std::min<i32>(6, d.max_h));
    if (d.max_w == 1) bw = 1;
  }
  const i64 area = static_cast<i64>(bw) * bh, base = static_cast<i64>(d.w) * d.h;
  const i64 cost = d.painted ? std::max<i64>(1, d.cost_denarii * area / std::max<i64>(1, base)) : d.cost_denarii;
  // Keep a reserve so wages never push the treasury into debt (Rome frowns on that).
  const i64 reserve = d.kind == BuildingKind::House ? 50 : 50 + std::min<i64>(350, w.population() / 5);
  if (w.econ.treasury < cost + reserve) return false;
  // Only where workers can walk from the town: a perimeter tile in the origin's region.
  const i32 origin = w.map.idx(ox_, oy_);
  auto reachable = [&](i32 x, i32 y) {
    for (i32 ty = y - 1; ty <= y + bh; ++ty)
      for (i32 tx = x - 1; tx <= x + bw; ++tx) {
        if (!w.map.in_bounds(tx, ty) || (ty >= y && ty < y + bh && tx >= x && tx < x + bw)) continue;
        if (w.nav.walkable(tx, ty) && w.nav.connected(w.map.idx(tx, ty), origin)) return true;
      }
    return false;
  };
  auto issue = [&](i32 x, i32 y) {
    Command c;
    c.type = CmdType::PlaceBuilding;
    c.def = def.v;
    c.x0 = x;
    c.y0 = y;
    c.x1 = bw;
    c.y1 = bh;
    w.push(c);
    last_ = d.name;
    ++actions;
    return true;
  };
  const bool anywhere = d.resource != 0 || d.needs_water_adjacent || d.kind == BuildingKind::Farm;
  if (anywhere || painted_block) {
    // Spiral search around the town for a valid spot (resources, water edges, fertile fields).
    i32 best_x = -1, best_y = -1, best_score = -(1 << 30);
    for (i32 r = 0; r < 60; r += 1)
      for (i32 dy = -r; dy <= r; ++dy)
        for (i32 dx = -r; dx <= r; ++dx) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const i32 x = nx + dx, y = ny + dy;
          if (!w.can_place(def, x, y, bw, bh) || !reachable(x, y)) continue;
          // Keep farms out of the housing blocks; prefer fertile soil.
          bool in_block = false;
          for (const Block& b : blocks_)
            if (b.active && x < b.x + 8 && x + bw > b.x && y < b.y + 8 && y + bh > b.y) in_block = true;
          if (in_block && d.kind == BuildingKind::Farm) continue;
          i32 fert = 0;
          for (i32 ty = y; ty < y + bh; ++ty)
            for (i32 tx = x; tx < x + bw; ++tx) fert += w.map.fertility[static_cast<size_t>(w.map.idx(tx, ty))];
          const i32 score = (d.kind == BuildingKind::Farm ? fert / (bw * bh) : 0) - r * 3;
          if (score > best_score) {
            best_score = score;
            best_x = x;
            best_y = y;
          }
        }
    if (best_x >= 0) return issue(best_x, best_y);
    return false;
  }
  // Lots on the ring of active blocks, nearest to (nx, ny); footprint must touch the block edge.
  i32 best_x = -1, best_y = -1, best_d = max_dist > 0 ? max_dist + 1 : 1 << 30;
  for (const Block& b : blocks_) {
    if (!b.active) continue;
    if (std::abs(b.x - nx) + std::abs(b.y - ny) > best_d + 16) continue;
    for (i32 y = b.y; y + bh <= b.y + 8; ++y)
      for (i32 x = b.x; x + bw <= b.x + 8; ++x) {
        const bool edge = x == b.x || y == b.y || x + bw == b.x + 8 || y + bh == b.y + 8;
        if (!edge) continue;
        if (!w.can_place(def, x, y, bw, bh)) continue;
        const i32 dd = std::abs(x - nx) + std::abs(y - ny);
        if (dd >= best_d || !reachable(x, y)) continue;
        if (dd < best_d) {
          best_d = dd;
          best_x = x;
          best_y = y;
        }
      }
  }
  if (best_x >= 0) return issue(best_x, best_y);
  if (max_dist == 0) activate_block(w, nx, ny);
  return false;
}

bool Autoplay::build_producer_of(World& w, GoodId g, i32 cx, i32 cy, int depth) {
  if (depth > 3 || !g.valid()) return false;
  for (u16 k = 0; k < w.defs->buildings.size(); ++k) {
    const auto& d = w.defs->buildings[k];
    if (!d.recipe.valid() || !w.is_unlocked(BuildingDefId{k})) continue;
    const auto& r = w.defs->recipes[d.recipe.v];
    bool makes = false;
    for (const auto& o : r.outputs)
      if (o.good == g) makes = true;
    if (!makes) continue;
    // Build missing inputs first: one input producer for every two consumers is a fair default.
    const u32 consumers = producers_of(w, g);
    for (const auto& in : r.inputs)
      if (w.econ.city_stock[in.good.v] < 20 && producers_of(w, in.good) * 2 < consumers + 1)
        return build_producer_of(w, in.good, cx, cy, depth + 1);
    if (place(w, BuildingDefId{k}, cx, cy)) return true;
  }
  return false;
}

bool Autoplay::build_for_requirement(World& w, const HousingReq& r, i32 cx, i32 cy) {
  switch (r.kind) {
    case HousingReq::Service:
    case HousingReq::Patrol:
      for (ServiceId s : r.services)
        for (u16 k = 0; k < w.defs->buildings.size(); ++k) {
          const auto& d = w.defs->buildings[k];
          if (d.service == s && w.is_unlocked(BuildingDefId{k}) && d.kind != BuildingKind::Monument && d.w <= 4)
            if (place(w, BuildingDefId{k}, cx, cy)) return true;
        }
      return false;
    case HousingReq::FoodKinds:
    case HousingReq::Food: {
      // Plenty of food in store but none at home: the house needs a market nearby.
      {
        u32 kinds = 0;
        for (GoodId g : w.food_slots) kinds += w.econ.city_stock[g.v] >= 100 ? 1u : 0u;
        if (kinds >= 2 && place(w, w.defs->building("market"), cx, cy)) return true;
      }
      // Another kind of food: a farm type we do not have yet, then processed food.
      for (const char* id : {"vegetable_garden", "wheat_farm", "fishing_wharf", "pig_farm"})
        if (count(w, id) == 0 && place(w, w.defs->building(id), ox_, oy_)) return true;
      if (r.kind == HousingReq::Food && !r.goods.empty()) return build_producer_of(w, r.goods[0], cx, cy, 0);
      if (count(w, "bakery") == 0) return build_producer_of(w, w.defs->good("bread"), cx, cy, 0);
      return false;
    }
    case HousingReq::Goods:
      for (GoodId g : r.goods)
        if (w.econ.city_stock[g.v] < 30) return build_producer_of(w, g, cx, cy, 0);
      return false;
    case HousingReq::Desirability:
      return place(w, first_unlocked(w, {"statue", "garden"}), cx, cy);
    default: return false;
  }
}

void Autoplay::do_trade_and_requests(World& w) {
  std::vector<u8> wanted(w.econ.goods, 0);
  for (const auto& r : w.requests)
    if (r.active) {
      Command c;
      c.type = CmdType::FulfillRequest;
      c.def = r.id;
      w.push(c);
      if (r.kind == 0 && r.good.valid() && r.good.v < wanted.size()) wanted[r.good.v] = 1;
    }
  request_good_ = GoodId{};
  // Goods requested by Rome: stockpile them (markets leave them in storage) and make sure they are produced.
  for (u32 g = 0; g < w.econ.goods; ++g) {
    const u8 want_mode = wanted[g] ? 3 : 0;
    if ((w.trade_mode[g] == 3 || wanted[g]) && w.trade_mode[g] != want_mode) {
      Command c;
      c.type = CmdType::SetTrade;
      c.def = static_cast<u16>(g);
      c.value = want_mode;
      c.x0 = w.trade_level[g];
      w.push(c);
    }
    if (wanted[g]) request_good_ = GoodId{static_cast<u16>(g)};
  }
  // Open affordable land routes and sell surpluses (once the town is established).
  for (u32 p = 0; p < w.routes.size() && w.population() >= 150; ++p) {
    const auto& rs = w.routes[p];
    if (rs.available && !rs.open && w.econ.treasury > w.defs->partners[p].open_cost + 400) {
      Command c;
      c.type = CmdType::OpenRoute;
      c.def = static_cast<u16>(p);
      w.push(c);
    }
  }
  for (u32 g = 0; g < w.econ.goods; ++g)
    if (w.econ.city_stock[g] > 200 && w.trade_mode[g] == 0) {
      Command c;
      c.type = CmdType::SetTrade;
      c.def = static_cast<u16>(g);
      c.value = 2;
      c.x0 = 120;
      w.push(c);
    }
}

void Autoplay::step(World& w) {
  if (w.now % kTicksPerHour != 25) return;
  // Defence: when raiders appear, raise militia (or legions) and march them on the enemy.
  {
    u32 enemies = 0, ours = 0;
    f32 ex = 0, ey = 0;
    for (const auto& u : w.army.units)
      if (u.alive) {
        if (u.side == 1) {
          enemies += u.count;
          ex = u.ax;
          ey = u.ay;
        } else {
          ours += u.count;
        }
      }
    if (enemies > 0) last_enemy_ = w.now;
    // Soldiers cost upkeep: stand the militia down two days after the last raid.
    if (enemies == 0 && ours > 0 && w.now - last_enemy_ > 2 * kTicksPerDay)
      for (u16 u = 0; u < military::kMaxUnits; ++u)
        if (w.army.units[u].alive && w.army.units[u].side == 0 && w.army.units[u].order != military::Order::Away) {
          Command c;
          c.type = CmdType::DisbandUnit;
          c.def = u;
          w.push(c);
        }
    if (enemies > 0) {
      if (ours < enemies) {
        BuildingId post;
        w.buildings.for_each([&](BuildingId id, const Building& b) {
          const auto& d = w.defs->b(b.def);
          if (!post.valid() && b.state == BState::Active && (d.id == "militia_post" || d.id == "barracks")) post = id;
        });
        if (post.valid()) {
          Command c;
          c.type = CmdType::RaiseUnit;
          c.def = w.defs->b(w.buildings[post].def).id == "barracks" ? w.defs->unit("legionary").v : w.defs->unit("militia").v;
          c.value = static_cast<i32>(post.raw);
          w.push(c);
        }
      }
      for (u16 u = 0; u < military::kMaxUnits; ++u)
        if (w.army.units[u].alive && w.army.units[u].side == 0 && w.army.units[u].order == military::Order::Hold) {
          Command c;
          c.type = CmdType::UnitOrder;
          c.def = u;
          c.value = static_cast<i32>(military::Order::Move);
          c.x0 = static_cast<i32>(ex);
          c.y0 = static_cast<i32>(ey);
          w.push(c);
        }
    }
  }
  const u32 hour = hour_of_day(w.now);
  if (hour == 6) {
    do_trade_and_requests(w);
    // Food rotting in granaries is wages for nothing: close the outermost farm while the surplus lasts.
    u32 food = 0;
    for (GoodId g : w.food_slots) food += w.econ.city_stock[g.v];
    if (food > 300 + w.population() * 5) {
      BuildingId far;
      i32 far_d = -1;
      w.buildings.for_each([&](BuildingId id, const Building& b) {
        if (b.state != BState::Active || w.defs->b(b.def).kind != BuildingKind::Farm) return;
        bool food_farm = false;
        for (const auto& o : w.defs->recipes[w.defs->b(b.def).recipe.v].outputs) food_farm |= w.defs->goods[o.good.v].food;
        const i32 d = std::abs(b.x - ox_) + std::abs(b.y - oy_);
        if (food_farm && d > far_d) {
          far_d = d;
          far = id;
        }
      });
      if (far.valid() && count(w, "wheat_farm") + count(w, "vegetable_garden") + count(w, "pig_farm") > 2) {
        const Building& b = w.buildings[far];
        Command c;
        c.type = CmdType::Demolish;
        c.x0 = b.x;
        c.y0 = b.y;
        c.x1 = b.x + b.w - 1;
        c.y1 = b.y + b.h - 1;
        w.push(c);
        last_ = "closed a surplus farm";
      }
    }
    // Budget: raise plebeian taxes while the treasury shrinks, ease them when rich.
    i64 net = 0;
    for (int k = 0; k < econ::kMoneyCount; ++k)
      net += econ::money_is_income(static_cast<econ::Money>(k)) ? w.econ.money.yesterday[k] : -w.econ.money.yesterday[k];
    Command c;
    c.type = CmdType::SetPolicy;
    c.def = static_cast<u16>(PolicyId::TaxPleb);
    if (w.econ.treasury < 3000 && net < 0 && w.policy.tax_rate[2] < 14) {
      c.value = w.policy.tax_rate[2] + 2;
      w.push(c);
    } else if (w.econ.treasury > 8000 && w.policy.tax_rate[2] > 6) {
      c.value = w.policy.tax_rate[2] - 1;
      w.push(c);
    }
  }
  if (w.econ.treasury < 120) return;
  const u32 pop = w.population();
  u32 cap = 0, residents = 0, houses = 0;
  std::map<u32, u32> blockers;  // (tier << 8 | req index) -> houses
  std::map<u32, std::pair<i32, i32>> blocked_at;  // a house held back by that requirement
  i32 bx = ox_, by = oy_;
  i32 sum_x = 0, sum_y = 0;
  BuildingDefId house = w.defs->building("house_plot");
  w.buildings.for_each([&](BuildingId, const Building& b) {
    if (w.defs->b(b.def).kind != BuildingKind::House || b.state == BState::Ruin) return;
    if (w.defs->b(b.def).house_line != 0) return;
    ++houses;
    cap += b.capacity;
    residents += b.residents;
    sum_x += b.x;
    sum_y += b.y;
    if (b.extra > 0 && b.residents > 0) {
      const u32 key = (static_cast<u32>(b.tier + 1) << 8) | (b.extra - 1);
      // Spread the sample: houses seen later replace it every few hits.
      if (blockers[key]++ % 3 == 0) blocked_at[key] = {b.x, b.y};
    }
  });
  if (houses) {
    bx = sum_x / static_cast<i32>(houses);
    by = sum_y / static_cast<i32>(houses);
  }
  u32 idle = 0, adults = 0;
  for (u32 i = 0; i < w.citizens.ids.high_water(); ++i) {
    if (!w.citizens.ids.alive_index(i)) continue;
    const u32 a = w.citizens.age_years(i, w.now);
    if (a < 14 || a >= 60) continue;
    ++adults;
    if (!w.citizens.work[i].valid()) ++idle;
  }
  const bool jobless = adults > 40 && idle * 100 / adults > 25;
  // 1. Always keep some vacant housing (unless jobs are what the city lacks).
  // Past the population goal, improve the houses there are rather than sprawl.
  const bool enough = w.goals.population > 0 && pop >= w.goals.population + w.goals.population / 10;
  if (houses < 6 || (cap - std::min(cap, residents) < std::max<u32>(15, pop / 8) && !(jobless && idle * 100 / adults > 35) && !enough)) {
    if (place(w, house, bx, by)) return;
  }
  // Water first: a well beside any occupied house that has no water source in reach.
  {
    const data::HousingReq water{data::HousingReq::Service, {w.defs->service("well"), w.defs->service("fountain")}, {}, 0};
    std::pair<i32, i32> dry[8];
    u32 ndry = 0;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      if (ndry == 8 || b.residents == 0 || w.defs->b(b.def).kind != BuildingKind::House) return;
      if (!w.requirement_met(b, water)) dry[ndry++] = {b.x, b.y};
    });
    if (ndry > 0 && water_cooldown_ <= w.now) {
      water_cooldown_ = w.now + kTicksPerHour * 3;  // let the service map catch up
      for (u32 k = 0; k < ndry; ++k)
        if (place(w, w.defs->building("well"), dry[k].first, dry[k].second, false, 4)) return;
    }
  }
  // Builders are few early on: keep at most two non-house construction sites at a time.
  // A site nobody has worked on for four days (unreachable, or its materials never come) is cleared.
  u32 sites = 0;
  BuildingId clear;
  w.buildings.for_each([&](BuildingId id, const Building& b) {
    const auto& sd = w.defs->b(b.def);
    if (b.state != BState::Construction || sd.kind == BuildingKind::House) return;
    if (b.build_progress == 0 && b.present == 0) {
      auto [it, fresh] = stuck_since_.try_emplace(id.raw, w.now);
      if (!fresh && w.now - it->second > 4 * kTicksPerDay) clear = id;
    } else {
      stuck_since_.erase(id.raw);
    }
    if (sd.build_work > 20) ++sites;
  });
  if (clear.valid()) {
    const Building& b = w.buildings[clear];
    stuck_since_.erase(clear.raw);
    Command c;
    c.type = CmdType::Demolish;
    c.x0 = b.x;
    c.y0 = b.y;
    c.x1 = b.x + b.w - 1;
    c.y1 = b.y + b.h - 1;
    w.push(c);
    last_ = "cleared a stalled site";
    return;
  }
  if (sites >= 2 + pop / 400) return;
  // 2. Basics in order.
  auto ensure = [&](const char* id, u32 want) {
    BuildingDefId d = w.defs->building(id);
    if (!d.valid() || !w.is_unlocked(d) || count(w, id) >= want) return false;
    return place(w, d, bx, by);
  };
  if (pop >= 5 && ensure("lumber_camp", 1)) return;
  if (pop >= 8 && ensure("well", 1 + pop / 60)) return;
  u32 food_stock = 0;
  for (GoodId g : w.food_slots) food_stock += w.econ.city_stock[g.v];
  if (pop >= 10 && count(w, "wheat_farm") + count(w, "vegetable_garden") < 1 + pop / 120 && food_stock < 200 + pop * 2) {
    if (place(w, w.defs->building(count(w, "wheat_farm") <= count(w, "vegetable_garden") ? "wheat_farm" : "vegetable_garden"),
              ox_, oy_))
      return;
  }
  if (pop >= 12 && ensure("market", 1 + pop / 300)) return;
  if (pop >= 40 && ensure("granary", 1 + pop / 700)) return;
  if (pop >= 25 && ensure("shrine", 1 + pop / 180)) return;
  if (pop >= 60 && ensure("lumber_camp", 1)) return;
  if (pop >= 80 && ensure("warehouse", 1 + pop / 900)) return;
  if (pop >= 40 && ensure("forum", 1 + pop / 500)) return;
  if (pop >= 120 && ensure("vigiles_post", pop / 250)) return;
  if (pop >= 150 && w.mission_number >= 3 && ensure("militia_post", 1 + pop / 1500)) return;
  if (pop >= 200 && ensure("engineers_post", pop / 400)) return;
  // Storage: when producers sit on full output piles, the granaries or warehouses are full.
  {
    u32 stuck_food = 0, stuck_other = 0;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      const auto& d = w.defs->b(b.def);
      if (b.state != BState::Active || !d.recipe.valid() || b.inv == econ::kNoInv || w.econ.free_space(b.inv) > 4) return;
      for (const auto& o : w.defs->recipes[d.recipe.v].outputs)
        if (w.econ.amt(b.inv, o.good) >= 10) ++(w.defs->goods[o.good.v].food ? stuck_food : stuck_other);
    });
    if (stuck_food >= 2 && place(w, w.defs->building("granary"), bx, by)) return;
    if (stuck_other >= 2 && place(w, w.defs->building("warehouse"), bx, by)) return;
  }
  // Rome's request: at least two producers of the requested good.
  if (request_good_.valid() && producers_of(w, request_good_) < 2 && build_producer_of(w, request_good_, bx, by, 0)) return;
  // Exports go out through a trade post.
  {
    bool any_open = false;
    for (const auto& rs : w.routes) any_open |= rs.open != 0;
    if (any_open && ensure("trade_post", 1)) return;
  }
  // 3. Whatever most houses need to grow.
  for (auto it = blockers.begin(); it != blockers.end();)
    it = (blocker_cooldown_.count(it->first) && blocker_cooldown_[it->first] > w.now) ? blockers.erase(it) : std::next(it);
  if (!blockers.empty()) {
    auto top = std::max_element(blockers.begin(), blockers.end(), [](auto& a, auto& b) { return a.second < b.second; });
    const u8 tier = static_cast<u8>(top->first >> 8);
    const auto* td = w.defs->tier(0, tier);
    if (td && (top->first & 0xFF) < td->reqs.size()) {
      // Give coverage maps and supply chains a day to catch up before chasing the same need again.
      blocker_cooldown_[top->first] = w.now + kTicksPerDay;
      const auto at = blocked_at.count(top->first) ? blocked_at[top->first] : std::pair<i32, i32>{bx, by};
      if (build_for_requirement(w, td->reqs[top->first & 0xFF], at.first, at.second)) return;
    }
  }
  // 4. Mission goal buildings.
  if (pop >= 100)
    for (const auto& g : w.goals.buildings) {
      if (count(w, w.defs->b(g.def).id.c_str()) >= g.count || !w.is_unlocked(g.def)) continue;
      bool materials = true;
      for (const auto& c : w.defs->b(g.def).cost_goods)
        if (w.econ.city_stock[c.good.v] < c.amount) {
          materials = false;
          if (producers_of(w, c.good) == 0 && build_producer_of(w, c.good, bx, by, 0)) return;
          // Cannot make it here: import it if an open route sells it.
          for (u32 p = 0; p < w.routes.size(); ++p)
            for (const auto& ga : w.defs->partners[p].sells)
              if (w.routes[p].open && ga.good == c.good && w.trade_mode[c.good.v] != 1) {
                Command cmd;
                cmd.type = CmdType::SetTrade;
                cmd.def = c.good.v;
                cmd.value = 1;
                cmd.x0 = c.amount + 20;
                w.push(cmd);
              }
        }
      if (materials && place(w, g.def, bx, by)) return;
    }
  // 5. Culture and comfort as the city grows.
  if (pop >= 150 && ensure("tavern", pop / 300)) return;
  if (pop >= 150 && ensure("physician", pop / 300)) return;
  if (pop >= 200 && ensure("latrina", pop / 250)) return;
  if (pop >= 250 && ensure("temple", pop / 450)) return;
  if (pop >= 300 && ensure("school", pop / 500)) return;
  if (pop >= 400 && ensure("theater", pop / 900)) return;
  if (pop >= 300 && ensure("guard_post", pop / 500)) return;
  // 6. Jobs: if many are idle, add a productive workshop.
  if (jobless) {
    // First, workshops that turn the largest pile of raw goods into something worth more.
    {
      GoodId pile{};
      u32 most = 300;
      for (u32 g = 0; g < w.econ.goods; ++g)
        if (w.econ.city_stock[g] > most) {
          for (u16 k = 0; k < w.defs->buildings.size(); ++k) {
            const auto& d = w.defs->buildings[k];
            if (!d.recipe.valid() || !w.is_unlocked(BuildingDefId{k})) continue;
            for (const auto& in : w.defs->recipes[d.recipe.v].inputs)
              if (in.good.v == g) {
                most = w.econ.city_stock[g];
                pile = GoodId{static_cast<u16>(g)};
              }
          }
        }
      if (pile.valid())
        for (u16 k = 0; k < w.defs->buildings.size(); ++k) {
          const auto& d = w.defs->buildings[k];
          if (!d.recipe.valid() || !w.is_unlocked(BuildingDefId{k})) continue;
          const auto& r = w.defs->recipes[d.recipe.v];
          bool uses = false;
          for (const auto& in : r.inputs) uses |= in.good == pile;
          if (uses && !r.outputs.empty() && build_producer_of(w, r.outputs[0].good, bx, by, 0)) return;
        }
    }
    // Prefer goods a trading partner buys (exports bring money into the city), fewest producers first.
    GoodId best{};
    u32 best_n = ~0u;
    for (u32 p = 0; p < w.routes.size(); ++p) {
      if (!w.routes[p].available) continue;
      for (const auto& ga : w.defs->partners[p].buys) {
        const u32 n = producers_of(w, ga.good);
        if (n < best_n) {
          best_n = n;
          best = ga.good;
        }
      }
    }
    if (best.valid() && build_producer_of(w, best, bx, by, 0)) return;
    if (build_producer_of(w, w.defs->good("pottery"), bx, by, 0)) return;
    if (food_stock < pop * 2) place(w, w.defs->building("wheat_farm"), ox_, oy_);
  }
}

}  // namespace urbs::sim
