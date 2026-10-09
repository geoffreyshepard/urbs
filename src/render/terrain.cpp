#include "render/terrain.hpp"

#include <algorithm>
#include <string>

namespace urbs::render {

using sim::kTilePx;
using sim::Road;
using sim::Terrain;

namespace {
const char* kSide[4] = {"n", "e", "s", "w"};
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

u32 height_tint(u8 h) {
  u8 v = static_cast<u8>(std::min(255, 232 + static_cast<int>(h) * 3));
  return rgba(v, v, v);
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
    for (int s = 0; s < 4; ++s) edge[t][s] = a.get("terrain/edge_" + tn + "_" + kSide[s]);
    for (int c = 0; c < 4; ++c) corner[t][c] = a.get("terrain/corner_" + tn + "_" + kCorner[c]);
    base_color[t] = a.average_color(base[t][0]);
  }
  cliff_s = a.get("terrain/cliff_s");
  cliff_e = a.get("terrain/cliff_e");
  cliff_w = a.get("terrain/cliff_w");
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
  auto T = [&](i32 x, i32 y, Terrain fallback) { return m.in_bounds(x, y) ? m.terrain[static_cast<size_t>(m.idx(x, y))] : fallback; };
  auto H = [&](i32 x, i32 y, u8 fallback) { return m.in_bounds(x, y) ? m.height[static_cast<size_t>(m.idx(x, y))] : fallback; };
  auto R = [&](i32 x, i32 y) { return m.in_bounds(x, y) ? m.road[static_cast<size_t>(m.idx(x, y))] : Road::None; };

  for (i32 y = y0; y < y1; ++y)
    for (i32 x = x0; x < x1; ++x) {
      const size_t i = static_cast<size_t>(m.idx(x, y));
      const Terrain t = m.terrain[i];
      const int ti = static_cast<int>(t);
      const u8 var = m.variant[i];
      const u8 h = m.height[i];
      const f32 px = static_cast<f32>(x * kTilePx), py = static_cast<f32>(y * kTilePx);
      const u32 ht = height_tint(h);
      out.push_back(inst(ts.base[ti][var & 3], px, py, kDepthTerrain, ht));

      // Edge blends from higher-priority neighbours, lowest priority first.
      struct Blend {
        int prio;
        const Sprite* s;
      };
      Blend blends[8];
      int nb = 0;
      const int p = terrain_priority(t);
      Terrain side_t[4];
      for (int s = 0; s < 4; ++s) {
        side_t[s] = T(x + kDir4X[s], y + kDir4Y[s], t);
        int np = terrain_priority(side_t[s]);
        if (np > p) blends[nb++] = {np, &ts.edge[static_cast<int>(side_t[s])][s]};
      }
      static constexpr int kCdx[4] = {1, 1, -1, -1}, kCdy[4] = {-1, 1, 1, -1};
      static constexpr int kCs0[4] = {0, 2, 2, 0}, kCs1[4] = {1, 1, 3, 3};  // adjacent orthogonal sides
      for (int c = 0; c < 4; ++c) {
        Terrain d = T(x + kCdx[c], y + kCdy[c], t);
        int dp = terrain_priority(d);
        if (dp > p && side_t[kCs0[c]] != d && side_t[kCs1[c]] != d)
          blends[nb++] = {dp, &ts.corner[static_cast<int>(d)][c]};
      }
      std::sort(blends, blends + nb, [](const Blend& a, const Blend& b) { return a.prio < b.prio; });
      for (int b = 0; b < nb; ++b) out.push_back(inst(*blends[b].s, px, py, kDepthTerrain, ht));

      // Cliffs where a neighbour is lower.
      if (!sim::terrain_water(t)) {
        if (H(x, y + 1, h) < h) out.push_back(inst(ts.cliff_s, px, py, kDepthTerrain));
        // Side faces only for real drops; single-level steps read as noise.
        if (H(x + 1, y, h) + 1 < h) out.push_back(inst(ts.cliff_e, px, py, kDepthTerrain));
        if (H(x - 1, y, h) + 1 < h) out.push_back(inst(ts.cliff_w, px, py, kDepthTerrain));
      }

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
