// Military: recruitment, upkeep, unit orders, flow-field marching, formations, combat, morale,
// towers, raids and off-map campaigns. Soldiers in the field tick every tick (budgeted separately).
#include <algorithm>
#include <cmath>

#include "core/profile.hpp"
#include "sim/world.hpp"

namespace urbs::sim {

using military::FlowField;
using military::Formation;
using military::kMaxFields;
using military::kMaxUnits;
using military::kNoSoldier;
using military::kNoUnit;
using military::Order;
using military::Stance;
using military::Unit;

namespace {
constexpr u32 kInf = 0xFFFFFFFFu;
constexpr u32 kSiegeCost = 400;  // enemies treat buildings as slow-to-cross obstacles (they attack them)
}  // namespace

// Tile cost for a side's flow field (0 = impassable). Enemies cannot pass gates.
u32 World::field_cost(i32 t, u8 side) const {
  const size_t i = static_cast<size_t>(t);
  if (terrain_water(map.terrain[i]) && map.road[i] != Road::Bridge) return 0;
  const BuildingId b = map.building[i];
  const bool gate = b.valid() && buildings.alive(b) && defs->b(buildings[b].def).id == "gate";
  if (side == 0) return nav.cost[i] ? nav.cost[i] : (gate ? 30 : 0);
  if (gate || (b.valid() && !(map.water[i] & 0x80))) return kSiegeCost;
  return nav.cost[i] ? nav.cost[i] : 0;
}

void World::compute_field(FlowField& f, i32 target, u8 side) {
  URBS_ZONE("flow_field");
  f.target = target;
  f.side = side;
  f.valid = 1;
  f.computed = now;
  std::fill(f.dist.begin(), f.dist.end(), kInf);
  field_heap_.clear();
  f.dist[static_cast<size_t>(target)] = 0;
  field_heap_.push_or_decrease(static_cast<u32>(target), 0);
  while (!field_heap_.empty()) {
    const u32 t = field_heap_.pop();
    const i32 x = static_cast<i32>(t) % map.w, y = static_cast<i32>(t) / map.w;
    const u32 ct = field_cost(static_cast<i32>(t), side);
    for (int d = 0; d < 8; ++d) {
      const i32 nx = x + kDir8X[d], ny = y + kDir8Y[d];
      if (!map.in_bounds(nx, ny)) continue;
      const i32 nt = map.idx(nx, ny);
      const u32 cn = field_cost(nt, side);
      if (cn == 0) continue;
      if (d & 1) {  // no diagonal corner cutting
        if (!field_cost(map.idx(x + kDir8X[d], y), side) || !field_cost(map.idx(x, y + kDir8Y[d]), side)) continue;
      }
      const u32 step = (d & 1) ? (ct + cn) * 7 / 10 : (ct + cn) / 2;
      const u32 nd = f.dist[t] + std::max<u32>(1, step);
      if (nd < f.dist[static_cast<size_t>(nt)]) {
        f.dist[static_cast<size_t>(nt)] = nd;
        field_heap_.push_or_decrease(static_cast<u32>(nt), nd);
      }
    }
  }
}

i16 World::flow_field(i32 target, u8 side) {
  i16 lru = 0;
  for (i16 k = 0; k < kMaxFields; ++k) {
    FlowField& f = army.fields[k];
    if (f.valid && f.target == target && f.side == side && f.computed >= nav_changed_tick_) {
      f.last_used = now;
      return k;
    }
    if (!f.valid || f.last_used < army.fields[lru].last_used) lru = k;
  }
  compute_field(army.fields[lru], target, side);
  army.fields[lru].last_used = now;
  return lru;
}

i32 World::city_target_tile() const {
  // The heart of the city: forum or governor's palace, else the first house.
  BuildingId best;
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.state != BState::Active) return;
    const auto& d = defs->b(b.def);
    if (d.id == "governor_palace" || (d.id == "forum" && !best.valid())) best = id;
  });
  if (!best.valid())
    buildings.for_each([&](BuildingId id, const Building& b) {
      if (!best.valid() && b.state == BState::Active && defs->b(b.def).kind == data::BuildingKind::House) best = id;
    });
  return best.valid() ? buildings[best].entrance : map.idx(map.w / 2, map.h / 2);
}

