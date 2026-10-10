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
#include "save/save.hpp"
#include "sim/scenario.hpp"
#include "sim/autoplay.hpp"
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
  std::string scenario_path, load_path;
  u64 ticks_override = 0;
  bool test = false, verbose = false, dump = false, autoplay = false, daily = false;
  int difficulty = -1;
  u64 seed_override = 0;
  for (int i = 1; i < argc; ++i) {
    auto is = [&](const char* n) { return std::strcmp(argv[i], n) == 0; };
    if (is("--version")) {
      std::printf("urbs_headless 0.1.0\n");
      return 0;
    }
    if (is("--scenario") && i + 1 < argc) scenario_path = argv[++i];
    else if (is("--load") && i + 1 < argc) load_path = argv[++i];
    else if (is("--ticks") && i + 1 < argc) ticks_override = std::strtoull(argv[++i], nullptr, 10);
    else if (is("--test")) test = true;
    else if (is("--verbose")) verbose = true;
    else if (is("--dump")) dump = true;
    else if (is("--autoplay")) autoplay = true;
    else if (is("--daily")) daily = true;
    else if (is("--difficulty") && i + 1 < argc) difficulty = std::atoi(argv[++i]);
    else if (is("--seed") && i + 1 < argc) seed_override = std::strtoull(argv[++i], nullptr, 10);
  }
  core::init_logging(verbose);
  if (scenario_path.empty() && load_path.empty()) {
    std::fprintf(stderr, "usage: urbs_headless (--scenario file.json | --load save.urbs) [--ticks N] [--test]\n");
    return 2;
  }
  data::Defs defs;
  sim::ScenarioSpec spec;
  try {
    defs = data::load_defs(data_root());
    if (!scenario_path.empty()) spec = sim::load_scenario(scenario_path);
    if (seed_override) spec.world.seed = seed_override;  // the simulation's randomness, not the map
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  auto world = std::make_unique<sim::World>();
  const auto ts = std::chrono::steady_clock::now();
  if (!load_path.empty()) {
    std::string err;
    if (!save::load_world(*world, defs, load_path, &err)) {
      std::fprintf(stderr, "error: %s\n", err.c_str());
      return 1;
    }
    std::printf("loaded %s: day %u, population %u\n", load_path.c_str(), world->mission_days, world->population());
  } else {
    sim::setup_scenario(*world, defs, spec);
  }
  URBS_LOG_INFO("setup {:.0f} ms, citizens {}, buildings {}",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - ts).count(),
                world->citizens.ids.count(), world->buildings.ids.count());

  if (verbose) {
    u32 res[static_cast<int>(sim::Resource::Count)] = {};
    for (auto r : world->map.resource) ++res[static_cast<int>(r)];
    std::printf("resource tiles: clay %u stone %u iron %u silver %u marble %u limestone %u pozzolana %u fish %u\n", res[1],
                res[2], res[3], res[4], res[5], res[6], res[7], res[8]);
  }
  u32 att_present = 0, att_employed = 0;
  sim::Autoplay bot;
  if (autoplay) bot.init(*world);
  if (difficulty >= 0) world->set_difficulty(static_cast<u8>(difficulty));
  const u64 ticks = ticks_override ? ticks_override : (spec.ticks ? spec.ticks : 1200);
  std::vector<double> ms;
  ms.reserve(ticks);
  core::alloc::reset_violations();
  u64 decisions = 0, path_req = 0, arrivals = 0;
  u32 max_walking = 0;
  double sys_max[8] = {}, sys_sum[8] = {};
  u64 fallbacks = 0, failed_paths = 0;
  for (u64 t = 0; t < ticks; ++t) {
    sim::push_due_orders(*world, spec);
    if (autoplay) bot.step(*world);
    world->tick();
    if (autoplay && world->mission != sim::MissionState::Running) break;
    if (world->now % kTicksPerDay == kTicksPerHour * 10) {
      att_present = att_employed = 0;
      if (verbose && world->mission_days % 20 == 10) {
        u32 acts[32] = {}, reasons[64] = {};
        const auto& C = world->citizens;
        for (u32 i = 0; i < C.ids.high_water(); ++i)
          if (C.ids.alive_index(i) && C.work[i].valid()) {
            ++acts[static_cast<int>(C.activity[i])];
            ++reasons[static_cast<int>(C.reason[i])];
          }
        std::printf("    employed at 10:00 by activity:");
        for (int k = 0; k < static_cast<int>(sim::Activity::Count); ++k)
          if (acts[k]) std::printf(" %s=%u", sim::activity_name(static_cast<sim::Activity>(k)), acts[k]);
        std::printf("\n    by reason:");
        for (int k = 0; k < static_cast<int>(sim::Reason::Count); ++k)
          if (reasons[k] > 5) std::printf(" [%s]=%u", sim::reason_text(static_cast<sim::Reason>(k)), reasons[k]);
        std::printf("\n");
      }
      world->buildings.for_each([&](BuildingId, const sim::Building& b) {
        if (b.state == sim::BState::Active && defs.b(b.def).recipe.valid()) {
          att_present += b.present;
          att_employed += b.employed;
        }
      });
    }
    if (daily && world->now % kTicksPerDay == kTicksPerDay / 2) {
      u32 food = 0, nofood = 0, hh = 0, idle = 0, adults = 0, jobs = 0;
      for (GoodId g : world->food_slots) food += world->econ.city_stock[g.v];
      for (u32 k = 0; k < world->households.ids.high_water(); ++k)
        if (world->households.ids.alive_index(k)) {
          ++hh;
          if (world->household_food(world->households.h[k]) == 0) ++nofood;
        }
      const auto& C = world->citizens;
      for (u32 i = 0; i < C.ids.high_water(); ++i) {
        if (!C.ids.alive_index(i)) continue;
        const u32 a = C.age_years(i, world->now);
        if (a < 14 || a >= 60) continue;
        ++adults;
        if (!C.work[i].valid()) ++idle;
      }
      world->buildings.for_each([&](BuildingId, const sim::Building& b) { jobs += b.jobs; });
      std::printf("day %3u pop %4u mood %3u food %5u hh %3u nofood %3u adults %4u idle %3u jobs %4u treas %6lld fav %3d imm %u emi %u %s\n",
                  world->mission_days, world->population(), world->avg_happiness, food, hh, nofood, adults, idle, jobs,
                  static_cast<long long>(world->econ.treasury), world->favor, world->immigrants_total, world->emigrants_total,
                  autoplay ? bot.last_action().c_str() : "");
      if (verbose) {
        // Attendance at workshops at the last mid-morning sample.
        std::printf("    attendance at 10:00: %u of %u workshop workers present\n", att_present, att_employed);
      }
      if (verbose && world->mission_days % 10 == 0) {
        i64 a[13] = {};
        u32 n = 0;
        const auto& C2 = world->citizens;
        for (u32 i = 0; i < C2.ids.high_water(); ++i) {
          if (!C2.ids.alive_index(i) || (C2.flags[i] & sim::kFlagVisitor)) continue;
          auto p = world->happiness_parts(i);
          const i16 v[13] = {p.food, p.water, p.rest, p.hygiene, p.religion, p.entertainment, p.variety, p.housing,
                             p.employment, p.taxes, p.gods, p.events, p.health};
          for (int k = 0; k < 13; ++k) a[k] += v[k];
          ++n;
        }
        const char* nm[13] = {"food", "water", "rest", "hyg", "rel", "ent", "var", "house", "emp", "tax", "gods", "ev", "hlth"};
        std::printf("    mood parts:");
        for (int k = 0; k < 13; ++k) std::printf(" %s %.0f", nm[k], n ? static_cast<double>(a[k]) / n : 0.0);
        std::printf("\n");
      }
      if (verbose && autoplay) {
        std::printf("    bot:");
        for (const auto& a : bot.take_log()) std::printf(" %s;", a.c_str());
        std::printf("\n");
      }
      if (verbose) {
        std::printf("    money:");
        for (int k = 0; k < econ::kMoneyCount; ++k)
          if (world->econ.money.yesterday[k])
            std::printf(" %s %s%lld", econ::money_name(static_cast<econ::Money>(k)),
                        econ::money_is_income(static_cast<econ::Money>(k)) ? "+" : "-",
                        static_cast<long long>(world->econ.money.yesterday[k]));
        std::printf("\n");
      }
      for (const auto& r : world->requests)
        if (r.active && r.kind == 0)
          std::printf("    request %s %u/%u, %lld days left\n", defs.goods[r.good.v].id.c_str(),
                      world->stored_amount(r.good), r.amount,
                      static_cast<long long>((r.deadline - world->now) / kTicksPerDay));
      if (verbose)
        for (const auto& r : world->requests)
          if (r.active && r.kind == 0)
            world->buildings.for_each([&](BuildingId, const sim::Building& b) {
              const auto& d = defs.b(b.def);
              if (!d.recipe.valid()) return;
              for (const auto& o : defs.recipes[d.recipe.v].outputs)
                if (o.good == r.good)
                  std::printf("      %s state %d employed %u/%u present %u has %u free %u\n", d.id.c_str(),
                              static_cast<int>(b.state), b.employed, b.jobs, b.present, world->econ.amt(b.inv, r.good),
                              world->econ.free_space(b.inv));
            });
    }
    ms.push_back(world->stats.tick_ms);
    decisions += world->stats.decisions;
    path_req += world->stats.path_requests;
    arrivals += world->stats.arrivals;
    max_walking = std::max(max_walking, world->stats.walking);
    fallbacks += world->paths.stats().fallbacks;
    failed_paths += world->paths.stats().failed;
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
  const double p999 = steady[std::min(steady.size() - 1, steady.size() * 999 / 1000)];
  u64 over8 = 0, over16 = 0;
  for (double v : steady) {
    over8 += v > 8.0 ? 1 : 0;
    over16 += v > 16.0 ? 1 : 0;
  }
  const double per_frame_4x = avg * 80.0 / 60.0;
  const u64 viol = core::alloc::violations();
  const auto& ps = world->paths.store();
  std::printf(
      "{\"scenario\":\"%s\",\"ticks\":%llu,\"citizens\":%u,\"avg_tick_ms\":%.3f,\"p99_tick_ms\":%.3f,"
      "\"p999_tick_ms\":%.3f,\"ticks_over_8ms\":%llu,\"ticks_over_16ms\":%llu,"
      "\"max_tick_ms\":%.3f,\"sim_ms_per_frame_4x\":%.3f,\"alloc_violations\":%llu,\"decisions\":%llu,"
      "\"path_requests\":%llu,\"arrivals\":%llu,\"max_walking\":%u,\"path_cache_hits\":%llu,"
      "\"path_cache_misses\":%llu,\"paths_live\":%u,\"sched_dropped\":%llu}\n",
      spec.name.c_str(), static_cast<unsigned long long>(ticks), world->citizens.ids.count(), avg, p99, p999,
      static_cast<unsigned long long>(over8), static_cast<unsigned long long>(over16), mx,
      per_frame_4x, static_cast<unsigned long long>(viol), static_cast<unsigned long long>(decisions),
      static_cast<unsigned long long>(path_req), static_cast<unsigned long long>(arrivals), max_walking,
      static_cast<unsigned long long>(ps.hits()), static_cast<unsigned long long>(ps.misses()), ps.live_paths(),
      static_cast<unsigned long long>(world->sched.dropped()));
  if (verbose) {
    const char* names[8] = {"commands", "services", "daily", "labor", "matcher", "events", "paths", "-"};
    std::printf("path fallbacks %llu, failed %llu\n", static_cast<unsigned long long>(fallbacks),
                static_cast<unsigned long long>(failed_paths));
    std::printf("system ms (avg / max):");
    for (int k = 0; k < 7; ++k) std::printf(" %s %.3f/%.2f", names[k], sys_sum[k] / static_cast<double>(ticks), sys_max[k]);
    std::printf("\n");
  }
  if (autoplay)
    std::printf("autoplay: %u actions, mission %s after %u days (pop %u, cul %u, pros %u, favor %d)%s%s\n", bot.actions,
                world->mission == sim::MissionState::Won ? "WON" : world->mission == sim::MissionState::Lost ? "LOST" : "running",
                world->mission_days, world->population(), world->ratings.culture, world->ratings.prosperity, world->favor,
                world->lost_reason.empty() ? "" : " - ", world->lost_reason.c_str());
  if (autoplay || verbose) {
    const auto& G = world->goals;
    std::printf("goals:");
    if (G.population) std::printf(" pop %u/%u", world->population(), G.population);
    if (G.culture) std::printf(" cul %u/%u", world->ratings.culture, G.culture);
    if (G.prosperity) std::printf(" pros %u/%u", world->ratings.prosperity, G.prosperity);
    if (G.security) std::printf(" sec %u/%u", world->ratings.security, G.security);
    if (G.requests) std::printf(" requests %u/%u (failed %u)", world->requests_fulfilled, G.requests, world->requests_failed);
    for (const auto& g : G.buildings) std::printf(" %s %u/%u", defs.b(g.def).id.c_str(), world->count_active(g.def), g.count);
    std::printf("\n");
  }
  if (verbose) {
    u32 per_side[2] = {}, units_side[2] = {};
    f32 ex = 0, ey = 0;
    for (const auto& u : world->army.units)
      if (u.alive) {
        per_side[u.side] += u.count;
        ++units_side[u.side];
        if (u.side == 1) {
          ex = u.ax;
          ey = u.ay;
        }
      }
    std::printf("army: city %u soldiers in %u units, enemy %u in %u units (one at %.0f,%.0f); lost %u, killed %u\n",
                per_side[0], units_side[0], per_side[1], units_side[1], static_cast<double>(ex), static_cast<double>(ey),
                world->soldiers_lost_, world->enemies_killed_);
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
    {
      // Coverage of occupied houses by the basic services.
      const data::HousingReq water{data::HousingReq::Service, {defs.service("well"), defs.service("fountain")}, {}, 0};
      const data::HousingReq market{data::HousingReq::Service, {defs.service("market")}, {}, 0};
      u32 occ = 0, no_water = 0, no_market = 0;
      world->buildings.for_each([&](BuildingId, const sim::Building& b) {
        if (defs.b(b.def).kind != data::BuildingKind::House || b.residents == 0) return;
        ++occ;
        if (!world->requirement_met(b, water)) ++no_water;
        if (!world->requirement_met(b, market)) ++no_market;
      });
      std::printf("occupied houses %u: without water %u, without a market in reach %u\n", occ, no_water, no_market);
      u32 farms = 0, fert_sum = 0, farm_tiles = 0, farm_workers = 0, farm_present = 0;
      world->buildings.for_each([&](BuildingId, const sim::Building& b) {
        if (b.state != sim::BState::Active || defs.b(b.def).kind != data::BuildingKind::Farm) return;
        ++farms;
        farm_workers += b.employed;
        farm_present += b.present;
        for (i32 y = b.y; y < b.y + b.h; ++y)
          for (i32 x = b.x; x < b.x + b.w; ++x) {
            fert_sum += world->map.fertility[static_cast<size_t>(world->map.idx(x, y))];
            ++farm_tiles;
          }
      });
      std::printf("farms %u (%u tiles, avg fertility %u, workers %u)\n", farms, farm_tiles,
                  farm_tiles ? fert_sum / farm_tiles : 0, farm_workers);
      (void)farm_present;
    }
    std::printf("money yesterday:");
    for (int k = 0; k < econ::kMoneyCount; ++k)
      if (world->econ.money.yesterday[k])
        std::printf(" %s %lld", econ::money_name(static_cast<econ::Money>(k)),
                    static_cast<long long>(world->econ.money.yesterday[k]));
    std::printf("  treasury %lld\n", static_cast<long long>(world->econ.treasury));
    std::printf("goods (stock/made/used/imp/exp/price):");
    for (u32 g = 0; g < world->econ.goods; ++g)
      if (world->econ.city_stock[g] || world->econ.produced_yday[g] || world->econ.consumed_yday[g])
        std::printf(" %s%s %u/%u/%u/%u/%u/%.1f", defs.goods[g].id.c_str(),
                    world->trade_mode[g] == 1 ? "[imp]" : world->trade_mode[g] == 2 ? "[exp]" : world->trade_mode[g] == 3 ? "[pile]" : "",
                    world->econ.city_stock[g],
                    world->econ.produced_yday[g], world->econ.consumed_yday[g], world->econ.imported_yday[g],
                    world->econ.exported_yday[g], world->econ.price[g] / 10.0);
    std::printf("\n");
    {
      u64 sav = 0, nh = 0, poor = 0, nofood = 0;
      for (u32 k = 0; k < world->households.ids.high_water(); ++k)
        if (world->households.ids.alive_index(k)) {
          const auto& h = world->households.h[k];
          sav += static_cast<u64>(std::max(0, h.savings));
          ++nh;
          if (h.savings < 5) ++poor;
          if (world->household_food(h) == 0) ++nofood;
        }
      for (size_t p = 0; p < world->routes.size(); ++p)
      if (world->routes[p].open)
      {
        u32 sold = 0, bought = 0;
        for (u32 g = 0; g < world->econ.goods; ++g) {
          sold += world->routes[p].sold[g];
          bought += world->routes[p].bought[g];
        }
        std::printf("route %s next_visit=%llu (now %llu) sold %u bought %u traded=%u\n", defs.partners[p].id.c_str(),
                    static_cast<unsigned long long>(world->routes[p].next_visit), static_cast<unsigned long long>(world->now),
                    sold, bought, world->routes[p].caravan_traded);
      }
    std::printf("entry tile %d, visitors %u\n", world->entry_tile, world->visitors);
    std::printf("households %llu, avg savings %.1f, poor %llu, no food %llu\n", static_cast<unsigned long long>(nh),
                  nh ? static_cast<double>(sav) / static_cast<double>(nh) : 0.0, static_cast<unsigned long long>(poor),
                  static_cast<unsigned long long>(nofood));
    }
    {
      u32 cls[8] = {}, ages[5] = {}, married = 0;
      for (u32 i = 0; i < C.ids.high_water(); ++i) {
        if (!C.ids.alive_index(i) || (C.flags[i] & sim::kFlagVisitor)) continue;
        ++cls[static_cast<int>(C.cls[i])];
        const u32 a = C.age_years(i, world->now);
        ++ages[a < 14 ? 0 : a < 30 ? 1 : a < 50 ? 2 : a < 70 ? 3 : 4];
        if (C.spouse[i].valid()) ++married;
      }
      std::printf("classes: patrician %u eques %u pleb %u libertus %u slave %u | ages <14 %u 14-29 %u 30-49 %u 50-69 %u 70+ %u | married %u\n",
                  cls[0], cls[1], cls[2], cls[3], cls[4], ages[0], ages[1], ages[2], ages[3], ages[4], married);
      std::printf("births %u deaths %u riots %u crimes yesterday %u\n", world->births_total, world->deaths_total,
                  world->riots_total, world->crimes_yday);
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
      std::printf("%-14s at %3d,%3d ent=%d state=%d jobs=%u emp=%u present=%u prog=%u/%u wp=%u inv:", d.id.c_str(), b.x, b.y, b.entrance,
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
    // Why are construction sites unstaffed? Count eligible idle workers that can reach each one.
    world->buildings.for_each([&](BuildingId, const sim::Building& b) {
      if (b.state != sim::BState::Construction || b.employed >= b.jobs) return;
      u32 idle = 0, homeless = 0, leaving = 0, reach = 0;
      const auto& C = world->citizens;
      for (u32 i = 0; i < C.ids.high_water(); ++i) {
        if (!C.ids.alive_index(i) || C.work[i].valid()) continue;
        const u32 a = C.age_years(i, world->now);
        if (a < 14 || a >= 60) continue;
        ++idle;
        if (!C.home[i].valid() || !world->buildings.alive(C.home[i])) { ++homeless; continue; }
        if (C.activity[i] == sim::Activity::Leave) { ++leaving; continue; }
        if (world->nav.connected(world->buildings[C.home[i]].entrance, b.entrance)) ++reach;
      }
      std::printf("site %s: ready %d idle %u homeless %u leaving %u can reach %u\n", defs.b(b.def).id.c_str(),
                  world->site_ready(b), idle, homeless, leaving, reach);
    });
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
