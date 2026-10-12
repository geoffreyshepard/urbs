#include "render/terrain.hpp"

#include <algorithm>
#include <string>

#include "render/iso.hpp"

namespace urbs::render {

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

// Anchored sprite: the anchor lands on the iso point (x, y).
SpriteInstance anchored(const Sprite& s, f32 x, f32 y, f32 z, u32 color) {
  return SpriteInstance{x - s.iso_ax(), y - s.iso_ay(), z, s.iso_w(), s.iso_h(), s.u, s.v, s.w, s.h, color};
}
SpriteInstance anchored(const Sprite& s, Vec2 p, f32 z, u32 color) { return anchored(s, p.x, p.y, z, color); }
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
    const std::string tn = std::string("terrain/") + sim::terrain_name(static_cast<Terrain>(t));
    for (int p = 0; p < 4; ++p) {
      const std::string ps = "_" + std::to_string(p);
      base[t][p] = a.get(tn + ps);
      for (int k = 0; k < 4; ++k)
        quarter[t][k][p] = a.get(std::string("terrain/q_") + sim::terrain_name(static_cast<Terrain>(t)) + "_" +
                                 std::to_string(k) + ps);
      if (t != static_cast<int>(Terrain::Deep))  // the lowest priority never overlays
        for (int m = 1; m < 15; ++m)
          dual[t][m][p] = a.get(std::string("terrain/dg_") + sim::terrain_name(static_cast<Terrain>(t)) + "_" +
                                std::to_string(m) + ps);
    }
    base_color[t] = a.average_color(base[t][0]);
  }
  for (int v = 0; v < 2; ++v) {
    cliff[0][v] = a.get("terrain/cliff_s_" + std::to_string(v));
    cliff[1][v] = a.get("terrain/cliff_e_" + std::to_string(v));
  }
  const char* rn[3] = {"", "dirt", "paved"};
  for (int r = 1; r < 3; ++r) {
    for (int k = 0; k < 2; ++k) {
      const std::string p = std::string("road/") + rn[r] + (k ? "_k_" : "_");
      // The edge bands are optional (an older atlas has none): find(), not get().
      road_center[r][k] = k ? a.find(p + "c") : a.get(p + "c");
      for (int d = 0; d < 8; ++d) road_spoke[r][d][k] = k ? a.find(p + kDir8[d]) : a.get(p + kDir8[d]);
      for (int c = 0; c < 4; ++c)
        road_fill[r][c][k] = k ? a.find(p + "fill_" + kCorner[c]) : a.get(p + "fill_" + kCorner[c]);
    }
    for (int c = 0; c < 4; ++c) road_quad[r][c] = a.find(std::string("road/") + rn[r] + "_q_" + kCorner[c]);
    road_color[r] = a.average_color(road_center[r][0]);
  }
  auto decor = [&](const std::string& n) { return Decor{a.get(n), a.get(n + "_shadow")}; };
  for (int i = 0; i < 7; ++i) tree[i] = decor("obj/tree_" + std::to_string(i));
  for (int i = 0; i < 3; ++i) {
    bush[i] = decor("obj/bush_" + std::to_string(i));
    reeds[i] = decor("obj/reeds_" + std::to_string(i));
    boulder[i] = decor("obj/boulder_" + std::to_string(i));
  }
  // Meadow decor: newer art, so looked up with find() (an older atlas just has none).
  auto meadow = [&](const std::string& n) { return Decor{a.find(n), a.find(n + "_shadow")}; };
  for (int i = 0; i < 3; ++i) tuft[i] = meadow("obj/tuft_" + std::to_string(i));
  for (int i = 0; i < 4; ++i) flowers[i] = meadow("obj/flowers_" + std::to_string(i));
  for (int i = 0; i < 2; ++i) pebbles[i] = meadow("obj/pebbles_" + std::to_string(i));
  for (int r = 1; r < static_cast<int>(sim::Resource::Count); ++r) res[r] = decor(std::string("obj/res_") + kResName[r]);
}