u16 World::spawn_unit(UnitDefId def, u8 side, f32 x, f32 y, u16 count, BuildingId home) {
  u16 u = kNoUnit;
  for (u16 k = 0; k < kMaxUnits; ++k)
    if (!army.units[k].alive) {
      u = k;
      break;
    }
  if (u == kNoUnit) return kNoUnit;
  const auto& ud = defs->units[def.v];
  Unit& un = army.units[u];
  un = Unit{};
  un.alive = 1;
  un.side = side;
  un.def = def;
  un.ax = x;
  un.ay = y;
  un.home = home;
  un.morale = ud.morale;
  un.formation = ud.ranged() ? Formation::Loose : Formation::Line;
  un.number = side == 0 ? army.next_number++ : 0;
  if (side == 0 && tech(Tech::LegionDrill)) {
    un.exp = 30;
    un.morale = static_cast<u8>(std::min(100, un.morale + 10));
  }
  if (effect(Effect::Morale) && side == 0) un.morale = static_cast<u8>(std::min(100, un.morale + 20));
  for (u16 s = 0; s < count; ++s) {
    SoldierId id = army.ids.create();
    if (!id) break;
    const u32 i = id.index();
    army.x[i] = army.px[i] = x + static_cast<f32>(s % 8) * 0.5f - 2.0f;
    army.y[i] = army.py[i] = y + static_cast<f32>(s / 8) * 0.5f;
    army.hp[i] = static_cast<i16>(ud.hp);
    army.unit[i] = u;
    army.slot[i] = s;
    army.cooldown[i] = static_cast<u16>(s % 20);
    army.target[i] = kNoSoldier;
    army.walked[i] = 0;
    army.attacking[i] = 0;
    ++un.count;
    ++army.deployed;
  }
  un.initial = un.count;
  return u;
}

bool World::raise_unit(UnitDefId def, BuildingId barracks) {
  if (!def.valid() || def.v >= defs->units.size() || !buildings.alive(barracks)) return false;
  const auto& ud = defs->units[def.v];
  if (ud.enemy) return false;
  Building& b = buildings[barracks];
  if (b.state != BState::Active || defs->b(b.def).kind != data::BuildingKind::Military || b.entrance < 0) return false;
  if (!ud.trained_at.empty() &&
      std::find(ud.trained_at.begin(), ud.trained_at.end(), defs->b(b.def).id) == ud.trained_at.end()) {
    add_alert(AlertKind::Event, barracks, 1102);  // this building cannot train that unit
    return false;
  }
  // Gear must be in the barracks.
  u16 n = ud.size;
  for (const auto& g : ud.gear) {
    const u16 have = b.inv == econ::kNoInv ? 0 : econ.amt(b.inv, g.good);
    n = std::min<u16>(n, static_cast<u16>(have / std::max<u16>(1, g.amount)));
  }
  if (n < ud.size / 2) {
    add_alert(AlertKind::Event, barracks, 1100);  // not enough equipment
    return false;
  }
  // Volunteers: adult plebeians and liberti; conscription widens the pool to the employed.
  auto& C = citizens;
  u16 got = 0;
  for (u32 i = 0; i < C.ids.high_water() && got < n; ++i) {
    if (!C.ids.alive_index(i) || (C.flags[i] & kFlagVisitor)) continue;
    if (C.cls[i] != CitizenClass::Pleb && C.cls[i] != CitizenClass::Libertus) continue;
    if (C.sex[i] != 0) continue;
    const u32 a = C.age_years(i, now);
    if (a < 17 || a > 45) continue;
    if (C.work[i].valid() && policy.conscription == 0) continue;
    ++got;
  }
  if (got < n / 2) {
    add_alert(AlertKind::Event, barracks, 1101);  // not enough recruits
    return false;
  }
  n = got;
  for (const auto& g : ud.gear) econ.take(b.inv, g.good, static_cast<u16>(g.amount * n));
  u16 taken = 0;
  for (u32 i = 0; i < C.ids.high_water() && taken < n; ++i) {
    if (!C.ids.alive_index(i) || (C.flags[i] & kFlagVisitor)) continue;
    if (C.cls[i] != CitizenClass::Pleb && C.cls[i] != CitizenClass::Libertus) continue;
    if (C.sex[i] != 0) continue;
    const u32 a = C.age_years(i, now);
    if (a < 17 || a > 45) continue;
    if (C.work[i].valid() && policy.conscription == 0) continue;
    despawn_citizen(C.ids.id_at(i));  // joins the legion
    ++taken;
  }
  const i32 e = b.entrance;
  spawn_unit(def, 0, static_cast<f32>(e % map.w) + 0.5f, static_cast<f32>(e / map.w) + 0.5f, taken, barracks);
  return true;
}

void World::disband_unit(u16 u) {
  if (u >= kMaxUnits || !army.units[u].alive) return;
  Unit& un = army.units[u];
  // Veterans return to civilian life if there is housing.
  for (u32 i = 0; i < army.ids.high_water(); ++i) {
    if (!army.ids.alive_index(i) || army.unit[i] != u) continue;
    if (un.side == 0) {
      const i32 here = map.idx(static_cast<i32>(army.x[i]), static_cast<i32>(army.y[i]));
      BuildingId h = find_vacant_house(here, 1, 0);
      if (h.valid()) {
        HouseholdId hh = create_household(h, CitizenClass::Pleb);
        if (hh) {
          households[hh].savings = 40;
          spawn_citizen(CitizenClass::Pleb, h, hh, 30, here);
        }
      }
    }
    army.ids.destroy(army.ids.id_at(i));
    if (army.deployed > 0) --army.deployed;
  }
  un.alive = 0;
  un.count = 0;
}

