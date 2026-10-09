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

TEST_CASE("water network: aqueduct flows downhill, fills reservoir, pipes feed a fountain") {
  sim::World w;
  sim::MapGenParams p;
  p.size = 64;
  p.flat = true;
  p.river = false;
  sim::generate_map(w.map, p);
  // Lake on the west edge; terrain falls from height 5 at the lake to 2 in the east.
  for (i32 y = 0; y < 64; ++y)
    for (i32 x = 0; x < 64; ++x) {
      const size_t i = static_cast<size_t>(w.map.idx(x, y));
      w.map.terrain[i] = x < 4 ? sim::Terrain::Shallow : sim::Terrain::Grass;
      w.map.height[i] = static_cast<u8>(x < 4 ? 0 : std::max(2, 5 - x / 10));
    }
  w.init(sim::WorldConfig{}, defs());
  const auto& D = defs();
  BuildingId a = w.place_building(D.building("aqueduct"), 4, 20, 20, 1, true);  // x 4..23, y 20
  REQUIRE(a.valid());
  BuildingId res = w.place_building(D.building("reservoir"), 24, 19, 3, 3, true);
  REQUIRE(res.valid());
  w.place_underground_line(27, 20, 35, 20, 1, true);
  BuildingId f = w.place_building(D.building("fountain"), 35, 21, 1, 1, true);
  REQUIRE(f.valid());
  // Uphill aqueduct: from the lake to a hill at height 7 must stay dry past the climb.
  for (i32 x = 4; x < 30; ++x) w.map.height[static_cast<size_t>(w.map.idx(x, 40))] = static_cast<u8>(x < 15 ? 3 : 7);
  BuildingId up = w.place_building(D.building("aqueduct"), 4, 40, 26, 1, true);
  REQUIRE(up.valid());
  w.recompute_water();
  CHECK((w.map.water[static_cast<size_t>(w.map.idx(23, 20))] & 1) != 0);
  CHECK((w.map.water[static_cast<size_t>(w.map.idx(30, 20))] & 2) != 0);
  CHECK((w.buildings[f].flags & 0x20) != 0);
  CHECK((w.map.water[static_cast<size_t>(w.map.idx(10, 40))] & 1) != 0);
  CHECK((w.map.water[static_cast<size_t>(w.map.idx(20, 40))] & 1) == 0);
  // Sewer draining into the lake.
  w.place_underground_line(4, 50, 20, 50, 2, true);
  w.recompute_water();
  CHECK((w.map.water[static_cast<size_t>(w.map.idx(20, 50))] & 4) != 0);
  // Aqueducts can be walked under.
  CHECK(w.nav.walkable(10, 20));
}

TEST_CASE("military: raise a cohort at a barracks, repel a raid") {
  auto spec = sim::load_scenario(scenario("town_1000.json"));
  auto w = std::make_unique<sim::World>();
  sim::setup_scenario(*w, defs(), spec);
  const auto& D = defs();
  // Barracks with gear on open ground next to the town's entry road.
  BuildingId bar;
  for (i32 y = 4; y < w->map.h - 8 && !bar.valid(); y += 3)
    for (i32 x = 4; x < 40 && !bar.valid(); x += 3)
      if (w->can_place(D.building("barracks"), x, y, 4, 4)) bar = w->place_building(D.building("barracks"), x, y, 4, 4, true);
  REQUIRE(bar.valid());
  w->add_stock(bar, D.good("weapons"), 60);
  w->add_stock(bar, D.good("armor"), 60);
  w->policy.conscription = 1;
  const u32 pop_before = w->population();
  REQUIRE(w->raise_unit(D.unit("legionary"), bar));
  CHECK(w->army.deployed >= 24);
  CHECK(w->population() < pop_before);
  // A raid arrives and fights; the sim keeps running without allocation.
  w->spawn_raid(D.unit("barbarian_warrior"), 1, 3);
  for (u16 u = 0; u < military::kMaxUnits; ++u)
    if (w->army.units[u].alive && w->army.units[u].side == 0) w->order_unit(u, military::Order::Move, w->city_target_tile());
  for (int t = 0; t < 3000 && w->enemies_killed_ + w->soldiers_lost_ == 0; ++t) w->tick();
  CHECK(w->enemies_killed_ + w->soldiers_lost_ > 0);
}

TEST_CASE("lua mission script: messages, requests, choices, save/load of script state") {
  auto spec = sim::load_scenario(scenario("mission_script.json"));
  auto w = std::make_unique<sim::World>();
  sim::setup_scenario(*w, defs(), spec);
  REQUIRE(w->script);
  CHECK(w->messages.size() == 1);
  for (int t = 0; t < 1200 * 5; ++t) w->tick();
  CHECK(w->requests_failed == 1);
  CHECK(w->favor < 50);
  CHECK(w->messages.size() >= 2);
  // Answer the choice: the Lua callback stores the option in urbs.state.
  u16 q = 0;
  for (const auto& m : w->messages)
    if (!m.options.empty()) q = m.id;
  REQUIRE(q != 0);
  w->answer(q, 2);
  const std::string st = w->script->export_state();
  CHECK(st.find("answer\tn\t2") != std::string::npos);
  CHECK(st.find("checked\tb\t1") != std::string::npos);
  // Save and load: script state and messages survive.
  const std::string path = (std::filesystem::temp_directory_path() / "urbs_script_save.urbs").string();
  std::string err;
  REQUIRE(save::save_world(*w, path, "test", &err));
  auto c = std::make_unique<sim::World>();
  REQUIRE_MESSAGE(save::load_world(*c, defs(), path, &err), err);
  REQUIRE(c->script);
  CHECK(c->script->export_state() == st);
  CHECK(c->messages.size() == w->messages.size());
  for (int t = 0; t < 600; ++t) {
    w->tick();
    c->tick();
  }
  CHECK(save::world_hash(*w) == save::world_hash(*c));
  std::remove(path.c_str());
}
