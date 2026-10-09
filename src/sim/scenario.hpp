#pragma once
#include <string>
#include <utility>
#include <vector>

#include "sim/world.hpp"

namespace urbs::sim {

struct ScenarioSpec {
  std::string name;
  MapGenParams map;
  WorldConfig world;
  std::string generator;  // "stress_city", "empty", "town"
  u32 citizens = 0;
  u32 houses = 0, workplaces = 0, markets = 0;
  u32 house_capacity = 0;  // override (stress only); 0 = def value
  u64 ticks = 0;
  // Test expectations (headless --test)
  u32 expect_min_citizens = 0;
  f64 expect_max_tick_ms = 0;
  u32 expect_max_alloc_violations = 0xFFFFFFFFu;
  std::vector<std::pair<std::string, u32>> expect_stock;   // good id -> minimum units anywhere
  std::vector<std::pair<std::string, u32>> expect_active;
  std::vector<std::pair<std::string, u32>> expect_produced;  // good id -> min produced yesterday+today  // building id -> minimum active count
  // Scripted orders: commands pushed before the given tick.
  std::vector<std::pair<Tick, Command>> orders;
  bool industry = false;  // town generator: add workshops, forum, trade post
  i64 min_treasury_end = -1000000000;
  u32 expect_min_births = 0, expect_min_deaths = 0;
  std::string json;  // raw scenario text (for scripted scenarios)
};

// Pushes all orders due at the next tick. Call before World::tick().
void push_due_orders(World& w, const ScenarioSpec& spec);
// Checks test expectations; prints failures to stderr. Returns true if all pass.
bool check_expectations(const World& w, const ScenarioSpec& spec);

ScenarioSpec load_scenario(const std::string& path);
// Generates the map and populates the world according to the spec. World must not be initialised.
void setup_scenario(World& w, const data::Defs& defs, ScenarioSpec& spec);

}  // namespace urbs::sim