void World::order_unit(u16 u, Order o, i32 tile) {
  if (u >= kMaxUnits || !army.units[u].alive) return;
  Unit& un = army.units[u];
  if (un.order == Order::Away) return;
  un.order = o;
  un.target_tile = tile;
  un.field = -1;
  if (o == Order::Return && un.home.valid() && buildings.alive(un.home)) un.target_tile = buildings[un.home].entrance;
  if (o == Order::Campaign || (o == Order::Rout && un.side == 1)) un.target_tile = nearest_edge_tile(map.idx(static_cast<i32>(un.ax), static_cast<i32>(un.ay)));
  if (un.target_tile >= 0) un.field = flow_field(un.target_tile, un.side);
}

i32 World::nearest_edge_tile(i32 from) const {
  const i32 x = from % map.w, y = from / map.w;
  const i32 dl = x, dr = map.w - 1 - x, du = y, dd = map.h - 1 - y;
  const i32 m = std::min({dl, dr, du, dd});
  if (m == dl) return map.idx(0, y);
  if (m == dr) return map.idx(map.w - 1, y);
  if (m == du) return map.idx(x, 0);
  return map.idx(x, map.h - 1);
}

void World::spawn_raid(UnitDefId def, u16 units, i32 side_hint) {
  if (!def.valid()) def = defs->unit("barbarian_warrior");
  if (!def.valid()) return;
  rng::Stream& r = stream(rng::StreamId::Military);
  const i32 side = side_hint >= 0 ? side_hint % 4 : static_cast<i32>(r.below(4));
  const i32 target = city_target_tile();
  // Difficulty: raids are smaller on Otium (x0.6) and larger on Imperium (x1.4).
  units = static_cast<u16>(std::max(1, static_cast<int>(static_cast<f32>(units) * (difficulty == 0 ? 0.6f : difficulty == 2 ? 1.4f : 1.0f) + 0.5f)));
  for (u16 k = 0; k < units; ++k) {
    // Find a walkable edge tile on that side.
    i32 tile = -1;
    for (int tries = 0; tries < 200 && tile < 0; ++tries) {
      const i32 along = static_cast<i32>(r.below(static_cast<u32>(side % 2 == 0 ? map.w : map.h)));
      const i32 x = side == 1 ? map.w - 2 : side == 3 ? 1 : along;
      const i32 y = side == 0 ? 1 : side == 2 ? map.h - 2 : along;
      if (field_cost(map.idx(x, y), 1) != 0 && field_cost(map.idx(x, y), 1) != kSiegeCost) tile = map.idx(x, y);
    }
    if (tile < 0) continue;
    const auto& ud = defs->units[def.v];
    const u16 u = spawn_unit(def, 1, static_cast<f32>(tile % map.w) + 0.5f, static_cast<f32>(tile / map.w) + 0.5f, ud.size,
                             BuildingId{});
    if (u == kNoUnit) break;
    order_unit(u, Order::AttackCity, target);
  }
  add_alert(AlertKind::Raid, BuildingId{}, units);
}

namespace {
// Formation slot offset (in tiles) for soldier `s` of `n`, facing +y.
void slot_offset(Formation f, u16 s, u16 n, f32& ox, f32& oy) {
  switch (f) {
    case Formation::Line: {
      const u16 per_row = std::max<u16>(8, static_cast<u16>(n / 4));
      ox = (static_cast<f32>(s % per_row) - static_cast<f32>(per_row) / 2) * 0.7f;
      oy = -static_cast<f32>(s / per_row) * 0.7f;
      break;
    }
    case Formation::Wedge: {
      u16 row = 0, left = s;
      while (left > row) {
        left = static_cast<u16>(left - (row + 1));
        ++row;
      }
      ox = (static_cast<f32>(left) - static_cast<f32>(row) / 2) * 0.7f;
      oy = -static_cast<f32>(row) * 0.6f;
      break;
    }
    case Formation::Testudo: {
      const u16 side = static_cast<u16>(std::max(2.0, std::ceil(std::sqrt(static_cast<double>(n)))));
      ox = (static_cast<f32>(s % side) - static_cast<f32>(side) / 2) * 0.5f;
      oy = -static_cast<f32>(s / side) * 0.5f;
      break;
    }
    default: {
      const u16 per_row = std::max<u16>(6, static_cast<u16>(n / 3));
      ox = (static_cast<f32>(s % per_row) - static_cast<f32>(per_row) / 2) * 1.3f;
      oy = -static_cast<f32>(s / per_row) * 1.3f;
      break;
    }
  }
}
}  // namespace