u32 build_chunk(const sim::TileMap& m, const TerrainSprites& ts, int cx, int cy, std::vector<SpriteInstance>& out) {
  const size_t first = out.size();
  // Decorations are collected here and appended after the ground (render thread only; reused).
  static std::vector<SpriteInstance> decor;
  decor.clear();
  const i32 x0 = cx * sim::kChunkTiles, y0 = cy * sim::kChunkTiles;
  const i32 x1 = std::min(m.w, x0 + sim::kChunkTiles), y1 = std::min(m.h, y0 + sim::kChunkTiles);
  const f32 map_h = iso::map_iso_h(m);
  auto R = [&](i32 x, i32 y) { return m.in_bounds(x, y) ? m.road[static_cast<size_t>(m.idx(x, y))] : Road::None; };
  auto Tc = [&](i32 x, i32 y) {
    return m.terrain[static_cast<size_t>(m.idx(std::clamp(x, 0, m.w - 1), std::clamp(y, 0, m.h - 1)))];
  };
  auto Hc = [&](i32 x, i32 y) {
    return static_cast<int>(m.height[static_cast<size_t>(m.idx(std::clamp(x, 0, m.w - 1), std::clamp(y, 0, m.h - 1)))]);
  };

  // Terrain: dual-grid cells centred on tile corners.  Cell (x, y) has the centres of tiles
  // (x-1, y-1) .. (x, y) at its corners (NW, NE, SW, SE = the diamond's top, right, left, bottom);
  // this chunk owns the cells whose SE tile it contains, plus the extra row/column past the map's
  // last tile.  A cell whose four tiles share one height level draws whole, with blends; a cell
  // across a height step (or on the map edge) draws each tile's quarter on that tile's own level.
  const i32 cx1 = x1 == m.w ? x1 + 1 : x1, cy1 = y1 == m.h ? y1 + 1 : y1;
  for (i32 y = y0; y < cy1; ++y)
    for (i32 x = x0; x < cx1; ++x) {
      static constexpr i32 kQx[4] = {-1, 0, -1, 0}, kQy[4] = {-1, -1, 0, 0};
      const int p = (x & 1) | ((y & 1) << 1);
      bool inb[4], uniform = true;
      int lv[4];
      Terrain c[4];
      for (int k = 0; k < 4; ++k) {
        inb[k] = m.in_bounds(x + kQx[k], y + kQy[k]);
        lv[k] = Hc(x + kQx[k], y + kQy[k]);
        c[k] = Tc(x + kQx[k], y + kQy[k]);
        uniform = uniform && inb[k] && lv[k] == lv[0];
      }
      const f32 fx = static_cast<f32>(x) + 0.5f, fy = static_cast<f32>(y) + 0.5f;
      if (!uniform) {
        for (int k = 0; k < 4; ++k) {
          if (!inb[k]) continue;
          const i32 tx = x + kQx[k], ty = y + kQy[k];
          out.push_back(anchored(ts.quarter[static_cast<int>(c[k])][k][p], iso::from_tile(fx, fy, static_cast<f32>(lv[k])),
                                 ground_depth(tx + ty, kLayerQuarter), ground_code_color(kKindTerrain, lv[k])));
        }
        continue;
      }
      int prio[4], low = 99;
      for (int k = 0; k < 4; ++k) low = std::min(low, prio[k] = terrain_priority(c[k]));
      int base = 0;
      for (int k = 0; k < 4; ++k)
        if (prio[k] == low) base = k;
      const Vec2 at = iso::from_tile(fx, fy, static_cast<f32>(lv[0]));
      const f32 z = ground_depth(x + y - 1, kLayerCell);
      const u32 code = ground_code_color(kKindTerrain, lv[0]);
      out.push_back(anchored(ts.base[static_cast<int>(c[base])][p], at, z, code));
      // Overlays for each higher terrain present, lowest first.
      for (int pr = low + 1; pr < kTerrainCount; ++pr) {
        int mask = 0, which = -1;
        for (int k = 0; k < 4; ++k)
          if (prio[k] >= pr) {
            mask |= 1 << k;
            if (prio[k] == pr) which = k;
          }
        if (which < 0) continue;
        out.push_back(anchored(ts.dual[static_cast<int>(c[which])][mask][p], at, z, ground_code_color(kKindBlend, lv[0])));
      }
    }

  for (i32 y = y0; y < y1; ++y)
    for (i32 x = x0; x < x1; ++x) {
      const size_t i = static_cast<size_t>(m.idx(x, y));
      const Terrain t = m.terrain[i];
      const u8 var = m.variant[i];
      const int lv = static_cast<int>(m.height[i]);
      const Vec2 corner = iso::tile_anchor(x, y, static_cast<f32>(lv));

      // Cliff faces under the south and east edges, one per level down to the neighbour (the map's
      // front edges drop to level 0).
      for (int side = 0; side < 2; ++side) {
        const i32 nx = x + (side == 1 ? 1 : 0), ny = y + (side == 0 ? 1 : 0);
        const int nl = m.in_bounds(nx, ny) ? Hc(nx, ny) : 0;
        for (int l = nl; l < lv; ++l)
          out.push_back(anchored(ts.cliff[side][(x * 7 + y * 3 + l) & 1], iso::tile_anchor(x, y, static_cast<f32>(l)),
                                 ground_depth(x + y, kLayerCliff), ground_code_color(kKindCliff, l)));
      }

      const Road road = m.road[i];
      const bool occupied = m.building[i].valid();
      const f32 zr = ground_depth(x + y + 1, kLayerRoad), zk = ground_depth(x + y + 1, kLayerKerb);
      const u32 road_code = ground_code_color(kKindRoad, lv);
      // One road piece: its edge band one sub-layer under its surface (see TerrainSprites).
      auto piece = [&](const Sprite(&p)[2]) {
        if (p[1].valid) out.push_back(anchored(p[1], corner, zk, road_code));
        out.push_back(anchored(p[0], corner, zr, road_code));
      };
      // Diagonal neighbour d (1 NE, 3 SE, 5 SW, 7 NW) <-> corner index (0 ne, 1 se, 2 sw, 3 nw).
      static constexpr int kSa[4] = {0, 2, 2, 0}, kSb[4] = {1, 1, 3, 3};  // N/E, S/E, S/W, N/W
      static constexpr int kDx[4] = {1, 1, -1, -1}, kDy[4] = {-1, 1, 1, -1};
      if (road != Road::None) {
        int ri = road == Road::Paved || road == Road::Bridge ? 2 : 1;
        piece(ts.road_center[ri]);
        bool orth[4];
        for (int s = 0; s < 4; ++s) orth[s] = R(x + kDir4X[s], y + kDir4Y[s]) != Road::None;
        for (int d = 0; d < 8; ++d) {
          if (R(x + kDir8X[d], y + kDir8Y[d]) == Road::None) continue;
          if (d & 1) {  // diagonal: only if neither adjacent orthogonal is a road
            int a = d / 2, b = ((d + 1) / 2) % 4;
            if (orth[a] || orth[b]) continue;
          }
          piece(ts.road_spoke[ri][d]);
        }
        // Wide streets and squares: fill the corner where both sides and the diagonal are road.
        for (int c = 0; c < 4; ++c)
          if (orth[kSa[c]] && orth[kSb[c]] && R(x + kDx[c], y + kDy[c]) != Road::None && ts.road_quad[ri][c].valid)
            out.push_back(anchored(ts.road_quad[ri][c], corner, zr, road_code));
        continue;
      }
      if (occupied) continue;

      // Diagonal road fills: the two side neighbours at a corner are joined by a diagonal spoke.
      for (int c = 0; c < 4; ++c) {
        Road ra = R(x + kDir4X[kSa[c]], y + kDir4Y[kSa[c]]), rb = R(x + kDir4X[kSb[c]], y + kDir4Y[kSb[c]]);
        if (ra == Road::None || rb == Road::None || R(x + kDx[c], y + kDy[c]) != Road::None) continue;
        int ri = (ra == Road::Dirt && rb == Road::Dirt) ? 1 : 2;
        piece(ts.road_fill[ri][c]);
      }

      // Decorations and resource outcrops, y-sorted with buildings and agents.  The foot is jittered
      // inside the tile by the tile's variant bits.
      auto put = [&](const TerrainSprites::Decor& d, f32 jx, f32 jy) {
        const Vec2 foot = iso::from_tile(static_cast<f32>(x) + 0.5f + jx, static_cast<f32>(y) + 0.5f + jy, static_cast<f32>(lv));
        const u32 code = ground_code_color(kKindDecor, lv);
        if (d.shadow.valid) decor.push_back(anchored(d.shadow, foot, kDepthDecorShadow, code));
        decor.push_back(anchored(d.body, foot, iso::depth_for_iso(foot.y, map_h), code));
      };
      const f32 jx = (static_cast<f32>((var >> 2) & 7) - 3.5f) / 14.0f;
      const f32 jy = (static_cast<f32>((var >> 5) & 3) - 1.5f) / 8.0f;
      // Meadow decor on open grass: tufts, wildflowers and pebbles, placed by a hash of the tile (render
      // only; built-on and road tiles never reach here, so it vanishes with the next chunk rebuild).
      // Tiles beside a road are trodden: sparser, no flowers.
      auto meadow = [&](i32 tx, i32 ty, bool fertile) {
        u32 h = static_cast<u32>(tx) * 0x9E3779B1u ^ static_cast<u32>(ty) * 0x85EBCA77u;
        h = (h ^ (h >> 15)) * 0x2C1B3C6Du;
        h ^= h >> 12;
        bool trodden = false;
        for (int s = 0; s < 4; ++s) {
          const i32 nx = tx + kDir4X[s], ny = ty + kDir4Y[s];
          if (R(nx, ny) != Road::None) trodden = true;
        }
        const int n = static_cast<int>((h & 0xFF) < (trodden ? 90u : 170u)) +
                      static_cast<int>(!trodden && ((h >> 8) & 0xFF) < 80u);
        for (int k = 0; k < n; ++k) {
          const u32 hk = (h >> (k * 7 + 3)) * 0x27D4EB2Du ^ h;
          const u32 pick = (hk >> 4) % 100;
          const TerrainSprites::Decor* d;
          if (pick < (trodden ? 70u : 48u))
            d = &ts.tuft[(hk >> 12) % (fertile ? 2 : 3)];  // fertile: no dry tufts
          else if (!trodden && pick < 86u)
            d = &ts.flowers[(hk >> 14) % 4];
          else
            d = &ts.pebbles[(hk >> 16) % 2];
          if (!d->body.valid) continue;
          const f32 ox = (static_cast<f32>((hk >> 18) & 15) / 15.0f - 0.5f) * 0.7f;
          const f32 oy = (static_cast<f32>((hk >> 22) & 15) / 15.0f - 0.5f) * 0.7f;
          put(*d, ox, oy);
        }
      };
      if (m.resource[i] != sim::Resource::None && !sim::terrain_water(t) && var % 4 == 0) {
        put(ts.res[static_cast<int>(m.resource[i])], jx * 0.5f, jy * 0.5f);
        continue;
      }
      switch (t) {
        case Terrain::Forest: {
          static constexpr int kForest[6] = {0, 1, 3, 5, 4, 1};  // cypress, pine, oak, holm oak
          if (var % 10 < 8) put(ts.tree[kForest[var % 6]], jx, jy);
          if (var % 3 == 0) put(ts.tree[kForest[(var / 7) % 6]], -jy - 0.2f, jx * 0.5f + 0.22f);
          break;
        }
        case Terrain::Grass:
          // Nothing tall on buildable ground: trees grow only where the sim has forest.
          if (var % 23 == 0 || var % 41 == 1)
            put(ts.bush[var % 3], jx, jy);
          else
            meadow(x, y, false);
          break;
        case Terrain::Fertile:
          if (var % 61 == 2)
            put(ts.bush[var % 3], jx, jy);
          else
            meadow(x, y, true);
          break;
        case Terrain::Marsh:
          if (var % 5 < 2) put(ts.reeds[var % 3], jx, jy);
          break;
        case Terrain::Rock:
          if (var % 4 == 0) put(ts.boulder[var % 3], jx, jy);
          break;
        default: break;
      }
    }
  const u32 ground = static_cast<u32>(out.size() - first);
  // Ground back to front: the soft edges of nearer (raised) pieces must blend over what is behind
  // them, not write depth first and leave the background showing through.  Stable, so blends keep
  // following their cell's base.
  std::stable_sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(),
                   [](const SpriteInstance& a, const SpriteInstance& b) { return a.z > b.z; });
  // Shadows first, then bodies far to near, so overlapping translucent edges blend in order.
  std::stable_sort(decor.begin(), decor.end(), [](const SpriteInstance& a, const SpriteInstance& b) { return a.z > b.z; });
  out.insert(out.end(), decor.begin(), decor.end());
  return ground;
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
