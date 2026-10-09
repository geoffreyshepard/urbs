#pragma once
#include <vector>

#include "core/math.hpp"
#include "core/types.hpp"
#include "sim/map.hpp"

namespace urbs::path {

// Movement cost of entering/leaving a tile, in tenths of a tick per tile (0 = blocked).
// A step between two tiles costs (cost[a] + cost[b]) / 2, times 14/10 for diagonals.
// Diagonal steps are only allowed when both orthogonal neighbours are walkable, so
// connectivity equals 4-connectivity.
inline constexpr u8 kBlocked = 0;
inline constexpr u8 kCostPaved = 25;
inline constexpr u8 kCostDirt = 30;
inline constexpr u8 kCostMin = 25;

struct NavGrid {
  i32 w = 0, h = 0;
  std::vector<u8> cost;
  std::vector<u32> region;           // connectivity component id (0 = blocked)
  std::vector<Tick> chunk_changed;   // tick of last nav change per chunk
  std::vector<u8> chunk_dirty;       // chunk needs HPA rebuild
  u32 region_count = 0;
  bool regions_dirty = true;

  void init(const sim::TileMap& map);
  // Recomputes a tile's cost from the map (terrain, road, building, extra blockers).
  void update_tile(const sim::TileMap& map, i32 x, i32 y, Tick now);
  void set_cost(i32 x, i32 y, u8 c, Tick now);
  void rebuild_regions();  // O(tiles), uses preallocated queue

  [[nodiscard]] bool in_bounds(i32 x, i32 y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  [[nodiscard]] i32 idx(i32 x, i32 y) const { return y * w + x; }
  [[nodiscard]] bool walkable(i32 x, i32 y) const { return in_bounds(x, y) && cost[static_cast<size_t>(idx(x, y))] != 0; }
  [[nodiscard]] bool connected(i32 a, i32 b) const {
    return region[static_cast<size_t>(a)] != 0 && region[static_cast<size_t>(a)] == region[static_cast<size_t>(b)];
  }
  [[nodiscard]] i32 chunks_x() const { return (w + sim::kChunkTiles - 1) / sim::kChunkTiles; }
  [[nodiscard]] i32 chunks_y() const { return (h + sim::kChunkTiles - 1) / sim::kChunkTiles; }
  [[nodiscard]] i32 chunk_of(i32 x, i32 y) const { return (y / sim::kChunkTiles) * chunks_x() + x / sim::kChunkTiles; }

  // Step cost from tile a to neighbour b (direction d in kDir8), or 0 if not allowed.
  [[nodiscard]] u32 step_cost(i32 ax, i32 ay, int d) const {
    i32 bx = ax + kDir8X[d], by = ay + kDir8Y[d];
    if (!in_bounds(bx, by)) return 0;
    u32 cb = cost[static_cast<size_t>(idx(bx, by))];
    if (cb == 0) return 0;
    u32 ca = cost[static_cast<size_t>(idx(ax, ay))];
    if (d & 1) {
      if (!walkable(ax + kDir8X[d], ay) || !walkable(ax, ay + kDir8Y[d])) return 0;
      return (ca + cb) * 7 / 10;  // (ca+cb)/2 * 1.4
    }
    return (ca + cb) / 2;
  }

 private:
  std::vector<i32> queue_;
};

u8 tile_cost(const sim::TileMap& map, i32 i);

}  // namespace urbs::path