void World::military_tick() {
  if (army.deployed == 0) return;
  URBS_ZONE("military");
  auto& A = army;
  for (u32 i = 0; i < A.ids.high_water(); ++i) {
    A.px[i] = A.x[i];
    A.py[i] = A.y[i];
  }
  A.rebuild_hash();
  rng::Stream& r = stream(rng::StreamId::Military);

  // 1. Unit anchors: march along their flow field.
  for (u16 u = 0; u < kMaxUnits; ++u) {
    Unit& un = A.units[u];
    if (!un.alive || un.order == Order::Away) continue;
    if (un.count == 0) {
      un.alive = 0;
      continue;
    }
    const auto& ud = defs->units[un.def.v];
    if (un.target_tile >= 0) {
      if (un.field < 0 || A.fields[un.field].target != un.target_tile || A.fields[un.field].side != un.side ||
          !A.fields[un.field].valid)
        un.field = flow_field(un.target_tile, un.side);
      FlowField& ff = A.fields[un.field];
      ff.last_used = now;
      const i32 tx = static_cast<i32>(un.ax), ty = static_cast<i32>(un.ay);
      const i32 t = map.idx(std::clamp(tx, 0, map.w - 1), std::clamp(ty, 0, map.h - 1));
      if (t == un.target_tile || ff.dist[static_cast<size_t>(t)] == 0) {
        if (un.order == Order::Campaign) {
          un.order = Order::Away;
          un.away_until = now + kTicksPerDay * 8;
          // Soldiers leave the map; they are kept in the unit's count.
          for (u32 i = 0; i < A.ids.high_water(); ++i)
            if (A.ids.alive_index(i) && A.unit[i] == u) {
              A.ids.destroy(A.ids.id_at(i));
              if (A.deployed > 0) --A.deployed;
            }
          continue;
        }
        if (un.side == 1 && un.order == Order::Rout) {
          for (u32 i = 0; i < A.ids.high_water(); ++i)
            if (A.ids.alive_index(i) && A.unit[i] == u) {
              A.ids.destroy(A.ids.id_at(i));
              if (A.deployed > 0) --A.deployed;
            }
          un.alive = 0;
          continue;
        }
        if (un.side == 1) {
          // Reached the heart of the city: pick a new victim building.
          BuildingId pick;
          u32 seen = 0;
          buildings.for_each([&](BuildingId id, const Building& b) {
            if (b.state != BState::Active) return;
            ++seen;
            if (r.below(seen) == 0) pick = id;
          });
          if (pick.valid()) order_unit(u, Order::AttackCity, buildings[pick].entrance);
          continue;
        }
        un.order = Order::Hold;
        un.field = -1;
      } else if (un.side == 1 && ff.dist[static_cast<size_t>(t)] == kInf) {
        // Raiders with no way into the city (cut off by water or cliffs) give up and leave.
        for (u32 i = 0; i < A.ids.high_water(); ++i)
          if (A.ids.alive_index(i) && A.unit[i] == u) {
            A.ids.destroy(A.ids.id_at(i));
            if (A.deployed > 0) --A.deployed;
          }
        un.alive = 0;
        continue;
      } else {
        // Step toward the lowest neighbour.
        u32 best = ff.dist[static_cast<size_t>(t)];
        i32 bx = tx, by = ty;
        for (int d = 0; d < 8; ++d) {
          const i32 nx = tx + kDir8X[d], ny = ty + kDir8Y[d];
          if (!map.in_bounds(nx, ny)) continue;
          const u32 nd = ff.dist[static_cast<size_t>(map.idx(nx, ny))];
          if (nd < best) {
            best = nd;
            bx = nx;
            by = ny;
          }
        }
        const f32 gx = static_cast<f32>(bx) + 0.5f - un.ax, gy = static_cast<f32>(by) + 0.5f - un.ay;
        const f32 len = std::sqrt(gx * gx + gy * gy);
        if (len > 0.001f) {
          const f32 sp = ud.speed * (un.order == Order::Rout ? 1.3f : 1.0f);
          // Building in the way (enemies): stop and let the soldiers attack it.
          const bool blocked = un.side == 1 && field_cost(map.idx(bx, by), 1) == kSiegeCost;
          if (!blocked) {
            un.ax += gx / len * std::min(sp, len);
            un.ay += gy / len * std::min(sp, len);
          }
          un.fx = gx / len;
          un.fy = gy / len;
        }
      }
    }
    // Aggressive units holding position advance as a body on enemies that come near.
    if (un.side == 0 && un.order == Order::Hold && un.stance == Stance::Aggressive && (now + u) % 5 == 0) {
      f32 best = 16.0f * 16.0f, bx = 0, by = 0;
      bool found = false;
      const i32 cx = static_cast<i32>(un.ax) / military::Army::kCell, cy = static_cast<i32>(un.ay) / military::Army::kCell;
      for (i32 qy = cy - 4; qy <= cy + 4; ++qy)
        for (i32 qx = cx - 4; qx <= cx + 4; ++qx) {
          if (qx < 0 || qy < 0 || qx >= A.cells_x || qy >= A.cells_y) continue;
          for (u32 j = A.cell_head[static_cast<size_t>(qy * A.cells_x + qx)]; j != kNoSoldier; j = A.next_in_cell[j]) {
            if (A.units[A.unit[j]].side == un.side) continue;
            const f32 dx = A.x[j] - un.ax, dy = A.y[j] - un.ay, d2 = dx * dx + dy * dy;
            if (d2 < best) {
              best = d2;
              bx = A.x[j];
              by = A.y[j];
              found = true;
            }
          }
        }
      if (found && best > 1.0f) {
        const f32 dx = bx - un.ax, dy = by - un.ay, d = std::sqrt(best);
        const f32 nx = un.ax + dx / d * ud.speed * 5.0f, ny = un.ay + dy / d * ud.speed * 5.0f;
        if (map.in_bounds(static_cast<i32>(nx), static_cast<i32>(ny)) &&
            field_cost(map.idx(static_cast<i32>(nx), static_cast<i32>(ny)), 0) != 0) {
          un.ax = nx;
          un.ay = ny;
        }
        un.fx = dx / d;
        un.fy = dy / d;
      }
    }
    // Morale every second.
    if (now % 20 == u % 20) {
      i32 m = un.morale;
      m -= un.losses_window * 300 / std::max<u16>(1, un.initial);
      m += un.kills_window / 2;
      if (un.losses_window == 0) m += un.order == Order::Rout ? 3 : 1;
      // A routed city unit rallies once it has broken contact and steadied.
      if (un.side == 0 && un.order == Order::Rout && un.losses_window == 0 && m >= 40) {
        un.order = Order::Hold;
        un.target_tile = -1;
        un.field = -1;
      }
      if (un.side == 0 && effect(Effect::Desertion) && r.chance(0.05f)) m -= 5;
      un.losses_window = un.kills_window = 0;
      un.morale = static_cast<u8>(std::clamp(m, 0, 100));
      if (un.morale < 15 && un.order != Order::Rout) {
        // Rout: city units fall back home (or simply away), enemies flee the map.
        if (un.side == 0) {
          if (un.home.valid() && buildings.alive(un.home)) order_unit(u, Order::Return, -1);
          un.order = Order::Rout;
        } else {
          order_unit(u, Order::Rout, -1);
        }
      }
    }
    // Homeless routing units back away from the nearest enemy.
    if (un.side == 0 && un.order == Order::Rout && un.target_tile < 0 && (now + u) % 5 == 0) {
      const i32 cx = static_cast<i32>(un.ax) / military::Army::kCell, cy = static_cast<i32>(un.ay) / military::Army::kCell;
      f32 best = 1e9f, bx = 0, by = 0;
      for (i32 qy = cy - 3; qy <= cy + 3; ++qy)
        for (i32 qx = cx - 3; qx <= cx + 3; ++qx) {
          if (qx < 0 || qy < 0 || qx >= A.cells_x || qy >= A.cells_y) continue;
          for (u32 j = A.cell_head[static_cast<size_t>(qy * A.cells_x + qx)]; j != kNoSoldier; j = A.next_in_cell[j]) {
            if (A.units[A.unit[j]].side == un.side) continue;
            const f32 dx = A.x[j] - un.ax, dy = A.y[j] - un.ay;
            if (dx * dx + dy * dy < best) {
              best = dx * dx + dy * dy;
              bx = A.x[j];
              by = A.y[j];
            }
          }
        }
      if (best < 1e9f) {
        const f32 dx = un.ax - bx, dy = un.ay - by, d = std::sqrt(best) + 0.001f;
        const f32 nx = un.ax + dx / d * ud.speed * 5.0f, ny = un.ay + dy / d * ud.speed * 5.0f;
        if (map.in_bounds(static_cast<i32>(nx), static_cast<i32>(ny)) &&
            field_cost(map.idx(static_cast<i32>(nx), static_cast<i32>(ny)), 0) != 0) {
          un.ax = nx;
          un.ay = ny;
        }
      }
    }
  }

  // 2. Soldiers.
  for (u32 i = 0; i < A.ids.high_water(); ++i) {
    if (!A.ids.alive_index(i)) continue;
    const u16 u = A.unit[i];
    Unit& un = A.units[u];
    const auto& ud = defs->units[un.def.v];
    if (A.cooldown[i] > 0) --A.cooldown[i];
    if (A.attacking[i] > 0) --A.attacking[i];
    const bool routing = un.order == Order::Rout;
    // Find a target: keep the current one if alive and near, else search the hash.
    u32 tgt = A.target[i];
    const f32 engage = ud.ranged() ? ud.range + 2.0f : (un.stance == Stance::Aggressive ? 7.0f : 3.0f);
    if (tgt != kNoSoldier && (!A.ids.alive_index(tgt) || A.units[A.unit[tgt]].side == un.side)) tgt = kNoSoldier;
    if (tgt != kNoSoldier) {
      const f32 dx = A.x[tgt] - A.x[i], dy = A.y[tgt] - A.y[i];
      if (dx * dx + dy * dy > engage * engage * 1.5f) tgt = kNoSoldier;
    }
    if (tgt == kNoSoldier && (now + i) % 4 == 0) {
      f32 best = routing ? ud.range * ud.range : engage * engage;  // routed men only fight what reaches them
      const i32 rc = static_cast<i32>(engage) / military::Army::kCell + 1;
      const i32 cx = static_cast<i32>(A.x[i]) / military::Army::kCell, cy = static_cast<i32>(A.y[i]) / military::Army::kCell;
      for (i32 qy = cy - rc; qy <= cy + rc; ++qy)
        for (i32 qx = cx - rc; qx <= cx + rc; ++qx) {
          if (qx < 0 || qy < 0 || qx >= A.cells_x || qy >= A.cells_y) continue;
          for (u32 j = A.cell_head[static_cast<size_t>(qy * A.cells_x + qx)]; j != kNoSoldier; j = A.next_in_cell[j]) {
            if (A.units[A.unit[j]].side == un.side) continue;
            const f32 dx = A.x[j] - A.x[i], dy = A.y[j] - A.y[i];
            const f32 d2 = dx * dx + dy * dy;
            if (d2 < best) {
              best = d2;
              tgt = j;
            }
          }
        }
    }
    A.target[i] = tgt;

    f32 gx = 0, gy = 0;  // desired point
    bool move = true;
    if (tgt != kNoSoldier) {
      const f32 dx = A.x[tgt] - A.x[i], dy = A.y[tgt] - A.y[i];
      const f32 d = std::sqrt(dx * dx + dy * dy);
      if (d <= ud.range) {
        move = false;
        if (A.cooldown[i] == 0) {
          A.cooldown[i] = 20;
          A.attacking[i] = 6;
          const auto& td = defs->units[A.units[A.unit[tgt]].def.v];
          f32 dmg = static_cast<f32>(ud.attack) * (0.8f + 0.4f * r.unit()) *
                    (1.0f + static_cast<f32>(un.exp) / 200.0f) * (0.6f + static_cast<f32>(un.morale) / 250.0f);
          if (A.walked[i] > 3.0f && ud.charge > 1.0f) dmg *= ud.charge;  // charging cavalry
          const auto& tu = A.units[A.unit[tgt]];
          // Testudo shrugs off missiles; drilled lines and testudos hold better than loose bands.
          f32 def_v = static_cast<f32>(td.defense) * (tu.formation == Formation::Testudo && ud.ranged() ? 3.0f : 1.0f);
          if (tu.formation == Formation::Line || tu.formation == Formation::Testudo) def_v *= 1.3f;
          dmg *= 10.0f / (10.0f + def_v);
          if (ud.ranged()) {
            for (auto& p : A.projectiles)
              if (!p.alive) {
                p = military::Projectile{A.x[i], A.y[i] - 0.5f, A.x[tgt], A.y[tgt] - 0.4f, tgt, BuildingId{},
                                         static_cast<u16>(dmg), un.side, 1};
                break;
              }
          } else {
            A.hp[tgt] = static_cast<i16>(A.hp[tgt] - static_cast<i16>(dmg));
          }
          A.walked[i] = 0;
        }
        if (d > 0.01f) A.facing[i] = static_cast<u8>(std::fabs(dx) > std::fabs(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0));
      } else {
        gx = A.x[tgt];
        gy = A.y[tgt];
      }
    } else {
      // Formation slot around the anchor, or follow the field while marching.
      f32 ox, oy;
      slot_offset(un.formation, A.slot[i], un.initial, ox, oy);
      // Rotate offset to the unit facing (offsets are given facing +y).
      // Rotate the offset (given for facing +y) to the unit's facing.
      const f32 rx = un.fy * ox + un.fx * oy, ry = -un.fx * ox + un.fy * oy;
      gx = un.ax + rx;
      gy = un.ay + ry;
      // Enemies stuck at a building attack it.
      if (un.side == 1 && un.field >= 0 && A.cooldown[i] == 0) {
        const i32 sx = static_cast<i32>(A.x[i]), sy = static_cast<i32>(A.y[i]);
        for (int d = 0; d < 8; ++d) {
          const i32 nx = sx + kDir8X[d], ny = sy + kDir8Y[d];
          if (!map.in_bounds(nx, ny)) continue;
          const BuildingId b = map.building[static_cast<size_t>(map.idx(nx, ny))];
          if (!b.valid() || !buildings.alive(b) || buildings[b].state == BState::Ruin) continue;
          Building& bb = buildings[b];
          A.cooldown[i] = 20;
          A.attacking[i] = 6;
          const i32 dmg = std::max(1, static_cast<i32>(static_cast<f32>(ud.attack) * ud.siege / 4.0f));
          bb.damage = static_cast<u8>(std::min(255, bb.damage + dmg));
          if (r.chance(0.01f)) ignite(b);
          if (bb.damage >= 250) {
            ruin(b);
            update_nav_rect(bb.x, bb.y, bb.w, bb.h);
          }
          move = false;
          break;
        }
      }
    }
    if (move) {
      f32 dx = gx - A.x[i], dy = gy - A.y[i];
      const f32 d = std::sqrt(dx * dx + dy * dy);
      if (d > 0.05f) {
        const f32 sp = std::min(d, ud.speed * (routing ? 1.3f : 1.0f) * (d > 3.0f ? 1.15f : 1.0f));
        dx /= d;
        dy /= d;
        // Separation from close neighbours (same cell).
        const i32 c = A.cell_of(A.x[i], A.y[i]);
        f32 sx = 0, sy = 0;
        int seen = 0;
        for (u32 j = A.cell_head[static_cast<size_t>(c)]; j != kNoSoldier && seen < 12; j = A.next_in_cell[j], ++seen) {
          if (j == i) continue;
          const f32 ex = A.x[i] - A.x[j], ey = A.y[i] - A.y[j];
          const f32 e2 = ex * ex + ey * ey;
          if (e2 < 0.25f && e2 > 1e-6f) {
            sx += ex / e2 * 0.02f;
            sy += ey / e2 * 0.02f;
          }
        }
        f32 nx = A.x[i] + dx * sp + std::clamp(sx, -0.05f, 0.05f), ny = A.y[i] + dy * sp + std::clamp(sy, -0.05f, 0.05f);
        // Don't walk into impassable tiles (the field keeps the anchor on walkable ground).
        const i32 tx = static_cast<i32>(nx), ty = static_cast<i32>(ny);
        if (map.in_bounds(tx, ty)) {
          const u32 fc = field_cost(map.idx(tx, ty), un.side);
          if (fc != 0 && fc != kSiegeCost) {
            A.walked[i] += std::fabs(nx - A.x[i]) + std::fabs(ny - A.y[i]);
            A.x[i] = nx;
            A.y[i] = ny;
          } else {
            // Slide along one axis.
            const i32 ax = static_cast<i32>(A.x[i] + dx * sp), ay = static_cast<i32>(A.y[i]);
            if (map.in_bounds(ax, ay) && field_cost(map.idx(ax, ay), un.side) > 0 &&
                field_cost(map.idx(ax, ay), un.side) != kSiegeCost)
              A.x[i] += dx * sp;
            else if (map.in_bounds(static_cast<i32>(A.x[i]), static_cast<i32>(A.y[i] + dy * sp)))
              A.y[i] += dy * sp;
          }
        }
        A.facing[i] = static_cast<u8>(std::fabs(dx) > std::fabs(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0));
      }
    }
  }

  // 3. Projectiles.
  for (auto& p : A.projectiles) {
    if (!p.alive) continue;
    if (p.target != kNoSoldier && A.ids.alive_index(p.target)) {
      p.tx = A.x[p.target];
      p.ty = A.y[p.target] - 0.4f;
    }
    const f32 dx = p.tx - p.x, dy = p.ty - p.y, d = std::sqrt(dx * dx + dy * dy);
    const f32 sp = 0.6f;
    if (d <= sp) {
      if (p.target != kNoSoldier && A.ids.alive_index(p.target)) A.hp[p.target] = static_cast<i16>(A.hp[p.target] - p.damage);
      p.alive = 0;
    } else {
      p.x += dx / d * sp;
      p.y += dy / d * sp;
    }
  }

  // 4. Towers shoot at nearby enemies every second.
  if (now % 20 == 3) {
    buildings.for_each([&](BuildingId, const Building& b) {
      if (b.state != BState::Active || b.employed == 0 || defs->b(b.def).id != "tower") return;
      const f32 bx = static_cast<f32>(b.x) + 1.0f, by = static_cast<f32>(b.y) + 1.0f;
      u32 best = kNoSoldier;
      f32 bd = 10.0f * 10.0f;
      const i32 c0x = static_cast<i32>(bx) / military::Army::kCell, c0y = static_cast<i32>(by) / military::Army::kCell;
      for (i32 qy = c0y - 3; qy <= c0y + 3; ++qy)
        for (i32 qx = c0x - 3; qx <= c0x + 3; ++qx) {
          if (qx < 0 || qy < 0 || qx >= A.cells_x || qy >= A.cells_y) continue;
          for (u32 j = A.cell_head[static_cast<size_t>(qy * A.cells_x + qx)]; j != kNoSoldier; j = A.next_in_cell[j]) {
            if (A.units[A.unit[j]].side != 1) continue;
            const f32 dx = A.x[j] - bx, dy = A.y[j] - by;
            if (dx * dx + dy * dy < bd) {
              bd = dx * dx + dy * dy;
              best = j;
            }
          }
        }
      if (best == kNoSoldier) return;
      for (auto& p : A.projectiles)
        if (!p.alive) {
          p = military::Projectile{bx, by - 1.5f, A.x[best], A.y[best], best, BuildingId{}, 14, 0, 1};
          break;
        }
    });
  }

  // 5. Deaths.
  for (u32 i = 0; i < A.ids.high_water(); ++i) {
    if (!A.ids.alive_index(i) || A.hp[i] > 0) continue;
    Unit& un = A.units[A.unit[i]];
    if (un.count > 0) --un.count;
    ++un.losses_window;
    if (A.target[i] != kNoSoldier && A.ids.alive_index(A.target[i])) ++A.units[A.unit[A.target[i]]].kills_window;
    A.ids.destroy(A.ids.id_at(i));
    if (A.deployed > 0) --A.deployed;
    if (un.side == 0) ++soldiers_lost_;
    else ++enemies_killed_;
  }
}

