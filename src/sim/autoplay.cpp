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
  for (u16 p : pending_) n += p == d.v ? 1u : 0u;  // ordered this hour, not built yet
  return n;
}

// Buildings (not ruins, plus those ordered this hour) whose recipe makes g.
u32 Autoplay::producers_of(World& w, GoodId g) const {
  auto makes = [&](BuildingDefId def) {
    const auto& d = w.defs->b(def);
    if (!d.recipe.valid()) return false;
    for (const auto& o : w.defs->recipes[d.recipe.v].outputs)
      if (o.good == g) return true;
    return false;
  };
  u32 n = 0;
  w.buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state != BState::Ruin && makes(b.def)) ++n;
  });
  for (u16 p : pending_) n += makes(BuildingDefId{p}) ? 1u : 0u;
  return n;
}

bool Autoplay::supply(World& w, GoodId g, u32 amount, i32 x, i32 y) {
  // Import it if an open route sells it.
  for (u32 p = 0; p < w.routes.size(); ++p)
    for (const auto& ga : w.defs->partners[p].sells)
      if (w.routes[p].open && ga.good == g && w.trade_mode[g.v] != 1) {
        Command c;
        c.type = CmdType::SetTrade;
        c.def = g.v;
        c.value = 1;
        c.x0 = static_cast<i32>(amount) + 40;
        w.push(c);
      }
  if (producers_of(w, g) < 1 + w.population() / 1500) return build_producer_of(w, g, x, y, 0);
  return false;
}

