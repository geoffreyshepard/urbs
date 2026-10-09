#pragma once
#include <string>

#include "core/types.hpp"
#include "data/defs.hpp"
#include "sim/world.hpp"

namespace urbs::save {

// Bump on any change to saved state; add a migration in load_world or reject with a clear error.
// Version history: 1 = v0.1.0; 2 = civic crime cut (M12). Older versions still load.
inline constexpr u32 kSaveVersion = 2;
inline constexpr u32 kOldestSaveVersion = 1;

struct SaveInfo {
  u32 version = 0;
  std::string mission;
  u64 seed = 0;
  Tick now = 0;
  u32 citizens = 0;
};

// Writes the world to `path` (header + zstd-compressed payload). Returns false and sets err on failure.
bool save_world(const sim::World& w, const std::string& path, const std::string& mission, std::string* err);
// Loads into a fresh (uninitialised) World. Returns false and sets err on failure.
bool load_world(sim::World& w, const data::Defs& defs, const std::string& path, std::string* err);
bool read_info(const std::string& path, SaveInfo& out);

// Hash of the simulation state, for determinism and round-trip tests.
u64 world_hash(const sim::World& w);

}  // namespace urbs::save
