// Water network (aqueducts with a downhill gradient, reservoirs, pipes), sewers, desirability field.
#include <algorithm>

#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

namespace {
constexpr u8 kAqueductWet = 1, kPiped = 2, kSewerOk = 4, kPassable = 0x80;
constexpr u8 kUnderPipe = 1, kUnderSewer = 2;
}  // namespace

void World::place_underground_line(i32 x0, i32 y0, i32 x1, i32 y1, u8 bit, bool place) {
  i32 x = x0, y = y0;
  for (int n = 0; n < 4096; ++n) {
    if (map.in_bounds(x, y)) {
      u8& u = map.under[static_cast<size_t>(map.idx(x, y))];
      u = place ? static_cast<u8>(u | bit) : static_cast<u8>(u & ~bit);
    }
    if (x == x1 && y == y1) break;
    // Underground lines are 4-connected: step along x first, then y.
    if (x != x1) x += (x1 > x) - (x1 < x);
    else y += (y1 > y) - (y1 < y);
  }
  water_dirty = true;
}

void World::recompute_water() {
  water_dirty = false;
  const size_t n = map.water.size();
  for (size_t i = 0; i < n; ++i) map.water[i] &= kPassable;
  BuildingDefId aq = defs->building("aqueduct"), gaq = defs->building("grand_aqueduct"), res = defs->building("reservoir");
  auto is_aq = [&](i32 t) {
    BuildingId b = map.building[static_cast<size_t>(t)];
    if (!b.valid() || !buildings.alive(b)) return false;
    const Building& bb = buildings[b];
    return bb.state == BState::Active && (bb.def == aq || bb.def == gaq);
  };
  auto is_res = [&](i32 t) {
    BuildingId b = map.building[static_cast<size_t>(t)];
    return b.valid() && buildings.alive(b) && buildings[b].def == res && buildings[b].state == BState::Active;
  };
  auto near_water = [&](i32 x, i32 y) {
    for (int d = 0; d < 4; ++d) {
      i32 nx = x + kDir4X[d], ny = y + kDir4Y[d];
      if (map.in_bounds(nx, ny) && terrain_water(map.terrain[static_cast<size_t>(map.idx(nx, ny))])) return true;
    }
    return false;
  };
  std::vector<i32>& q = water_queue_;
  // 1. Aqueducts and reservoirs, iterated: water flows downhill (height never rises along the flow).
  std::fill(reservoir_full_.begin(), reservoir_full_.end(), u8{0});
  bool changed = true;
  for (int pass = 0; pass < 8 && changed; ++pass) {
    changed = false;
    q.clear();
    for (i32 t = 0; t < map.w * map.h; ++t) {
      if (!is_aq(t) || (map.water[static_cast<size_t>(t)] & kAqueductWet)) continue;
      const i32 x = t % map.w, y = t / map.w;
      bool source = near_water(x, y);
      for (int d = 0; d < 4 && !source; ++d) {
        i32 nx = x + kDir4X[d], ny = y + kDir4Y[d];
        if (!map.in_bounds(nx, ny)) continue;
        BuildingId nb = map.building[static_cast<size_t>(map.idx(nx, ny))];
        if (nb.valid() && is_res(map.idx(nx, ny)) && reservoir_full_[nb.index()] &&
            map.height[static_cast<size_t>(map.idx(nx, ny))] >= map.height[static_cast<size_t>(t)])
          source = true;
      }
      if (source) {
        map.water[static_cast<size_t>(t)] |= kAqueductWet;
        q.push_back(t);
      }
    }
    for (size_t h = 0; h < q.size(); ++h) {
      const i32 t = q[h];
      const i32 x = t % map.w, y = t / map.w;
      for (int d = 0; d < 4; ++d) {
        i32 nx = x + kDir4X[d], ny = y + kDir4Y[d];
        if (!map.in_bounds(nx, ny)) continue;
        const i32 nt = map.idx(nx, ny);
        if ((map.water[static_cast<size_t>(nt)] & kAqueductWet) || !is_aq(nt)) continue;
        if (map.height[static_cast<size_t>(nt)] > map.height[static_cast<size_t>(t)]) continue;  // no uphill flow
        map.water[static_cast<size_t>(nt)] |= kAqueductWet;
        q.push_back(nt);
      }
    }
    // Reservoirs fill from an adjacent wet aqueduct or open water.
    buildings.for_each([&](BuildingId id, const Building& b) {
      if (b.def != res || b.state != BState::Active || reservoir_full_[id.index()]) return;
      bool full = false;
      for (i32 ty = b.y - 1; ty <= b.y + b.h && !full; ++ty)
        for (i32 tx = b.x - 1; tx <= b.x + b.w && !full; ++tx) {
          if (!map.in_bounds(tx, ty)) continue;
          const size_t i = static_cast<size_t>(map.idx(tx, ty));
          if ((map.water[i] & kAqueductWet) || terrain_water(map.terrain[i])) full = true;
        }
      if (full) {
        reservoir_full_[id.index()] = 1;
        changed = true;
      }
    });
  }
  // 2. Pipes: pressurised from any full reservoir, regardless of height.
  q.clear();
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.def != res || !reservoir_full_[id.index()]) return;
    for (i32 ty = b.y - 1; ty <= b.y + b.h; ++ty)
      for (i32 tx = b.x - 1; tx <= b.x + b.w; ++tx) {
        if (!map.in_bounds(tx, ty)) continue;
        const i32 t = map.idx(tx, ty);
        if ((map.under[static_cast<size_t>(t)] & kUnderPipe) && !(map.water[static_cast<size_t>(t)] & kPiped)) {
          map.water[static_cast<size_t>(t)] |= kPiped;
          q.push_back(t);
        }
      }
  });
  for (size_t h = 0; h < q.size(); ++h) {
    const i32 t = q[h];
    for (int d = 0; d < 4; ++d) {
      i32 nx = t % map.w + kDir4X[d], ny = t / map.w + kDir4Y[d];
      if (!map.in_bounds(nx, ny)) continue;
      const i32 nt = map.idx(nx, ny);
      if (!(map.under[static_cast<size_t>(nt)] & kUnderPipe) || (map.water[static_cast<size_t>(nt)] & kPiped)) continue;
      map.water[static_cast<size_t>(nt)] |= kPiped;
      q.push_back(nt);
    }
  }
  // 3. Sewers drain if connected to open water.
  q.clear();
  for (i32 t = 0; t < map.w * map.h; ++t)
    if ((map.under[static_cast<size_t>(t)] & kUnderSewer) && near_water(t % map.w, t / map.w)) {
      map.water[static_cast<size_t>(t)] |= kSewerOk;
      q.push_back(t);
    }
  for (size_t h = 0; h < q.size(); ++h) {
    const i32 t = q[h];
    for (int d = 0; d < 4; ++d) {
      i32 nx = t % map.w + kDir4X[d], ny = t / map.w + kDir4Y[d];
      if (!map.in_bounds(nx, ny)) continue;
      const i32 nt = map.idx(nx, ny);
      if (!(map.under[static_cast<size_t>(nt)] & kUnderSewer) || (map.water[static_cast<size_t>(nt)] & kSewerOk)) continue;
      map.water[static_cast<size_t>(nt)] |= kSewerOk;
      q.push_back(nt);
    }
  }
  // 4. Building flags: bit 0x20 piped water, 0x10 sewer connection (any tile of the footprint or its ring).
  buildings.for_each([&](BuildingId, Building& b) {
    u8 f = static_cast<u8>(b.flags & ~0x30);
    for (i32 ty = b.y - 1; ty <= b.y + b.h; ++ty)
      for (i32 tx = b.x - 1; tx <= b.x + b.w; ++tx) {
        if (!map.in_bounds(tx, ty)) continue;
        const u8 wv = map.water[static_cast<size_t>(map.idx(tx, ty))];
        if (wv & kPiped) f |= 0x20;
        if (wv & kSewerOk) f |= 0x10;
      }
    b.flags = f;
  });
  providers_dirty_ = true;
}

