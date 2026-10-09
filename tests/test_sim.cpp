#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <memory>

#include "data/defs.hpp"
#include "path/hpa.hpp"
#include "path/nav.hpp"
#include "path/search.hpp"
#include "save/save.hpp"
#include "sim/scenario.hpp"
#include "sim/scheduler.hpp"
#include "sim/world.hpp"

using namespace urbs;

namespace {
const data::Defs& defs() {
  static data::Defs d = data::load_defs(std::string(URBS_SOURCE_DIR) + "/data");
  return d;
}
std::string scenario(const char* name) { return std::string(URBS_SOURCE_DIR) + "/tests/scenarios/" + name; }
}  // namespace

TEST_CASE("defs load and cross-reference") {
  const auto& d = defs();
  CHECK(d.goods.size() > 30);
  CHECK(d.buildings.size() > 80);
  CHECK(d.building("bakery").valid());
  CHECK(d.recipes[d.b(d.building("bakery")).recipe.v].id == "bake_bread");
  CHECK(d.service("market").valid());
}

TEST_CASE("scheduler pops due events sorted and spills over budget") {
  sim::Scheduler s;
  s.init(1000, 6);  // small wheel to exercise overflow
  s.set_now(0);
  for (u32 i = 0; i < 10; ++i) s.schedule({5, 10 - i, 1, 0, sim::EventKind::Citizen});
  s.schedule({500, 1, 1, 0, sim::EventKind::Citizen});  // overflow (wheel = 64)
  core::FixedVec<sim::Event> out;
  out.init(1000);
  for (Tick t = 1; t < 5; ++t) {
    s.pop_due(t, 100, out);
    CHECK(out.empty());
  }
  s.pop_due(5, 4, out);
  REQUIRE(out.size() == 4);
  CHECK(out[0].target == 1);
  CHECK(out[3].target == 4);
  out.clear();
  s.pop_due(6, 100, out);
  CHECK(out.size() == 6);
  out.clear();
  u32 got = 0;
  for (Tick t = 7; t <= 600; ++t) {
    s.pop_due(t, 100, out);
    for (auto& e : out) {
      CHECK(e.when == 500);
      CHECK(t == 500);
      ++got;
    }
    out.clear();
  }
  CHECK(got == 1);
  CHECK(s.size() == 0);
}

TEST_CASE("HPA paths are valid and close to optimal") {
  sim::TileMap map;
  sim::MapGenParams p;
  p.size = 128;
  p.seed = 11;
  p.river = true;
  sim::generate_map(map, p);
  path::NavGrid nav;
  nav.init(map);
  path::Hpa hpa;
  hpa.init(nav);
  hpa.rebuild_dirty(nav);
  path::PathScratch sc;
  sc.init(hpa.total_nodes());
  path::GridSearch full;
  full.init(static_cast<u32>(map.w * map.h));
  rng::Stream r(3, 3);
  int tested = 0;
  for (int k = 0; k < 300 && tested < 60; ++k) {
    i32 a = static_cast<i32>(r.below(static_cast<u32>(map.w * map.h)));
    i32 b = static_cast<i32>(r.below(static_cast<u32>(map.w * map.h)));
    if (!nav.connected(a, b)) continue;
    bool fb = false;
    u32 c = hpa.find(nav, a, b, sc, &fb);
    u32 opt = full.astar(nav, {0, 0, map.w, map.h}, a, b, nullptr);
    REQUIRE(opt != path::kNoPath);
    if (fb) continue;
    REQUIRE(c != path::kNoPath);
    CHECK(sc.tiles.front() == a);
    CHECK(sc.tiles.back() == b);
    for (size_t i = 1; i < sc.tiles.size(); ++i) {
      i32 dx = sc.tiles[i] % map.w - sc.tiles[i - 1] % map.w, dy = sc.tiles[i] / map.w - sc.tiles[i - 1] / map.w;
      CHECK(std::abs(dx) <= 1);
      CHECK(std::abs(dy) <= 1);
      CHECK(nav.cost[static_cast<size_t>(sc.tiles[i])] != 0);
    }
    CHECK(c >= opt);
    INFO("a=" << a % map.w << "," << a / map.w << " b=" << b % map.w << "," << b / map.w << " len=" << sc.tiles.size());
    CHECK(c <= opt * 13 / 10 + 50);  // HPA* is near-optimal
    ++tested;
  }
  CHECK(tested > 20);
}

TEST_CASE("simulation is deterministic and save/load round-trips exactly") {
  auto spec = sim::load_scenario(scenario("small_town.json"));
  auto a = std::make_unique<sim::World>();
  auto b = std::make_unique<sim::World>();
  sim::setup_scenario(*a, defs(), spec);
  sim::setup_scenario(*b, defs(), spec);
  for (int t = 0; t < 700; ++t) {
    a->tick();
    b->tick();
  }
  CHECK(save::world_hash(*a) == save::world_hash(*b));

  const std::string path = (std::filesystem::temp_directory_path() / "urbs_test_save.urbs").string();
  std::string err;
  REQUIRE(save::save_world(*a, path, "test", &err));
  auto c = std::make_unique<sim::World>();
  REQUIRE_MESSAGE(save::load_world(*c, defs(), path, &err), err);
  CHECK(save::world_hash(*c) == save::world_hash(*a));
  for (int t = 0; t < 900; ++t) {
    a->tick();
    c->tick();
  }
  CHECK(save::world_hash(*c) == save::world_hash(*a));
  CHECK(a->citizens.ids.count() == c->citizens.ids.count());
  std::remove(path.c_str());
}
