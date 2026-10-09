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
  bool test = false, verbose = false, dump = false;
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
    else if (is("--dump")) dump = true;
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
  double sys_max[8] = {}, sys_sum[8] = {};
  for (u64 t = 0; t < ticks; ++t) {
    sim::push_due_orders(*world, spec);
    world->tick();
    ms.push_back(world->stats.tick_ms);
    decisions += world->stats.decisions;
    path_req += world->stats.path_requests;
    arrivals += world->stats.arrivals;
    max_walking = std::max(max_walking, world->stats.walking);
    for (int k = 0; k < 8; ++k) {
      sys_max[k] = std::max(sys_max[k], world->stats.sys_ms[k]);
      sys_sum[k] += world->stats.sys_ms[k];
    }
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
  if (verbose) {
    const char* names[8] = {"commands", "services", "daily", "labor", "matcher", "events", "paths", "-"};
    std::printf("system ms (avg / max):");
    for (int k = 0; k < 7; ++k) std::printf(" %s %.3f/%.2f", names[k], sys_sum[k] / static_cast<double>(ticks), sys_max[k]);
    std::printf("\n");
  }
  if (dump || verbose) {
    u32 reasons[64] = {}, acts[32] = {}, tiers[8] = {};
    const auto& C = world->citizens;
    for (u32 i = 0; i < C.ids.high_water(); ++i)
      if (C.ids.alive_index(i)) {
        ++reasons[static_cast<int>(C.reason[i])];
        ++acts[static_cast<int>(C.activity[i])];
      }
    world->buildings.for_each([&](BuildingId, const sim::Building& b) {
      if (defs.b(b.def).kind == data::BuildingKind::House) ++tiers[b.tier & 7];
    });
    {
      i64 a[13] = {};
      u32 n = 0;
      for (u32 i = 0; i < C.ids.high_water(); ++i) {
        if (!C.ids.alive_index(i)) continue;
        auto p = world->happiness_parts(i);
        i16 v[13] = {p.food, p.water, p.rest, p.hygiene, p.religion, p.entertainment, p.variety, p.housing,
                     p.employment, p.taxes, p.gods, p.events, p.health};
        for (int k = 0; k < 13; ++k) a[k] += v[k];
        ++n;
      }
      const char* names[13] = {"food", "water", "rest", "hygiene", "religion", "entert", "variety", "housing",
                               "employ", "taxes", "gods", "events", "health"};
      std::printf("happiness parts (avg):");
      for (int k = 0; k < 13; ++k) std::printf(" %s=%.1f", names[k], n ? static_cast<double>(a[k]) / n : 0.0);
      std::printf("\n");
      u32 unmet[32] = {};
      world->buildings.for_each([&](BuildingId, const sim::Building& b) {
        if (defs.b(b.def).kind == data::BuildingKind::House) ++unmet[std::min<u32>(b.extra, 31)];
      });
      std::printf("houses by first unmet next-tier requirement (1-based, 0=none):");
      for (int k = 0; k < 32; ++k)
        if (unmet[k]) std::printf(" #%d=%u", k, unmet[k]);
      std::printf("\n");
    }
    std::printf("reasons:");
    for (int k = 0; k < static_cast<int>(sim::Reason::Count); ++k)
      if (reasons[k]) std::printf(" [%s]=%u", sim::reason_text(static_cast<sim::Reason>(k)), reasons[k]);
    std::printf("\nactivities:");
    for (int k = 0; k < static_cast<int>(sim::Activity::Count); ++k)
      if (acts[k]) std::printf(" %s=%u", sim::activity_name(static_cast<sim::Activity>(k)), acts[k]);
    std::printf("\nhouse tiers: %u %u %u %u %u  appeal %d  happiness %u  immigrants %u emigrants %u\n", tiers[0],
                tiers[1], tiers[2], tiers[3], tiers[4], world->appeal, world->avg_happiness, world->immigrants_total,
                world->emigrants_total);
  }
  if (dump) {
    world->buildings.for_each([&](BuildingId, const sim::Building& b) {
      const auto& d = defs.b(b.def);
      std::printf("%-14s at %3d,%3d state=%d jobs=%u emp=%u present=%u prog=%u/%u wp=%u inv:", d.id.c_str(), b.x, b.y,
                  static_cast<int>(b.state), b.jobs, b.employed, b.present, b.build_progress, b.build_needed,
                  b.work_progress);
      if (b.inv != econ::kNoInv)
        for (u32 g = 0; g < world->econ.goods; ++g) {
          GoodId gid{static_cast<u16>(g)};
          if (world->econ.amt(b.inv, gid) || world->econ.rin(b.inv, gid) || world->econ.rout(b.inv, gid))
            std::printf(" %s=%u(+%u,-%u)", defs.goods[g].id.c_str(), world->econ.amt(b.inv, gid),
                        world->econ.rin(b.inv, gid), world->econ.rout(b.inv, gid));
        }
      std::printf("\n");
    });
    for (u32 g = 0; g < world->econ.goods; ++g)
      if (world->econ.produced_yday[g] || world->econ.consumed_yday[g])
        std::printf("  %s: produced %u consumed %u (yesterday)\n", defs.goods[g].id.c_str(), world->econ.produced_yday[g],
                    world->econ.consumed_yday[g]);
    std::printf("open tasks %u, treasury %lld\n", world->econ.open_tasks, static_cast<long long>(world->econ.treasury));
  }
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
    if (!sim::check_expectations(*world, spec)) ok = false;
    if (world->sched.dropped() > 0) {
      std::fprintf(stderr, "FAIL: scheduler dropped events\n");
      ok = false;
    }
    return ok ? 0 : 1;
  }
  return 0;
}