// Plumbing: a reservoir beside open water, and a pipe from it to every building that needs piped water.
bool Autoplay::plumb(World& w) {
  const BuildingDefId res = w.defs->building("reservoir");
  if (!res.valid() || !w.is_unlocked(res)) return false;
  // Something needing pipes without water?
  BuildingId dry;
  w.buildings.for_each([&](BuildingId id, const Building& b) {
    if (dry.valid() || b.state != BState::Active || !w.defs->b(b.def).needs_pipe || (b.flags & 0x20)) return;
    auto it = pipe_tried_.find(id.raw);
    if (it != pipe_tried_.end() && it->second > w.now) return;
    dry = id;
  });
  if (!dry.valid()) return false;
  BuildingId full;
  i32 best = 1 << 30;
  w.buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.def != res || b.state != BState::Active || !w.reservoir_full(id)) return;
    const i32 d = std::abs(b.x - w.buildings[dry].x) + std::abs(b.y - w.buildings[dry].y);
    if (d < best) {
      best = d;
      full = id;
    }
  });
  if (!full.valid()) {
    // A reservoir on the shore nearest the town fills straight from the water.
    if (res_cooldown_ > w.now || count(w, "reservoir") > 0) return false;
    res_cooldown_ = w.now + kTicksPerDay;
    for (const auto& c : w.defs->b(res).cost_goods)
      if (w.econ.city_stock[c.good.v] < c.amount) return supply(w, c.good, c.amount, ox_, oy_);
    if (w.econ.treasury < w.defs->b(res).cost_denarii + 300) return false;
    i32 bx = -1, by = -1;
    for (i32 r = 0; r < 110 && bx < 0; ++r)
      for (i32 dy = -r; dy <= r && bx < 0; ++dy)
        for (i32 dx = -r; dx <= r && bx < 0; ++dx) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const i32 x = ox_ + dx, y = oy_ + dy;
          if (!w.can_place(res, x, y, 3, 3)) continue;
          bool shore = false;
          for (i32 ty = y - 1; ty <= y + 3 && !shore; ++ty)
            for (i32 tx = x - 1; tx <= x + 3 && !shore; ++tx)
              if (w.map.in_bounds(tx, ty) && terrain_water(w.map.terrain[static_cast<size_t>(w.map.idx(tx, ty))])) shore = true;
          if (shore) {
            bx = x;
            by = y;
          }
        }
    if (bx < 0) return false;
    Command c;
    c.type = CmdType::PlaceBuilding;
    c.def = res.v;
    c.x0 = bx;
    c.y0 = by;
    c.x1 = 3;
    c.y1 = 3;
    w.push(c);
    note("Reservoir");
    return true;
  }
  // Pipe from beside the reservoir to the building (underground lines go along x, then y).
  const Building& rb = w.buildings[full];
  const Building& db = w.buildings[dry];
  pipe_tried_[dry.raw] = w.now + kTicksPerDay;
  Command c;
  c.type = CmdType::PlaceUnder;
  c.def = 1;
  c.value = 1;
  c.x0 = rb.x + 1;
  c.y0 = rb.y + 3;
  c.x1 = db.x;
  c.y1 = db.y;
  w.push(c);
  note("pipe to " + w.defs->b(db.def).name);
  return true;
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
  note("roads for a new block");
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
  // About a day of wages, so a bad day never tips the city into debt.
  const i64 wages = w.econ.money.yesterday[static_cast<int>(econ::Money::Wages)];
  const i64 reserve = d.kind == BuildingKind::House ? 50 : 100 + std::min<i64>(1500, wages / 2);
  if (w.econ.treasury < cost + reserve) return false;
  // Materials first: a site whose materials nobody makes would stall. Make (or import) them, then build.
  if (supplying_ < 2)
    for (const auto& c : d.cost_goods) {
      const u32 need = d.kind == BuildingKind::Monument ? (c.amount + 3) / 4 : c.amount;
      if (w.econ.city_stock[c.good.v] >= need) continue;
      ++supplying_;
      const bool acted = supply(w, c.good, c.amount, nx, ny);
      --supplying_;
      return acted;
    }
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
    pending_.push_back(def.v);
    note(d.name);
    ++actions;
    return true;
  };
  const bool anywhere = d.resource != 0 || d.needs_water_adjacent || d.kind == BuildingKind::Farm;
  if (anywhere || painted_block) {
    // Spiral search around the town for a valid spot (resources, water edges, fertile fields).
    i32 best_x = -1, best_y = -1, best_score = -(1 << 30);
    for (i32 r = 0; r < (d.kind == BuildingKind::Farm ? 90 : 110); r += 1)
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

// How much of a good the city sensibly keeps; producing beyond it wastes wages.
static u32 stock_cap(World& w, GoodId g) {
  const u32 pop = w.population();
  return w.defs->goods[g.v].food ? 300 + pop * 2 : 150 + pop / 5;
}

bool Autoplay::build_producer_of(World& w, GoodId g, i32 cx, i32 cy, int depth) {
  if (depth > 3 || !g.valid()) return false;
  if (w.econ.city_stock[g.v] > stock_cap(w, g)) return false;  // enough already
  // A producer exists but its chain is broken: rebuild the missing input first.
  if (producers_of(w, g) > 0)
    for (u16 k = 0; k < w.defs->buildings.size(); ++k) {
      const auto& d = w.defs->buildings[k];
      if (!d.recipe.valid() || count(w, d.id.c_str()) == 0) continue;
      bool makes = false;
      for (const auto& o : w.defs->recipes[d.recipe.v].outputs) makes |= o.good == g;
      if (!makes) continue;
      for (const auto& in : w.defs->recipes[d.recipe.v].inputs)
        if (producers_of(w, in.good) == 0 && w.econ.city_stock[in.good.v] < 20 &&
            build_producer_of(w, in.good, cx, cy, depth + 1))
          return true;
    }
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
      if (w.econ.city_stock[in.good.v] < 20 && producers_of(w, in.good) * 2 < consumers + 1 &&
          build_producer_of(w, in.good, cx, cy, depth + 1))
        return true;
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
          if (d.service == s && w.is_unlocked(BuildingDefId{k}) && d.kind != BuildingKind::Monument && d.w <= 4 &&
              count(w, d.id.c_str()) <
                  (d.cost_denarii > 200 ? 1 + w.population() / 1500
                                        : 2 + w.population() / std::max<u32>(120, d.service_capacity * 3u)))  // only as needed
            if (place(w, BuildingDefId{k}, cx, cy)) return true;
        }
      return false;
    case HousingReq::FoodKinds:
    case HousingReq::Food: {
      // Plenty of food in store but none at home: the house needs a market nearby.
      {
        u32 kinds = 0;
        for (GoodId g : w.food_slots) kinds += w.econ.city_stock[g.v] >= 100 ? 1u : 0u;
        if (kinds >= 2 && count(w, "market") < 3 + w.population() / 80 && place(w, w.defs->building("market"), cx, cy))
          return true;
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
  for (u32 p = 0; p < w.routes.size() && w.population() >= 60; ++p) {
    const auto& rs = w.routes[p];
    if (rs.available && !rs.open && w.econ.treasury > w.defs->partners[p].open_cost + 300) {
      Command c;
      c.type = CmdType::OpenRoute;
      c.def = static_cast<u16>(p);
      w.push(c);
    }
  }
  // Imports are for a purpose: stop once the level is reached or money runs short.
  for (u32 g = 0; g < w.econ.goods; ++g)
    if (w.trade_mode[g] == 1 && (w.econ.city_stock[g] >= w.trade_level[g] || w.econ.treasury < 1500)) {
      Command c;
      c.type = CmdType::SetTrade;
      c.def = static_cast<u16>(g);
      c.value = 0;
      c.x0 = 0;
      w.push(c);
    }
  // Export whatever an open route buys once there is a little stock to spare.
  for (u32 g = 0; g < w.econ.goods; ++g) {
    if (w.trade_mode[g] != 0 || w.econ.city_stock[g] < 60) continue;
    bool bought = false;
    for (u32 p = 0; p < w.routes.size(); ++p)
      for (const auto& ga : w.defs->partners[p].buys) bought |= w.routes[p].open && ga.good.v == g;
    if (!bought) continue;
    Command c;
    c.type = CmdType::SetTrade;
    c.def = static_cast<u16>(g);
    c.value = 2;
    c.x0 = w.defs->goods[g].food ? 100 + static_cast<i32>(w.population()) : 40;
    w.push(c);
  }
}

void Autoplay::step(World& w) {
  pending_.clear();  // last hour's orders have been applied by now
  if (w.now % kTicksPerHour != 25) return;
  // Defence: a standing garrison where raids are expected; when raiders appear, conscript, raise more
  // units (legions where a barracks has the gear, else militia) and march them on the enemy.
  {
    u32 enemies = 0, ours = 0;
    f32 ex = 0, ey = 0;
    for (const auto& u : w.army.units)
      if (u.alive) {
        if (u.side == 1) {
          enemies += u.count;
          ex = u.ax;
          ey = u.ay;
        } else if (u.order != military::Order::Away) {
          ours += u.count;
        }
      }
    if (enemies > 0) last_enemy_ = w.now;
    const u32 garrison = w.mission_number >= 3 && w.population() >= 300 ? w.population() / 40 : 0;
    const u32 want = enemies > 0 ? std::max(garrison, enemies + enemies / 4) : garrison;
    auto set_conscription = [&](u8 v) {
      if (w.policy.conscription == v) return;
      Command c;
      c.type = CmdType::SetPolicy;
      c.def = static_cast<u16>(PolicyId::Conscription);
      c.value = v;
      w.push(c);
    };
    set_conscription(enemies > 0 ? 1 : 0);
    if (ours < want && (enemies > 0 || w.now % (kTicksPerHour * 6) == 25)) {
      BuildingId post, barracks;
      const UnitDefId legion = w.defs->unit("legionary"), militia = w.defs->unit("militia");
      w.buildings.for_each([&](BuildingId id, const Building& b) {
        if (b.state != BState::Active) return;
        const auto& d = w.defs->b(b.def);
        if (d.id == "militia_post" && !post.valid()) post = id;
        if (d.id == "barracks" && !barracks.valid() && legion.valid()) {
          bool gear = b.inv != econ::kNoInv;
          for (const auto& g : w.defs->units[legion.v].gear)
            gear &= w.econ.amt(b.inv, g.good) >= g.amount * w.defs->units[legion.v].size / 2;
          if (gear) barracks = id;
        }
      });
      Command c;
      c.type = CmdType::RaiseUnit;
      if (barracks.valid()) {
        c.def = legion.v;
        c.value = static_cast<i32>(barracks.raw);
        w.push(c);
      } else if (post.valid() && militia.valid()) {
        c.def = militia.v;
        c.value = static_cast<i32>(post.raw);
        w.push(c);
      }
    }
    if (enemies > 0 && ours >= enemies + enemies / 5) {
      // Attack together, only with the advantage; otherwise hold in town (holding units engage what comes near).
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
    } else if (enemies == 0 && ours > garrison + 30 && w.now - last_enemy_ > 2 * kTicksPerDay) {
      // Soldiers cost upkeep: stand down the units beyond the garrison two days after the last raid.
      for (u16 u = military::kMaxUnits; u-- > 0;)
        if (w.army.units[u].alive && w.army.units[u].side == 0 && w.army.units[u].order != military::Order::Away) {
          Command c;
          c.type = CmdType::DisbandUnit;
          c.def = u;
          w.push(c);
          break;
        }
    }
  }
  const u32 hour = hour_of_day(w.now);
  if (hour == 6) {
    do_trade_and_requests(w);
    // Goods piling up unused are wages for nothing: close the outermost producer of the most overstocked
    // good (food counts as one pool), one a day.
    {
      u32 food = 0;
      for (GoodId g : w.food_slots) food += w.econ.city_stock[g.v];
      const u32 pop_now = w.population();
      BuildingId far;
      i32 far_d = -1;
      w.buildings.for_each([&](BuildingId id, const Building& b) {
        const auto& d = w.defs->b(b.def);
        if (b.state != BState::Active || !d.recipe.valid()) return;
        bool surplus = false;
        for (const auto& o : w.defs->recipes[d.recipe.v].outputs) {
          const bool food_good = w.defs->goods[o.good.v].food;
          // Inputs of a workshop count as wanted: only close producers of raw piles nobody uses.
          // Food: only farms close (wharves also feed garum workshops).
          if (food_good ? d.kind == BuildingKind::Farm && food > 400 + pop_now * 3
                        : w.econ.city_stock[o.good.v] > 3 * stock_cap(w, o.good))
            surplus = true;
        }
        for (const auto& o : w.defs->recipes[d.recipe.v].outputs) surplus &= o.good != request_good_;
        const i32 dd = std::abs(b.x - ox_) + std::abs(b.y - oy_);
        if (surplus && dd > far_d) {
          far_d = dd;
          far = id;
        }
      });
      if (far.valid() && w.now >= close_cooldown_ &&
          producers_of(w, w.defs->recipes[w.defs->b(w.buildings[far].def).recipe.v].outputs[0].good) > 1) {
        close_cooldown_ = w.now + 4 * kTicksPerDay;  // stocks take days to respond
        const Building& b = w.buildings[far];
        Command c;
        c.type = CmdType::Demolish;
        c.x0 = b.x;
        c.y0 = b.y;
        c.x1 = b.x + b.w - 1;
        c.y1 = b.y + b.h - 1;
        w.push(c);
        note("closed a surplus " + w.defs->b(b.def).name);
      }
    }
    // Budget: raise plebeian taxes while the treasury shrinks, ease them when rich.
    i64 net = 0;
    for (int k = 0; k < econ::kMoneyCount; ++k)
      net += econ::money_is_income(static_cast<econ::Money>(k)) ? w.econ.money.yesterday[k] : -w.econ.money.yesterday[k];
    Command c;
    c.type = CmdType::SetPolicy;
    c.def = static_cast<u16>(PolicyId::TaxPleb);
    // The annona: free staples for poor households while many are poor.
    {
      u32 hh = 0, poor = 0;
      for (u32 k = 0; k < w.households.ids.high_water(); ++k)
        if (w.households.ids.alive_index(k)) {
          ++hh;
          if (w.households.h[k].savings < 10) ++poor;
        }
      const i32 want = hh > 50 && poor * 10 > hh ? 1 : 0;
      if (w.policy.rations != want && (want == 1 || poor * 20 < hh)) {
        Command rc;
        rc.type = CmdType::SetPolicy;
        rc.def = static_cast<u16>(PolicyId::Rations);
        rc.value = want;
        w.push(rc);
      }
    }
    // In debt: pay a little less than Rome's rate until the books balance.
    if (w.econ.treasury < 0 && w.policy.wage > 3) {
      Command wc;
      wc.type = CmdType::SetPolicy;
      wc.def = static_cast<u16>(PolicyId::Wage);
      wc.value = 3;
      w.push(wc);
    } else if (w.econ.treasury > 5000 && w.policy.wage < 4) {
      Command wc;
      wc.type = CmdType::SetPolicy;
      wc.def = static_cast<u16>(PolicyId::Wage);
      wc.value = 4;
      w.push(wc);
    }
    if (w.econ.treasury < 3000 && net < 0 && w.policy.tax_rate[2] < (w.econ.treasury < 0 ? 20 : 14)) {
      c.value = w.policy.tax_rate[2] + 2;
      w.push(c);
    } else if (w.econ.treasury > 8000 && w.policy.tax_rate[2] > 6) {
      c.value = w.policy.tax_rate[2] - 1;
      w.push(c);
    }
  }
  // Up to three actions an hour (a player clicks more than once an hour).
  for (int k = 0; k < 3; ++k) {
    const u32 before = noted_;
    act(w);
    if (noted_ == before) {
      if (k == 0) ++idle_hours_[idle_];
      break;
    }
  }
}

void Autoplay::act(World& w) {
  idle_ = "other";
  // Clear ruins so the lots can be rebuilt.
  {
    BuildingId ruin;
    w.buildings.for_each([&](BuildingId id, const Building& b) {
      if (!ruin.valid() && b.state == BState::Ruin) ruin = id;
    });
    if (ruin.valid()) {
      const Building& b = w.buildings[ruin];
      Command c;
      c.type = CmdType::Demolish;
      c.x0 = b.x;
      c.y0 = b.y;
      c.x1 = b.x + b.w - 1;
      c.y1 = b.y + b.h - 1;
      w.push(c);
      note("cleared ruins");
      return;
    }
  }
  if (w.econ.treasury < 120) {
    idle_ = "broke";
    return;
  }
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
  const bool jobless = adults > 40 && idle * 100 / adults > 15;
  // 1. Always keep some vacant housing (unless jobs are what the city lacks).
  // Past the population goal, improve the houses there are rather than sprawl.
  const bool enough = w.goals.population > 0 && pop >= w.goals.population + w.goals.population / 10;
  if (houses < 6 || (cap - std::min(cap, residents) < std::max<u32>(15, pop / 8) && !(adults > 40 && idle * 100 / adults > 20) && !enough)) {
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
      water_cooldown_ = w.now + kTicksPerHour;  // let the service map catch up
      for (u32 k = 0; k < ndry; ++k)
        if (place(w, w.defs->building("well"), dry[k].first, dry[k].second, false, 4)) return;
    }
  }
  if (plumb(w)) return;
  // Fire watch: vigiles near any building whose fire risk is climbing without a patrol.
  if (fire_cooldown_ <= w.now) {
    i32 fx = -1, fy = -1;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      if (fx < 0 && b.state == BState::Active && b.fire_risk > 100 && !w.patrol_covers("vigiles", b)) {
        fx = b.x;
        fy = b.y;
      }
    });
    if (fx >= 0) {
      fire_cooldown_ = w.now + kTicksPerHour * 4;
      if (place(w, w.defs->building("vigiles_post"), fx, fy, false, 8)) return;
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
      // Hopeless (materials the city does not have, or ready but nobody can reach it) after four days;
      // otherwise far sites get time for their deliveries.
      bool hopeless = w.site_ready(b) && b.employed == 0;
      for (const auto& c : sd.cost_goods)
        if (w.econ.city_stock[c.good.v] + (b.inv != econ::kNoInv ? w.econ.amt(b.inv, c.good) : 0u) < c.amount) hopeless = true;
      if (!fresh && w.now - it->second > (hopeless ? 4 : 12) * kTicksPerDay) clear = id;
    } else {
      stuck_since_.erase(id.raw);
    }
    if (sd.build_work > 20 && w.site_ready(b)) ++sites;  // sites waiting for materials hire no builders
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
    note("cleared a stalled site");
    return;
  }
  if (sites >= 2 + pop / 400) {
    idle_ = "site limit";
    return;
  }
  // 2. Basics in order.
  auto ensure = [&](const char* id, u32 want) {
    BuildingDefId d = w.defs->building(id);
    if (!d.valid() || !w.is_unlocked(d) || count(w, id) >= want) return false;
    return place(w, d, bx, by);
  };
  if (pop >= 5 && ensure("lumber_camp", 1)) return;
  // Wood builds everything: more camps while it runs short.
  if (pop >= 100 && w.econ.city_stock[w.defs->good("wood").v] < 40 && ensure("lumber_camp", 2 + pop / 400)) return;
  if (pop >= 8 && ensure("well", 1 + pop / 60)) return;
  u32 food_stock = 0;
  for (GoodId g : w.food_slots) food_stock += w.econ.city_stock[g.v];
  auto make_jobs = [&]() -> bool {

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
          if (uses && !r.outputs.empty() && producers_of(w, r.outputs[0].good) < 2 + pop / 500 &&
              build_producer_of(w, r.outputs[0].good, bx, by, 0))
            return true;
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
    if (best.valid() && build_producer_of(w, best, bx, by, 0)) return true;
    if (build_producer_of(w, w.defs->good("pottery"), bx, by, 0)) return true;
    return food_stock < pop && place(w, w.defs->building("wheat_farm"), ox_, oy_);
  };
  if (jobless && idle * 100 / adults > 35 && make_jobs()) return;
  if (pop >= 10 && count(w, "wheat_farm") + count(w, "vegetable_garden") < 1 + pop / 120 && food_stock < 200 + pop) {
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
        if (w.econ.amt(b.inv, o.good) >= 10 && w.econ.city_stock[o.good.v] < stock_cap(w, o.good))
          ++(w.defs->goods[o.good.v].food ? stuck_food : stuck_other);
    });
    // One storage site at a time: a new granary takes a while to open.
    u32 storage_sites = 0;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      if (b.state == BState::Construction && w.defs->b(b.def).kind == BuildingKind::Storage) ++storage_sites;
    });
    for (u16 p : pending_) storage_sites += w.defs->b(BuildingDefId{p}).kind == BuildingKind::Storage ? 1u : 0u;
    if (storage_sites == 0) {
      if (stuck_food >= 2 && place(w, w.defs->building("granary"), bx, by)) return;
      if (stuck_other >= 2 && place(w, w.defs->building("warehouse"), bx, by)) return;
    }
  }
  // Exports are the city's real income: keep about one producer of a sellable good per 150 people,
  // favouring valuable goods partners still want (production must exceed what the markets absorb).
  if (pop >= 150) {
    u32 export_producers = 0;
    GoodId pick{};
    i64 pick_score = 0;
    for (u32 g = 0; g < w.econ.goods; ++g) {
      i64 demand = 0;  // denarii a partner would still pay this year (tenths)
      for (u32 p = 0; p < w.routes.size(); ++p)
        if (w.routes[p].open)
          for (const auto& ga : w.defs->partners[p].buys)
            if (ga.good.v == g && ga.amount > w.routes[p].sold[g])
              demand += static_cast<i64>(ga.amount - w.routes[p].sold[g]) * w.trade_price(p, ga.good, false);
      if (demand == 0 || w.defs->goods[g].food) continue;  // food exports come from the farms' surplus
      const GoodId gid{static_cast<u16>(g)};
      const u32 n = producers_of(w, gid);
      export_producers += n;
      if (w.econ.city_stock[g] >= stock_cap(w, gid)) continue;
      bool makeable = false;  // something unlocked makes it
      for (u16 k = 0; k < w.defs->buildings.size() && !makeable; ++k) {
        const auto& d = w.defs->buildings[k];
        if (!d.recipe.valid() || !w.is_unlocked(BuildingDefId{k})) continue;
        for (const auto& o : w.defs->recipes[d.recipe.v].outputs) makeable |= o.good.v == g;
      }
      const i64 score = demand / (1 + static_cast<i64>(n));
      if (makeable && score > pick_score) {
        pick_score = score;
        pick = gid;
      }
    }
    if (pick.valid() && export_producers < pop / 150 && build_producer_of(w, pick, bx, by, 0)) return;
  }
  // Rome's request: at least two producers of the requested good.
  if (request_good_.valid() && producers_of(w, request_good_) < 2 && build_producer_of(w, request_good_, bx, by, 0)) return;
  // Exports go out through a trade post.
  {
    bool land = false, sea = false;
    for (u32 p = 0; p < w.routes.size(); ++p)
      if (w.routes[p].open) (w.defs->partners[p].sea ? sea : land) = true;
    if (land && ensure("trade_post", 1)) return;
    if (sea && pop >= 300 && ensure("harbor", 1)) return;
  }
  // Saving up: past half the population goal with a grand goal building still missing, spend on nothing else.
  // Only while income actually accumulates and the target is about ten days away.
  bool saving = false;
  i64 net = 0;
  for (int k = 0; k < econ::kMoneyCount; ++k)
    if (static_cast<econ::Money>(k) != econ::Money::Construction)
      net += econ::money_is_income(static_cast<econ::Money>(k)) ? w.econ.money.yesterday[k] : -w.econ.money.yesterday[k];
  if (pop >= w.goals.population / 2 && net > 0)
    for (const auto& g : w.goals.buildings) {
      const auto& gd = w.defs->b(g.def);
      if (count(w, gd.id.c_str()) < g.count && w.is_unlocked(g.def) && gd.cost_denarii > 300 &&
          gd.cost_denarii + 3000 - w.econ.treasury < net * 10)
        saving = true;
    }
  if (!saving) {
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
  }
  // 4. Mission goal buildings.
  if (pop >= 100)
    for (const auto& g : w.goals.buildings) {
      if (count(w, w.defs->b(g.def).id.c_str()) >= g.count || !w.is_unlocked(g.def)) continue;
      // Grand buildings wait until the city can carry them.
      const i64 gc = w.defs->b(g.def).cost_denarii;
      if (gc > 300 && (pop < w.goals.population / 3 || w.econ.treasury < gc + gc / 3)) continue;
      bool materials = true;
      for (const auto& c : w.defs->b(g.def).cost_goods)
        if (w.econ.city_stock[c.good.v] < c.amount) {
          materials = false;
          if (producers_of(w, c.good) < 2 && build_producer_of(w, c.good, bx, by, 0)) return;
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
  if (saving) {
    idle_ = "saving";
    return;
  }
  // 5. Culture and comfort as the city grows.
  if (pop >= 150 && ensure("tavern", pop / 300)) return;
  if (pop >= 150 && ensure("physician", pop / 300)) return;
  if (pop >= 200 && ensure("latrina", pop / 250)) return;
  if (pop >= 250 && ensure("temple", pop / 450)) return;
  if (pop >= 300 && ensure("school", pop / 500)) return;
  if (pop >= 400 && ensure("theater", pop / 900)) return;
  if (pop >= 300 && ensure("guard_post", pop / 500)) return;
  // 6. Jobs: if many are idle, add a productive workshop (early when unemployment is severe).
  if (jobless && make_jobs()) return;
  idle_ = jobless ? "no job site found" : "nothing to do";
}

}  // namespace urbs::sim