void World::military_daily() {
  // Upkeep: wages and food for every soldier; hungry units lose morale.
  for (u16 u = 0; u < kMaxUnits; ++u) {
    Unit& un = army.units[u];
    if (!un.alive || un.side != 0) continue;
    const auto& ud = defs->units[un.def.v];
    spend(static_cast<i64>(ud.upkeep) * un.count, econ::Money::Military);
    if (un.order == Order::Away) {
      if (now >= un.away_until) {
        // Return from campaign: outcome by strength, morale and experience vs. difficulty.
        rng::Stream& r = stream(rng::StreamId::Military);
        const f32 strength = static_cast<f32>(un.count) * (1.0f + un.exp / 100.0f) * (0.5f + un.morale / 100.0f);
        const f32 need = 30.0f * static_cast<f32>(un.campaign_level);
        const bool won = r.unit() * strength > r.unit() * need;
        const f32 loss = won ? 0.1f + 0.1f * r.unit() : 0.3f + 0.3f * r.unit();
        const u16 survivors = static_cast<u16>(static_cast<f32>(un.count) * (1.0f - loss));
        const i32 edge = nearest_edge_tile(city_target_tile());
        const UnitDefId def = un.def;
        const BuildingId home = un.home;
        const u8 exp = static_cast<u8>(std::min(100, un.exp + 15));
        un.alive = 0;
        const u16 nu = spawn_unit(def, 0, static_cast<f32>(edge % map.w) + 0.5f, static_cast<f32>(edge / map.w) + 0.5f,
                                  survivors, home);
        if (nu != kNoUnit) {
          army.units[nu].exp = exp;
          order_unit(nu, Order::Return, -1);
        }
        if (won) {
          earn(500 * un.campaign_level, econ::Money::Tribute);
          set_effect(Effect::Favor, kTicksPerDay * 4);
          buy_captives_ = static_cast<u16>(buy_captives_ + 4 * un.campaign_level);
        }
        add_alert(AlertKind::Event, BuildingId{}, static_cast<u16>(won ? 1200 : 1201));
      }
      continue;
    }
    // Food from the home barracks.
    if (un.home.valid() && buildings.alive(un.home) && buildings[un.home].inv != econ::kNoInv) {
      const u32 inv = buildings[un.home].inv;
      u32 need = (un.count + 1) / 2;
      for (GoodId g : food_slots) {
        const u16 take = econ.take(inv, g, static_cast<u16>(std::min<u32>(need, 60000)));
        need -= take;
        econ.consumed_today[g.v] += take;
        if (need == 0) break;
      }
      if (need > 0) un.morale = static_cast<u8>(std::max(0, un.morale - 10));
    }
    // Training grounds raise experience.
    if (un.order == Order::Hold) {
      bool trained = false;
      buildings.for_each([&](BuildingId, const Building& b) {
        if (!trained && b.state == BState::Active && b.employed > 0 && defs->b(b.def).id == "training_ground" &&
            std::abs(b.x - static_cast<i32>(un.ax)) + std::abs(b.y - static_cast<i32>(un.ay)) < 30)
          trained = true;
      });
      un.exp = static_cast<u8>(std::min(100, un.exp + (trained ? 3 : 1)));
    }
  }
  // Captives from campaigns become public slaves (or are sold if there is no room).
  if (buy_captives_ > 0) {
    buy_public_slaves(buy_captives_, false);
    buy_captives_ = 0;
  }
}

}  // namespace urbs::sim
