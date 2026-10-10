#pragma once
// Chaos player for robustness testing: issues random commands through the same queue the UI uses.
// Most are things a careless player could do (build anywhere, demolish anything, extreme taxes,
// raise and disband armies, call disasters); a few carry out-of-range ids and coordinates to fuzz
// the command validation. Deterministic for a given seed. See docs/testing.md.
#include "core/types.hpp"

namespace urbs::sim {

struct World;

struct Chaos {
  explicit Chaos(u64 seed) : s_(seed ? seed : 1) {}
  // Call before every tick; acts `per_hour` times in each game hour.
  void step(World& w, u32 per_hour);
  u64 actions = 0;

 private:
  u64 s_;
  u64 next();
  i32 below(i32 n) { return n > 0 ? static_cast<i32>(next() % static_cast<u64>(n)) : 0; }
  void act(World& w);
};

}  // namespace urbs::sim
