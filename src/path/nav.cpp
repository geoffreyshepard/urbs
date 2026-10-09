#include "path/nav.hpp"

namespace urbs::path {

u8 tile_cost(const sim::TileMap& m, i32 i) {
  const size_t s = static_cast<size_t>(i);
  if (m.building[s].valid()) return kBlocked;
  switch (m.road[s]) {
    case sim::Road::Paved:
    case sim::Road::Bridge: return kCostPaved;
    case sim::Road::Dirt: return kCostDirt;
    default: break;
  }
  switch (m.terrain[s]) {
    case sim::Terrain::Grass: return 50;
    case sim::Terrain::Fertile: return 55;
    case sim::Terrain::Sand: return 55;
    case sim::Terrain::Rock: return 65;
    case sim::Terrain::Forest: return 70;
    case sim::Terrain::Marsh: return 85;
    default: return kBlocked;
  }
}

void NavGrid::init(const sim::TileMap& m) {
  w = m.w;
  h = m.h;
  const size_t n = static_cast<size_t>(w * h);
  cost.resize(n);
  for (size_t i = 0; i < n; ++i) cost[i] = tile_cost(m, static_cast<i32>(i));
  region.assign(n, 0);
  queue_.assign(n, 0);
  chunk_changed.assign(static_cast<size_t>(chunks_x() * chunks_y()), 0);
  chunk_dirty.assign(static_cast<size_t>(chunks_x() * chunks_y()), 1);
  regions_dirty = true;
  rebuild_regions();
}

void NavGrid::set_cost(i32 x, i32 y, u8 c, Tick now) {
  size_t i = static_cast<size_t>(idx(x, y));
  if (cost[i] == c) return;
  const bool connectivity_changed = (cost[i] == 0) != (c == 0);
  cost[i] = c;
  // A border tile change affects portals of the neighbouring chunk too.
  for (i32 dy = -1; dy <= 1; ++dy)
    for (i32 dx = -1; dx <= 1; ++dx) {
      i32 tx = x + dx, ty = y + dy;
      if (!in_bounds(tx, ty)) continue;
      size_t ch = static_cast<size_t>(chunk_of(tx, ty));
      chunk_dirty[ch] = 1;
      chunk_changed[ch] = now;
    }
  if (connectivity_changed) regions_dirty = true;
}

void NavGrid::update_tile(const sim::TileMap& m, i32 x, i32 y, Tick now) { set_cost(x, y, tile_cost(m, idx(x, y)), now); }

void NavGrid::rebuild_regions() {
  if (!regions_dirty) return;
  std::fill(region.begin(), region.end(), 0u);
  u32 next = 0;
  const i32 n = w * h;
  for (i32 s = 0; s < n; ++s) {
    if (cost[static_cast<size_t>(s)] == 0 || region[static_cast<size_t>(s)] != 0) continue;
    ++next;
    i32 qh = 0, qt = 0;
    queue_[static_cast<size_t>(qt++)] = s;
    region[static_cast<size_t>(s)] = next;
    while (qh < qt) {
      i32 c = queue_[static_cast<size_t>(qh++)];
      i32 cx = c % w, cy = c / w;
      for (int d = 0; d < 4; ++d) {
        i32 nx = cx + kDir4X[d], ny = cy + kDir4Y[d];
        if (!in_bounds(nx, ny)) continue;
        size_t ni = static_cast<size_t>(idx(nx, ny));
        if (cost[ni] == 0 || region[ni] != 0) continue;
        region[ni] = next;
        queue_[static_cast<size_t>(qt++)] = static_cast<i32>(ni);
      }
    }
  }
  region_count = next;
  regions_dirty = false;
}

}  // namespace urbs::path
