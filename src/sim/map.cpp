#include "sim/map.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

#include "core/rng.hpp"

namespace urbs::sim {

void TileMap::resize(i32 width, i32 height_tiles) {
  w = width;
  h = height_tiles;
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  terrain.assign(n, Terrain::Grass);
  height.assign(n, 1);
  groundwater.assign(n, 128);
  fertility.assign(n, 128);
  resource.assign(n, Resource::None);
  road.assign(n, Road::None);
  variant.assign(n, 0);
  building.assign(n, BuildingId{});
  chunk_dirty.assign(static_cast<size_t>(chunks_x() * chunks_y()), 1);
  ++revision;
}

void TileMap::mark_dirty(i32 x, i32 y) {
  // Autotiling reads neighbours, so a change on a chunk border affects the adjacent chunk too.
  for (i32 dy = -1; dy <= 1; ++dy)
    for (i32 dx = -1; dx <= 1; ++dx) {
      i32 tx = x + dx, ty = y + dy;
      if (!in_bounds(tx, ty)) continue;
      chunk_dirty[static_cast<size_t>((ty / kChunkTiles) * chunks_x() + tx / kChunkTiles)] = 1;
    }
  ++revision;
}

void TileMap::mark_all_dirty() {
  std::fill(chunk_dirty.begin(), chunk_dirty.end(), u8{1});
  ++revision;
}

namespace {

f32 lattice(u64 seed, i32 x, i32 y) {
  u64 h = rng::hash64(seed ^ (static_cast<u64>(static_cast<u32>(x)) * 0x9E3779B1ull) ^
                      (static_cast<u64>(static_cast<u32>(y)) << 32));
  return static_cast<f32>(h >> 40) * (1.0f / 16777216.0f);
}

f32 smooth(f32 t) { return t * t * (3.0f - 2.0f * t); }

f32 value_noise(u64 seed, f32 x, f32 y) {
  i32 x0 = static_cast<i32>(std::floor(x)), y0 = static_cast<i32>(std::floor(y));
  f32 fx = smooth(x - static_cast<f32>(x0)), fy = smooth(y - static_cast<f32>(y0));
  f32 a = lattice(seed, x0, y0), b = lattice(seed, x0 + 1, y0);
  f32 c = lattice(seed, x0, y0 + 1), d = lattice(seed, x0 + 1, y0 + 1);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

f32 fbm(u64 seed, f32 x, f32 y, int octaves) {
  f32 sum = 0, amp = 0.5f, norm = 0, freq = 1;
  for (int o = 0; o < octaves; ++o) {
    sum += amp * value_noise(seed + static_cast<u64>(o) * 7919u, x * freq, y * freq);
    norm += amp;
    amp *= 0.5f;
    freq *= 2.0f;
  }
  return sum / norm;
}

// Multi-source BFS distance (in tiles, 4-connected) from every tile where `src` is true.
std::vector<i32> distance_field(const TileMap& m, const std::vector<u8>& src) {
  std::vector<i32> dist(src.size(), 1 << 20);
  std::queue<i32> q;
  for (size_t i = 0; i < src.size(); ++i)
    if (src[i]) {
      dist[i] = 0;
      q.push(static_cast<i32>(i));
    }
  while (!q.empty()) {
    i32 i = q.front();
    q.pop();
    Vec2i p = m.pos(i);
    for (int d = 0; d < 4; ++d) {
      i32 nx = p.x + kDir4X[d], ny = p.y + kDir4Y[d];
      if (!m.in_bounds(nx, ny)) continue;
      i32 j = m.idx(nx, ny);
      if (dist[static_cast<size_t>(j)] > dist[static_cast<size_t>(i)] + 1) {
        dist[static_cast<size_t>(j)] = dist[static_cast<size_t>(i)] + 1;
        q.push(j);
      }
    }
  }
  return dist;
}

}  // namespace

void generate_map(TileMap& m, const MapGenParams& p) {
  m.resize(p.size, p.size);
  rng::Stream r(p.seed, static_cast<u64>(rng::StreamId::Terrain));
  const u64 s_elev = r.next_u64(), s_moist = r.next_u64(), s_detail = r.next_u64(), s_res = r.next_u64();
  const i32 W = m.w, H = m.h;
  const size_t N = static_cast<size_t>(W * H);

  // 1. Elevation field in [0,1].
  std::vector<f32> elev(N), moist(N);
  for (i32 y = 0; y < H; ++y)
    for (i32 x = 0; x < W; ++x) {
      f32 fx = static_cast<f32>(x), fy = static_cast<f32>(y);
      f32 e = fbm(s_elev, fx / 72.0f, fy / 72.0f, 5);
      e = 0.35f + (e - 0.5f) * (0.6f + 1.4f * p.hills);
      if (p.coast >= 0) {
        f32 t = 0;  // 0 at coast edge, 1 far inland
        switch (p.coast) {
          case 0: t = fy / static_cast<f32>(H); break;
          case 1: t = 1.0f - fx / static_cast<f32>(W); break;
          case 2: t = 1.0f - fy / static_cast<f32>(H); break;
          default: t = fx / static_cast<f32>(W); break;
        }
        f32 wobble = (fbm(s_detail, fx / 24.0f, fy / 24.0f, 3) - 0.5f) * 0.12f;
        e -= std::max(0.0f, 0.38f - (t + wobble) * 1.6f);
      }
      elev[static_cast<size_t>(y * W + x)] = std::clamp(e, 0.0f, 1.0f);
      moist[static_cast<size_t>(y * W + x)] = fbm(s_moist, fx / 48.0f, fy / 48.0f, 4);
    }

  // 2. Sea.
  const f32 sea = 0.12f;
  std::vector<u8> water(N, 0);
  for (size_t i = 0; i < N; ++i)
    if (elev[i] < sea) water[i] = 1;

  // 3. River: walk from an inland edge toward the coast (or the opposite edge), meandering.
  std::vector<u8> river(N, 0);
  if (p.river) {
    i32 side = p.coast >= 0 ? (p.coast + 2) % 4 : static_cast<i32>(r.below(4));
    i32 target = p.coast >= 0 ? p.coast : (side + 2) % 4;
    Vec2i cur;
    i32 span = W / 3;
    i32 off = W / 3 + static_cast<i32>(r.below(static_cast<u32>(span)));
    switch (side) {
      case 0: cur = {off, 0}; break;
      case 1: cur = {W - 1, off}; break;
      case 2: cur = {off, H - 1}; break;
      default: cur = {0, off}; break;
    }
    Vec2i dir{kDir4X[target], kDir4Y[target]};
    f32 phase = 0;
    i32 width = std::max(2, W / 64);
    for (int step = 0; step < W * 4 && m.in_bounds(cur); ++step) {
      for (i32 dy = -width; dy <= width; ++dy)
        for (i32 dx = -width; dx <= width; ++dx) {
          i32 x = cur.x + dx, y = cur.y + dy;
          if (!m.in_bounds(x, y) || dx * dx + dy * dy > width * width) continue;
          river[static_cast<size_t>(y * W + x)] = static_cast<u8>(dx * dx + dy * dy <= (width - 1) * (width - 1) ? 2 : 1);
        }
      if (water[static_cast<size_t>(m.idx(cur))] && step > 20) break;
      phase += 0.07f + r.unit() * 0.05f;
      f32 sway = std::sin(phase) * 0.9f + (fbm(s_detail, static_cast<f32>(step) / 30.0f, 3.0f, 2) - 0.5f) * 1.5f;
      // Advance mostly along `dir`, sway sideways.
      if (r.unit() < 0.62f) {
        cur = cur + dir;
      } else {
        Vec2i side_v{dir.y != 0 ? (sway > 0 ? 1 : -1) : 0, dir.x != 0 ? (sway > 0 ? 1 : -1) : 0};
        cur = cur + side_v;
      }
    }
  }

  std::vector<u8> any_water(N, 0);
  for (size_t i = 0; i < N; ++i) any_water[i] = static_cast<u8>(water[i] || river[i]);
  auto dist_water = distance_field(m, any_water);

  // 4. Classify terrain.
  for (i32 y = 0; y < H; ++y)
    for (i32 x = 0; x < W; ++x) {
      size_t i = static_cast<size_t>(y * W + x);
      f32 e = elev[i], mo = moist[i];
      i32 dw = dist_water[i];
      f32 detail = fbm(s_detail, static_cast<f32>(x) / 9.0f, static_cast<f32>(y) / 9.0f, 3);
      Terrain t = Terrain::Grass;
      if (water[i]) {
        t = e < sea - 0.05f ? Terrain::Deep : Terrain::Shallow;
      } else if (river[i]) {
        t = river[i] == 2 ? Terrain::Deep : Terrain::Shallow;
      } else if (p.coast >= 0 && dw <= 2 && water[i] == 0 && e < sea + 0.05f) {
        t = Terrain::Sand;
      } else if (e > 0.78f - 0.1f * p.hills && detail > 0.42f) {
        t = Terrain::Rock;
      } else if (dw <= 6 && e < 0.3f && mo > 0.62f - 0.15f * p.marsh) {
        t = Terrain::Marsh;
      } else if (mo > 0.58f - 0.12f * p.forest && detail > 0.45f) {
        t = Terrain::Forest;
      } else if (dw <= 10 && detail > 0.35f) {
        t = Terrain::Fertile;
      } else if (mo < 0.32f && detail < 0.3f) {
        t = Terrain::Sand;
      }
      m.terrain[i] = t;
      f32 land = std::clamp((e - sea) / (1.0f - sea), 0.0f, 1.0f);
      m.height[i] = terrain_water(t) ? 0 : static_cast<u8>(std::clamp(1 + static_cast<int>(land * 7.0f), 1, 7));
      m.groundwater[i] = static_cast<u8>(std::clamp(255 - dw * 9 - static_cast<int>(land * 120.0f), 20, 255));
      int fert = 128 + static_cast<int>((mo - 0.5f) * 100.0f) + std::max(0, 40 - dw * 3);
      if (t == Terrain::Fertile) fert += 70;
      if (t == Terrain::Sand || t == Terrain::Rock) fert = 30;
      m.fertility[i] = static_cast<u8>(std::clamp(fert, 0, 255));
      m.variant[i] = static_cast<u8>(rng::hash64(p.seed ^ (i * 2654435761ull)) & 0xFF);
    }

  // 5. Resources.
  for (i32 y = 0; y < H; ++y)
    for (i32 x = 0; x < W; ++x) {
      size_t i = static_cast<size_t>(y * W + x);
      Terrain t = m.terrain[i];
      f32 n = fbm(s_res, static_cast<f32>(x) / 7.0f, static_cast<f32>(y) / 7.0f, 2);
      f32 n2 = fbm(s_res + 99, static_cast<f32>(x) / 11.0f, static_cast<f32>(y) / 11.0f, 2);
      Resource res = Resource::None;
      if (t == Terrain::Rock) {
        if (n > 0.66f) res = Resource::IronOre;
        else if (n2 > 0.72f) res = Resource::SilverOre;
        else if (n2 < 0.24f) res = Resource::Marble;
        else res = Resource::Stone;
      } else if (terrain_water(t)) {
        if (n > 0.55f) res = Resource::Fish;
      } else if ((t == Terrain::Marsh || dist_water[i] <= 3) && n > 0.6f) {
        res = Resource::Clay;
      } else if (m.height[i] >= 4 && n2 > 0.7f) {
        res = Resource::Limestone;
      } else if (m.height[i] >= 3 && n < 0.2f) {
        res = Resource::Pozzolana;
      }
      m.resource[i] = res;
    }
  m.mark_all_dirty();
}

}  // namespace urbs::sim
