#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/alloc_track.hpp"
#include "core/log.hpp"
#include "data/defs.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

using namespace urbs;

namespace {
std::string data_root() {
#ifdef URBS_SOURCE_DIR
  return std::string(URBS_SOURCE_DIR) + "/data";
#else
  return "data";
#endif
}
}  // namespace

int main(int argc, char** argv) {
  std::string scenario_path;
  u64 ticks_override = 0;
  bool test = false, verbose = false;
  for (int i = 1; i < argc; ++i) {
    auto is = [&](const char* n) { return std::strcmp(argv[i], n) == 0; };
    if (is("--version")) {
      std::printf("urbs_headless 0.1.0\n");
      return 0;
    }
    if (is("--scenario") && i + 1 < argc) scenario_path = argv[++i];
    else if (is("--ticks") && i + 1 < argc) ticks_override = std::strtoull(argv[++i], nullptr, 10);
    else if (is("--test")) test = true;
    else if (is("--verbose")) verbose = true;
  }
  core::init_logging(verbose);
  if (scenario_path.empty()) {
    std::fprintf(stderr, "usage: urbs_headless --scenario file.json [--ticks N] [--test]\n");
    return 2;
  }
  data::Defs defs;
  sim::ScenarioSpec spec;
  try {
    defs = data::load_defs(data_root());
    spec = sim::load_scenario(scenario_path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  auto world = std::make_unique<sim::World>();
  const auto ts = std::chrono::steady_clock::now();
  sim::setup_scenario(*world, defs, spec);
  URBS_LOG_INFO("setup {:.0f} ms, citizens {}, buildings {}",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - ts).count(),
                world->citizens.ids.count(), world->buildings.ids.count());

  const u64 ticks = ticks_override ? ticks_override : (spec.ticks ? spec.ticks : 1200);
  std::vector<double> ms;
  ms.reserve(ticks);
  core::alloc::reset_violations();
  u64 decisions = 0, path_req = 0, arrivals = 0;
  u32 max_walking = 0;
  for (u64 t = 0; t < ticks; ++t) {
    world->tick();
    ms.push_back(world->stats.tick_ms);
    decisions += world->stats.decisions;
    path_req += world->stats.path_requests;
    arrivals += world->stats.arrivals;
    max_walking = std::max(max_walking, world->stats.walking);
    if (verbose && world->now % kTicksPerHour == 0)
      URBS_LOG_INFO("t={} h={} walking={} events={} pathq={} tick={:.2f}ms", world->now, sim::hour_of_day(world->now),
                    world->stats.walking, world->stats.events, world->paths.pending(), world->stats.tick_ms);
  }
  // Warm-up (first game hour) excluded from steady-state stats.
  std::vector<double> steady(ms.begin() + static_cast<std::ptrdiff_t>(std::min<u64>(ticks, kTicksPerHour)), ms.end());
  if (steady.empty()) steady = ms;
  double sum = 0;
  for (double v : steady) sum += v;
  const double avg = sum / static_cast<double>(steady.size());
  std::sort(steady.begin(), steady.end());
  const double p99 = steady[std::min(steady.size() - 1, steady.size() * 99 / 100)];
  const double mx = steady.back();
  const double per_frame_4x = avg * 80.0 / 60.0;
  const u64 viol = core::alloc::violations();
  const auto& ps = world->paths.store();
  std::printf(
      "{\"scenario\":\"%s\",\"ticks\":%llu,\"citizens\":%u,\"avg_tick_ms\":%.3f,\"p99_tick_ms\":%.3f,"
      "\"max_tick_ms\":%.3f,\"sim_ms_per_frame_4x\":%.3f,\"alloc_violations\":%llu,\"decisions\":%llu,"
      "\"path_requests\":%llu,\"arrivals\":%llu,\"max_walking\":%u,\"path_cache_hits\":%llu,"
      "\"path_cache_misses\":%llu,\"paths_live\":%u,\"sched_dropped\":%llu}\n",
      spec.name.c_str(), static_cast<unsigned long long>(ticks), world->citizens.ids.count(), avg, p99, mx,
      per_frame_4x, static_cast<unsigned long long>(viol), static_cast<unsigned long long>(decisions),
      static_cast<unsigned long long>(path_req), static_cast<unsigned long long>(arrivals), max_walking,
      static_cast<unsigned long long>(ps.hits()), static_cast<unsigned long long>(ps.misses()), ps.live_paths(),
      static_cast<unsigned long long>(world->sched.dropped()));
  if (test) {
    bool ok = true;
    if (world->citizens.ids.count() < spec.expect_min_citizens) {
      std::fprintf(stderr, "FAIL: citizens %u < %u\n", world->citizens.ids.count(), spec.expect_min_citizens);
      ok = false;
    }
    if (spec.expect_max_tick_ms > 0 && avg > spec.expect_max_tick_ms) {
      std::fprintf(stderr, "FAIL: avg tick %.3f ms > %.3f\n", avg, spec.expect_max_tick_ms);
      ok = false;
    }
    if (core::alloc::hook_installed() && viol > spec.expect_max_alloc_violations) {
      std::fprintf(stderr, "FAIL: %llu allocations inside tick()\n", static_cast<unsigned long long>(viol));
      ok = false;
    }
    if (world->sched.dropped() > 0) {
      std::fprintf(stderr, "FAIL: scheduler dropped events\n");
      ok = false;
    }
    return ok ? 0 : 1;
  }
  return 0;
}
