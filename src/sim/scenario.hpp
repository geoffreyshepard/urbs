#pragma once
#include <string>

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
  std::string json;  // raw scenario text (for scripted scenarios)
};

ScenarioSpec load_scenario(const std::string& path);
// Generates the map and populates the world according to the spec. World must not be initialised.
void setup_scenario(World& w, const data::Defs& defs, const ScenarioSpec& spec);

}  // namespace urbs::sim
