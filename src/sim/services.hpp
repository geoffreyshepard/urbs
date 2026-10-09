#pragma once
#include <vector>

#include "core/types.hpp"
#include "path/nav.hpp"

namespace urbs::save {
class Writer;
class Reader;
}  // namespace urbs::save

namespace urbs::sim {

// Per-service "nearest provider" maps: multi-source Dijkstra (Dial's buckets) over the walk graph
// from every provider entrance. Each tile gets its nearest provider and the travel cost.
// Recomputed incrementally: one service at a time, a bounded number of tiles per tick.
class ServiceMaps {
 public:
  static constexpr u16 kUncovered = 0xFFFF;
  static constexpr u32 kMaxCost = 4000;  // tenths of ticks (~8 game hours of walking)

  void init(i32 w, i32 h, u32 services, u32 max_providers);
  // Provider set for a service. Call when it changes (marks the service dirty).
  void set_providers(u32 service, const std::vector<BuildingId>& ids, const std::vector<i32>& entrances);
  void mark_dirty(u32 service) { dirty_[service] = 1; }
  void mark_all_dirty();
  // Advances the computation by up to `budget` settled tiles. Call once per tick.
  void step(const path::NavGrid& nav, u32 budget);

  [[nodiscard]] BuildingId provider(u32 service, i32 tile) const {
    u16 p = maps_[service].prov[static_cast<size_t>(tile)];
    return p == kUncovered ? BuildingId{} : maps_[service].ids[p];
  }
  // Travel cost in ticks, or 0xFFFF if uncovered.
  [[nodiscard]] u16 cost_ticks(u32 service, i32 tile) const {
    u16 p = maps_[service].prov[static_cast<size_t>(tile)];
    return p == kUncovered ? 0xFFFF : static_cast<u16>(maps_[service].cost[static_cast<size_t>(tile)] / 10);
  }
  void serialize(save::Writer& w) const;
  void deserialize(save::Reader& r);
  [[nodiscard]] u32 services() const { return static_cast<u32>(maps_.size()); }
  [[nodiscard]] bool ready(u32 service) const { return maps_[service].ready; }
  [[nodiscard]] u32 generation(u32 service) const { return maps_[service].generation; }

 private:
  struct Map {
    std::vector<u16> prov;  // provider index into ids, kUncovered if none
    std::vector<u16> cost;  // tenths of ticks
    std::vector<BuildingId> ids;
    bool ready = false;
    u32 generation = 0;
  };
  void begin(u32 service, const path::NavGrid& nav);
  void link(u32 tile, u32 bucket);
  void unlink(u32 tile);

  i32 w_ = 0, h_ = 0;
  std::vector<Map> maps_;
  std::vector<std::vector<BuildingId>> pending_ids_;
  std::vector<std::vector<i32>> pending_entr_;
  std::vector<u8> dirty_;
  // In-progress computation
  Map work_;
  i32 running_ = -1;
  std::vector<u32> bucket_head_;  // kBuckets
  std::vector<u32> next_, prev_;
  std::vector<u8> settled_;
  u32 cur_cost_ = 0, in_queue_ = 0;
  u32 rr_ = 0;
};

}  // namespace urbs::sim
