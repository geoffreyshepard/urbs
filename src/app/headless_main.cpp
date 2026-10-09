#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/log.hpp"
#include "sim/sim.hpp"

int main(int argc, char** argv) {
  urbs::u64 ticks = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--version") == 0) {
      std::printf("urbs_headless 0.1.0\n");
      return 0;
    }
    if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) ticks = std::strtoull(argv[++i], nullptr, 10);
  }
  urbs::core::init_logging(false);
  urbs::sim::Sim sim;
  const auto t0 = std::chrono::steady_clock::now();
  for (urbs::u64 t = 0; t < ticks; ++t) sim.tick();
  const auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  URBS_LOG_INFO("ran {} ticks in {:.2f} ms", sim.now(), dt);
  return 0;
}
