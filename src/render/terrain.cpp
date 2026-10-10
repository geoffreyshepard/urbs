#include "render/terrain.hpp"

#include <algorithm>
#include <string>

namespace urbs::render {

using sim::kTilePx;
using sim::Road;
using sim::Terrain;

namespace {
const char* kCorner[4] = {"ne", "se", "sw", "nw"};
const char* kDir8[8] = {"n", "ne", "e", "se", "s", "sw", "w", "nw"};
const char* kResName[] = {"", "clay", "stone", "iron", "silver", "marble", "limestone", "pozzolana", "fish"};

u32 tint(u32 c, f32 k) {
  auto ch = [&](int sh) {
    f32 v = static_cast<f32>((c >> sh) & 0xFF) * k;
    return static_cast<u32>(std::clamp(v, 0.0f, 255.0f)) << sh;
  };
  return ch(0) | ch(8) | ch(16) | (c & 0xFF000000u);
}

// Smooth value noise over the tile grid: lattice values every `cell` tiles, bilinear in between.
f32 lattice(i32 x, i32 y, u32 seed) {
  u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u + seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return static_cast<f32>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}
f32 value_noise(i32 x, i32 y, i32 cell, u32 seed) {
  const i32 gx = x / cell, gy = y / cell;
  f32 fx = static_cast<f32>(x % cell) / static_cast<f32>(cell), fy = static_cast<f32>(y % cell) / static_cast<f32>(cell);
  fx = fx * fx * (3 - 2 * fx);
  fy = fy * fy * (3 - 2 * fy);
  const f32 a = lattice(gx, gy, seed), b = lattice(gx + 1, gy, seed), c = lattice(gx, gy + 1, seed),
            d = lattice(gx + 1, gy + 1, seed);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

SpriteInstance inst(const Sprite& s, f32 x, f32 y, f32 z, u32 color = kWhite) {
  return SpriteInstance{x, y, z, static_cast<f32>(s.w), static_cast<f32>(s.h), s.u, s.v, s.w, s.h, color};
}
// Anchored sprite: the anchor pixel lands on (wx, wy).
SpriteInstance anchored(const Sprite& s, f32 wx, f32 wy, f32 z, u32 color = kWhite) {
  return inst(s, wx - static_cast<f32>(s.ax), wy - static_cast<f32>(s.ay), z, color);
}
}  // namespace

int terrain_priority(Terrain t) {
  switch (t) {
    case Terrain::Deep: return 0;
    case Terrain::Shallow: return 1;
    case Terrain::Marsh: return 2;
    case Terrain::Sand: return 3;
    case Terrain::Rock: return 4;
    case Terrain::Grass: return 5;
    case Terrain::Fertile: return 6;
    case Terrain::Forest: return 7;
    default: return 0;
  }
}

void TerrainSprites::resolve(const Atlas& a) {
  for (int t = 0; t < kTerrainCount; ++t) {
    std::string tn = sim::terrain_name(static_cast<Terrain>(t));
    for (int v = 0; v < 4; ++v) base[t][v] = a.get("terrain/" + tn + "_" + std::to_string(v));
    if (t != static_cast<int>(Terrain::Deep))  // the lowest priority never overlays
      for (int m = 1; m < 15; ++m) dual[t][m] = a.get("terrain/dg_" + tn + "_" + std::to_string(m));
    base_color[t] = a.average_color(base[t][0]);
  }
  for (int k = 1; k < 15; ++k) hcliff[k] = a.get("terrain/hc_" + std::to_string(k));
  const char* rn[3] = {"", "dirt", "paved"};
  for (int r = 1; r < 3; ++r) {
    road_center[r] = a.get(std::string("road/") + rn[r] + "_c");
    for (int d = 0; d < 8; ++d) road_spoke[r][d] = a.get(std::string("road/") + rn[r] + "_" + kDir8[d]);
    for (int c = 0; c < 4; ++c) road_fill[r][c] = a.get(std::string("road/") + rn[r] + "_fill_" + kCorner[c]);
    road_color[r] = a.average_color(road_center[r]);
  }
  for (int i = 0; i < 6; ++i) tree[i] = a.get("obj/tree_" + std::to_string(i));
  for (int i = 0; i < 3; ++i) {
    bush[i] = a.get("obj/bush_" + std::to_string(i));
    reeds[i] = a.get("obj/reeds_" + std::to_string(i));
    boulder[i] = a.get("obj/boulder_" + std::to_string(i));
  }
  for (int r = 1; r < static_cast<int>(sim::Resource::Count); ++r) res[r] = a.get(std::string("obj/res_") + kResName[r]);
}

void build_chunk(const sim::TileMap& m, const TerrainSprites& ts, int cx, int cy, std::vector<SpriteInstance>& out) {
  const i32 x0 = cx * sim::kChunkTiles, y0 = cy * sim::kChunkTiles;
  const i32 x1 = std::min(m.w, x0 + sim::kChunkTiles), y1 = std::min(m.h, y0 + sim::kChunkTiles);
  const f32 map_px_h = static_cast<f32>(m.h * kTilePx);
  auto R = [&](i32 x, i32 y) { return m.in_bounds(x, y) ? m.road[static_cast<size_t>(m.idx(x, y))] : Road::None; };

  auto Tc = [&](i32 x, i32 y) {
    return m.terrain[static_cast<size_t>(m.idx(std::clamp(x, 0, m.w - 1), std::clamp(y, 0, m.h - 1)))];
  };
  // Terrain: dual-grid cells centred on tile corners (tools/artgen/terrain.py, dual_layers). Cell
  // (x, y) has the centres of tiles (x-1, y-1) .. (x, y) at its corners; this chunk owns the cells
  // whose bottom-right tile it contains, plus the extra row/column past the map's last tile.
  const i32 cx1 = x1 == m.w ? x1 + 1 : x1, cy1 = y1 == m.h ? y1 + 1 : y1;
  for (i32 y = y0; y < cy1; ++y)
    for (i32 x = x0; x < cx1; ++x) {
      const Terrain c[4] = {Tc(x - 1, y - 1), Tc(x, y - 1), Tc(x - 1, y), Tc(x, y)};
      int prio[4], low = 99;
      for (int k = 0; k < 4; ++k) low = std::min(low, prio[k] = terrain_priority(c[k]));
      const f32 px = static_cast<f32>(x * kTilePx - kTilePx / 2), py = static_cast<f32>(y * kTilePx - kTilePx / 2);
      const u8 var = m.variant[static_cast<size_t>(m.idx(std::min(x, m.w - 1), std::min(y, m.h - 1)))];
      int base = 0;
      for (int k = 0; k < 4; ++k)
        if (prio[k] == low) base = k;
      out.push_back(inst(ts.base[static_cast<int>(c[base])][var & 3], px, py, kDepthTerrain));
      // Overlays for each higher terrain present, lowest first.
      for (int p = low + 1; p < kTerrainCount; ++p) {
        int mask = 0, which = -1;
        for (int k = 0; k < 4; ++k)
          if (prio[k] >= p) {
            mask |= 1 << k;
            if (prio[k] == p) which = k;
          }
        if (which < 0) continue;
        out.push_back(inst(ts.dual[static_cast<int>(c[which])][mask], px, py, kDepthTerrain));
      }
      // Height steps: one contour per level crossed inside the cell.
      auto Hc = [&](i32 hx, i32 hy) {
        return m.height[static_cast<size_t>(m.idx(std::clamp(hx, 0, m.w - 1), std::clamp(hy, 0, m.h - 1)))];
      };
      const u8 hh[4] = {Hc(x - 1, y - 1), Hc(x, y - 1), Hc(x - 1, y), Hc(x, y)};
      const u8 hmin = std::min({hh[0], hh[1], hh[2], hh[3]}), hmax = std::max({hh[0], hh[1], hh[2], hh[3]});
      for (int lv = hmin + 1; lv <= hmax; ++lv) {
        int mask = 0;
        for (int k = 0; k < 4; ++k)
          if (hh[k] >= lv) mask |= 1 << k;
        out.push_back(inst(ts.hcliff[mask], px, py, kDepthCliff));
      }
    }

  for (i32 y = y0; y < y1; ++y)
    for (i32 x = x0; x < x1; ++x) {
      const size_t i = static_cast<size_t>(m.idx(x, y));
      const Terrain t = m.terrain[i];
      const u8 var = m.variant[i];
      const f32 px = static_cast<f32>(x * kTilePx), py = static_cast<f32>(y * kTilePx);

      const Road road = m.road[i];
      const bool occupied = m.building[i].valid();
      if (road != Road::None) {
        int ri = road == Road::Paved || road == Road::Bridge ? 2 : 1;
        out.push_back(inst(ts.road_center[ri], px, py, kDepthRoad));
        bool orth[4];
        for (int s = 0; s < 4; ++s) orth[s] = R(x + kDir4X[s], y + kDir4Y[s]) != Road::None;
        for (int d = 0; d < 8; ++d) {
          if (R(x + kDir8X[d], y + kDir8Y[d]) == Road::None) continue;
          if (d & 1) {  // diagonal: only if neither adjacent orthogonal is a road
            int a = d / 2, b = ((d + 1) / 2) % 4;
            if (orth[a] || orth[b]) continue;
          }
          out.push_back(inst(ts.road_spoke[ri][d], px, py, kDepthRoad));
        }
        continue;
      }
      if (occupied) continue;

      // Diagonal road fills: the two side neighbours at a corner are joined by a diagonal spoke.
      {
        static constexpr int kSa[4] = {0, 2, 2, 0}, kSb[4] = {1, 1, 3, 3};  // N/E, S/E, S/W, N/W
        static constexpr int kDx[4] = {1, 1, -1, -1}, kDy[4] = {-1, 1, 1, -1};
        for (int c = 0; c < 4; ++c) {
          Road ra = R(x + kDir4X[kSa[c]], y + kDir4Y[kSa[c]]), rb = R(x + kDir4X[kSb[c]], y + kDir4Y[kSb[c]]);
          if (ra == Road::None || rb == Road::None || R(x + kDx[c], y + kDy[c]) != Road::None) continue;
          int ri = (ra == Road::Dirt && rb == Road::Dirt) ? 1 : 2;
          out.push_back(inst(ts.road_fill[ri][c], px, py, kDepthRoad));
        }
      }

      if (m.resource[i] != sim::Resource::None && !sim::terrain_water(t) && (var & 0x30) != 0)
        out.push_back(inst(ts.res[static_cast<int>(m.resource[i])], px, py, kDepthRoad));

      // Decorations, y-sorted with buildings and agents.
      const f32 ox = static_cast<f32>((var >> 2) & 7) - 3.5f, oy = static_cast<f32>((var >> 5) & 3);
      const f32 foot_x = px + 8 + ox, foot_y = py + 12 + oy;
      const f32 z = depth_for_foot(foot_y, map_px_h);
      switch (t) {
        case Terrain::Forest:
          if (var % 10 < 8) out.push_back(anchored(ts.tree[var % 6], foot_x, foot_y, z));
          break;
        case Terrain::Grass:
          if (var % 23 == 0) out.push_back(anchored(ts.bush[var % 3], foot_x, foot_y, z));
          else if (var % 41 == 1) out.push_back(anchored(ts.tree[var % 6], foot_x, foot_y, z));
          break;
        case Terrain::Marsh:
          if (var % 5 < 2) out.push_back(anchored(ts.reeds[var % 3], foot_x, foot_y, z));
          break;
        case Terrain::Rock:
          if (var % 4 == 0) out.push_back(anchored(ts.boulder[var % 3], foot_x, foot_y, z));
          break;
        default: break;
      }
    }
}

void build_ground_texels(const sim::TileMap& m, i32 x0, i32 y0, i32 x1, i32 y1, std::vector<u32>& out) {
  const i32 w = x1 - x0, h = y1 - y0;
  out.resize(static_cast<size_t>(w * h));
  auto at = [&](i32 x, i32 y) { return static_cast<size_t>(m.idx(std::clamp(x, 0, m.w - 1), std::clamp(y, 0, m.h - 1))); };
  for (i32 y = y0; y < y1; ++y)
    for (i32 x = x0; x < x1; ++x) {
      const size_t i = static_cast<size_t>(m.idx(x, y));
      const Terrain t = m.terrain[i];
      const bool water = sim::terrain_water(t);

      // Wear: strongest next to buildings, lighter along roads, fading over two tiles.
      f32 wear = 0;
      if (!water && m.road[i] == Road::None && !m.building[i].valid()) {
        for (i32 dy = -2; dy <= 2; ++dy)
          for (i32 dx = -2; dx <= 2; ++dx) {
            const i32 dist = std::max(std::abs(dx), std::abs(dy));
            if (dist == 0) continue;
            const size_t j = at(x + dx, y + dy);
            f32 src = 0;
            if (m.building[j].valid()) src = 0.85f;
            else if (m.road[j] == Road::Dirt) src = 0.55f;
            else if (m.road[j] != Road::None) src = 0.40f;
            wear = std::max(wear, src * (dist == 1 ? 1.0f : 0.40f));
          }
        wear *= 0.75f + 0.5f * value_noise(x + 4096, y + 4096, 3, 7);
      } else if (!water && m.building[i].valid()) {
        wear = 0.7f;  // yards and gaps inside a footprint
      }

      // Luminance: height plus two octaves of broad variation.
      f32 lum = 0.91f + 0.0118f * static_cast<f32>(m.height[i]);
      lum *= 0.93f + 0.10f * value_noise(x + 1024, y + 1024, 13, 1) + 0.04f * value_noise(x, y, 5, 2);
      if (water) lum = 1.0f;

      // Vegetation: grass and fertile soil are lush where the noise says so, drier elsewhere.
      f32 veg = 0;
      switch (t) {
        case Terrain::Grass: veg = 0.5f + 0.5f * value_noise(x + 300, y + 900, 9, 3); break;
        case Terrain::Fertile: veg = 0.85f + 0.15f * value_noise(x, y, 7, 4); break;
        case Terrain::Forest: veg = 0.9f; break;
        case Terrain::Marsh: veg = 0.8f; break;
        default: break;
      }

      // Land mask: filtered linearly it ramps from 0 at water tile centres to 0.5 on the coastline.
      const f32 shore = water ? 0.0f : 1.0f;
      auto q = [](f32 v) { return static_cast<u8>(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f)); };
      out[static_cast<size_t>((y - y0) * w + (x - x0))] =
          rgba(q(wear), q(lum * (128.0f / 255.0f)), veg > 0 ? q(veg) : 0, q(shore));
    }
}

void build_tile_colors(const sim::TileMap& m, const TerrainSprites& ts, std::vector<u32>& out) {
  out.resize(static_cast<size_t>(m.w * m.h));
  for (size_t i = 0; i < out.size(); ++i) {
    u32 c = ts.base_color[static_cast<int>(m.terrain[i])];
    if (m.terrain[i] == Terrain::Forest) c = tint(c, 0.8f);
    if (m.road[i] != Road::None) c = ts.road_color[m.road[i] == Road::Dirt ? 1 : 2];
    f32 k = 0.84f + 0.03f * static_cast<f32>(m.height[i]);
    out[i] = tint(c, k);
  }
}

}  // namespace urbs::render
