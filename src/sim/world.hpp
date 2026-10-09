#pragma once
#include <memory>
#include <vector>

#include "core/pool.hpp"
#include "core/rng.hpp"
#include "data/defs.hpp"
#include "path/nav.hpp"
#include "path/path_system.hpp"
#include "sim/buildings.hpp"
#include "sim/citizens.hpp"
#include "sim/map.hpp"
#include "sim/scheduler.hpp"
#include "sim/services.hpp"
#include "sim/time.hpp"

namespace enki {
class TaskScheduler;
}
namespace urbs::save {
class Writer;
class Reader;
}  // namespace urbs::save

namespace urbs::sim {

struct WorldConfig {
  u32 max_citizens = 16384;
  u32 max_buildings = 4096;
  u32 max_households = 8192;
  u32 threads = 0;  // 0 = hardware concurrency
  u32 path_budget = 384;
  u32 decision_budget = 4000;
  u32 service_budget = 60000;  // tiles settled per tick across service maps
  u64 seed = 1;
};

// ---- Commands: the only way the outside world mutates the sim (applied at tick start) -----------
enum class CmdType : u8 { PlaceBuilding, PlaceRoad, Demolish, SetPolicy, Debug };
struct Command {
  CmdType type = CmdType::Debug;
  u16 def = 0;    // building def / road type / policy id
  i32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  i32 value = 0;
  u8 flags = 0;   // PlaceBuilding: bit0 = instant (no construction)
};

enum class CitizenEvent : u16 { Decide = 1, Arrive = 2 };
enum class BuildingEvent : u16 { Produce = 1, Fire = 2 };

struct TickStats {
  u32 events = 0, decisions = 0, arrivals = 0, path_requests = 0, path_results = 0;
  u32 walking = 0;
  f64 tick_ms = 0;
};

struct World {
  WorldConfig cfg;
  const data::Defs* defs = nullptr;
  Tick now = 0;

  TileMap map;
  path::NavGrid nav;
  path::PathSystem paths;
  Scheduler sched;
  ServiceMaps services;
  Buildings buildings;
  Citizens citizens;
  Households households;
  rng::Stream rng[static_cast<size_t>(rng::StreamId::Count)];

  std::vector<Command> commands;  // pending, applied at next tick start
  TickStats stats;
  u32 walking = 0;

  World();
  ~World();
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  // Sets up all systems for an existing map (map must be generated/loaded first).
  void init(const WorldConfig& cfg, const data::Defs& defs);
  void tick();
  void push(const Command& c) { commands.push_back(c); }

  rng::Stream& stream(rng::StreamId id) { return rng[static_cast<size_t>(id)]; }

  // --- Structure (used by commands, scenarios and tests) ---
  BuildingId place_building(BuildingDefId def, i32 x, i32 y, i32 w, i32 h, bool instant);
  bool can_place(BuildingDefId def, i32 x, i32 y, i32 w, i32 h) const;
  void place_road(i32 x, i32 y, Road type);
  void place_road_line(i32 x0, i32 y0, i32 x1, i32 y1, Road type);
  void demolish(i32 x0, i32 y0, i32 x1, i32 y1);
  void remove_building(BuildingId id);
  void complete_construction(BuildingId id);
  [[nodiscard]] i32 entrance_tile(BuildingId id) const { return buildings[id].entrance; }

  // --- Citizens ---
  CitizenId spawn_citizen(CitizenClass cls, BuildingId at, HouseholdId hh, u32 age_years);
  void despawn_citizen(CitizenId id);
  HouseholdId create_household(BuildingId home, CitizenClass cls);
  // World-px position for rendering at fractional tick `alpha` past `now`. Returns false if indoors.
  bool citizen_position(u32 i, f32 alpha, Vec2& out_px, int* facing) const;

  // --- Services ---
  void refresh_service_providers();

  enki::TaskScheduler* jobs() { return jobs_.get(); }

  // Save/load of dynamic state (map and config are handled by save::save_world).
  void serialize(save::Writer& w) const;
  void deserialize(save::Reader& r);

  // Event helpers
  void schedule_citizen(u32 i, Tick when, CitizenEvent ev);
  void request_walk(u32 i, i32 from_tile, BuildingId to, i32 to_tile, Activity next);

 private:
  void apply_commands();
  void process_events();
  void apply_path_results();
  void on_citizen_event(const Event& e);
  void decide(u32 i);
  void arrive(u32 i);
  void start_activity(u32 i, Activity a);
  [[nodiscard]] i32 current_tile(u32 i) const;
  [[nodiscard]] i32 jitter(u32 i) const;
  void compute_entrance(Building& b);
  void update_nav_rect(i32 x, i32 y, i32 w, i32 h);

  std::unique_ptr<enki::TaskScheduler> jobs_;
  core::FixedVec<Event> due_;
  core::FixedVec<path::PathResult> results_;
  std::vector<BuildingId> scratch_ids_;
  std::vector<i32> scratch_tiles_;
  bool providers_dirty_ = true;
};

}  // namespace urbs::sim