void World::recompute_desirability() {
  desir_dirty = false;
  std::vector<i16>& acc = desir_acc_;
  std::fill(acc.begin(), acc.end(), i16{0});
  // Terrain: water views and woods are pleasant, marsh is not.
  for (i32 y = 0; y < map.h; ++y)
    for (i32 x = 0; x < map.w; ++x) {
      const Terrain t = map.terrain[static_cast<size_t>(map.idx(x, y))];
      i16 v = 0;
      if (t == Terrain::Marsh) v = -3;
      if (t == Terrain::Forest) v = 1;
      if (terrain_water(t)) v = 2;
      if (v == 0) continue;
      for (i32 dy = -1; dy <= 1; ++dy)
        for (i32 dx = -1; dx <= 1; ++dx)
          if (map.in_bounds(x + dx, y + dy)) acc[static_cast<size_t>(map.idx(x + dx, y + dy))] += v;
    }
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state == BState::Construction) return;
    const auto& d = defs->b(b.def);
    i32 v = d.desirability;
    i32 r = d.desirability_range;
    if (d.kind == BuildingKind::House) {
      // Fine houses improve their street a little; crowded insulae and tents do not.
      v = d.house_line == 1 ? 2 + b.tier : (b.tier >= 3 ? -1 : b.tier == 2 ? 1 : 0);
      r = 2;
    }
    if (b.state == BState::Ruin) {
      v = -4;
      r = 2;
    }
    if (v == 0 || r == 0) return;
    for (i32 ty = b.y - r; ty < b.y + b.h + r; ++ty)
      for (i32 tx = b.x - r; tx < b.x + b.w + r; ++tx) {
        if (!map.in_bounds(tx, ty)) continue;
        const i32 dx = tx < b.x ? b.x - tx : (tx >= b.x + b.w ? tx - (b.x + b.w - 1) : 0);
        const i32 dy = ty < b.y ? b.y - ty : (ty >= b.y + b.h ? ty - (b.y + b.h - 1) : 0);
        const i32 dist = std::max(dx, dy);
        if (dist > r) continue;
        acc[static_cast<size_t>(map.idx(tx, ty))] += static_cast<i16>(v * (r - dist + 1) / (r + 1));
      }
  });
  for (size_t i = 0; i < acc.size(); ++i) map.desir[i] = static_cast<i8>(std::clamp<i32>(acc[i], -60, 60));
}

i32 World::building_desirability(const Building& b) const {
  i32 s = 0, n = 0;
  for (i32 ty = b.y - 1; ty <= b.y + b.h; ++ty)
    for (i32 tx = b.x - 1; tx <= b.x + b.w; ++tx)
      if (map.in_bounds(tx, ty)) {
        s += map.desir[static_cast<size_t>(map.idx(tx, ty))];
        ++n;
      }
  return n ? s / n : 0;
}

}  // namespace urbs::sim
