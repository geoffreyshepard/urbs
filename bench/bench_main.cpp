#include <chrono>
#include <cstdio>

#include "sim/sim.hpp"

int main() {
  urbs::sim::Sim sim;
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000000; ++i) sim.tick();
  auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::printf("{\"empty_tick_ms_per_1M\": %.3f}\n", ms);
  return 0;
}
