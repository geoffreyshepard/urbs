#pragma once
// Consistency checks over the whole world: counters and reservations that the simulation keeps
// incrementally must agree with a recount from scratch. Run between ticks (never inside one): it
// allocates freely. Used by urbs_headless --check and by tests; see docs/testing.md.
#include <string>
#include <vector>

#include "core/types.hpp"

namespace urbs::sim {

struct World;

struct InvariantChecker {
  // Appends one line per violation (at most `limit` lines) and returns the number of violations.
  // Call it once a game day: the money check compares the treasury with the ledger since the last call.
  u32 check(const World& w, std::vector<std::string>& out, u32 limit = 20);

 private:
  bool primed_ = false;
  i64 last_treasury_ = 0;
  i64 last_today_ = 0;  // signed sum of the ledger's "today" column at the last call
  u32 last_day_ = 0;
};

}  // namespace urbs::sim
