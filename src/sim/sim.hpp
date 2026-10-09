#pragma once
#include "core/types.hpp"

namespace urbs::sim {

// M0 stub: owns the tick counter. Replaced by the full world in M2.
class Sim {
 public:
  void tick() { ++tick_; }
  [[nodiscard]] Tick now() const { return tick_; }

 private:
  Tick tick_ = 0;
};

}  // namespace urbs::sim
