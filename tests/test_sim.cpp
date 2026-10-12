#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include "data/defs.hpp"
#include "path/hpa.hpp"
#include "path/nav.hpp"
#include "path/search.hpp"
#include "save/save.hpp"
#include "sim/chaos.hpp"
#include "sim/invariants.hpp"
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

TEST_CASE("monuments rise in stages as materials arrive, and dedication applies its effects") {
  sim::World w;
  sim::MapGenParams p;
  p.size = 64;
  p.flat = true;
  p.river = false;
  sim::generate_map(w.map, p);
  w.init(sim::WorldConfig{}, defs());
  const auto& D = defs();
  const BuildingDefId tj = D.building("temple_of_jupiter");
  REQUIRE(tj.valid());
  BuildingId m = w.place_building(tj, 20, 20, D.b(tj).w, D.b(tj).h, false);
  REQUIRE(m.valid());
  sim::Building& b = w.buildings[m];
  CHECK(b.jobs >= 8);
  CHECK(w.construction_stage(b) == 0);
  // With nothing delivered the site is not ready; a quarter of each material starts stage 1.
  CHECK_FALSE(w.site_ready(b));
  for (const auto& c : D.b(tj).cost_goods) w.econ.add(b.inv, c.good, static_cast<u16>((c.amount + 3) / 4));
  CHECK(w.site_ready(b));
  // Builders work up to the end of the stage, then wait for the next share.
  b.present = 40;
  for (int h = 0; h < 200; ++h) w.building_hourly(m);
  CHECK(b.state == sim::BState::Construction);
  CHECK(w.construction_stage(b) == 1);
  CHECK_FALSE(w.site_ready(b));
  // Deliver everything: it completes and is dedicated.
  for (const auto& c : D.b(tj).cost_goods) w.econ.add(b.inv, c.good, static_cast<u16>(c.amount - (c.amount + 3) / 4));
  const i32 favor0 = w.favor;
  for (int h = 0; h < 400 && w.buildings[m].state == sim::BState::Construction; ++h) w.building_hourly(m);
  CHECK(w.buildings[m].state == sim::BState::Active);
  CHECK(w.favor == std::min(100, favor0 + D.b(tj).dedication.favor));
}

TEST_CASE("map files round-trip and scenarios can load a painted map") {
  sim::TileMap m;
  sim::MapGenParams p;
  p.size = 64;
  p.seed = 42;
  sim::generate_map(m, p);
  // Paint something recognisable.
  for (i32 x = 10; x < 20; ++x) {
    m.terrain[static_cast<size_t>(m.idx(x, 30))] = sim::Terrain::Rock;
    m.resource[static_cast<size_t>(m.idx(x, 30))] = sim::Resource::Marble;
    m.road[static_cast<size_t>(m.idx(x, 31))] = sim::Road::Dirt;
  }
  const std::string dir = (std::filesystem::temp_directory_path() / "urbs_map_test").string();
  std::filesystem::create_directories(dir);
  std::string err;
  REQUIRE(save::save_map(m, dir + "/map.urbsmap", &err));
  sim::TileMap back;
  REQUIRE(save::load_map(back, dir + "/map.urbsmap", &err));
  CHECK(back.w == m.w);
  CHECK(back.terrain == m.terrain);
  CHECK(back.height == m.height);
  CHECK(back.resource == m.resource);
  CHECK(back.road == m.road);
  // A scenario referring to the map file starts on it.
  {
    std::ofstream f(dir + "/scenario.json");
    f << R"({"name": "painted", "map_file": "map.urbsmap", "generator": "scripted", "entry_road": 2,
             "start": {"treasury": 1000}})";
  }
  sim::ScenarioSpec spec = sim::load_scenario(dir + "/scenario.json");
  sim::World w;
  sim::setup_scenario(w, defs(), spec);
  CHECK(w.map.w == 64);
  CHECK(w.map.resource[static_cast<size_t>(w.map.idx(15, 30))] == sim::Resource::Marble);
  std::filesystem::remove_all(dir);
}

TEST_CASE("the main menu's backdrop city loads with evolved houses and families") {
  // data/menu/backdrop is a scripted scenario frozen from a bot-built city (tools/menu_backdrop.sh).
  // If a defs change breaks it, regenerate it; the game would otherwise show a plain landscape.
  sim::ScenarioSpec spec = sim::load_scenario(std::string(URBS_SOURCE_DIR) + "/data/menu/backdrop/scenario.json");
  sim::World w;
  sim::setup_scenario(w, defs(), spec);
  CHECK(w.buildings.ids.count() > 200);
  CHECK(w.citizens.ids.count() > 1000);
  u32 evolved = 0;
  w.buildings.for_each([&](BuildingId, const sim::Building& b) {
    if (defs().b(b.def).kind == data::BuildingKind::House && b.tier > 0) ++evolved;
  });
  CHECK(evolved > 20);
  for (int t = 0; t < 600; ++t) w.tick();
  CHECK(w.citizens.ids.count() > 1000);
}

TEST_CASE("scripted scenarios place evolved houses and families of a given class") {
  const char* text = R"({"name": "t", "map": {"size": 64, "seed": 3, "coast": -1, "river": false, "hills": 0},
    "generator": "scripted",
    "start": {"buildings": [{"def": "house_plot", "x": 30, "y": 30, "tier": 2}, {"def": "domus_plot", "x": 20, "y": 20}],
              "households": [{"building": 0, "count": 2, "members": 3},
                             {"building": 1, "count": 1, "members": 2, "class": "patrician"}]}})";
  sim::ScenarioSpec spec = sim::parse_scenario(text, "t.json");
  sim::World w;
  sim::setup_scenario(w, defs(), spec);
  BuildingId house, domus;
  w.buildings.for_each(
      [&](BuildingId id, const sim::Building& b) { (defs().b(b.def).id == "house_plot" ? house : domus) = id; });
  REQUIRE(house.valid());
  REQUIRE(domus.valid());
  CHECK(w.buildings[house].tier == 2);
  CHECK(w.buildings[house].capacity == defs().tier(defs().b(w.buildings[house].def).house_line, 2)->capacity);
  CHECK(w.citizens.ids.count() == 8);
  u32 patricians = 0;
  for (u32 i = 0; i < w.households.ids.high_water(); ++i)
    if (w.households.ids.alive_index(i) && w.households[w.households.ids.id_at(i)].cls == sim::CitizenClass::Patrician)
      ++patricians;
  CHECK(patricians == 1);
}

TEST_CASE("every good has a source and a use") {
  // A good nothing makes or sells is dead content; one nothing uses, eats or buys is a dead end.
  const auto& D = defs();
  std::vector<u8> source(D.goods.size(), 0), use(D.goods.size(), 0);
  for (const auto& r : D.recipes) {
    for (const auto& a : r.inputs) use[a.good.v] = 1;
    for (const auto& a : r.outputs) source[a.good.v] = 1;
  }
  for (const auto& p : D.partners) {
    for (const auto& a : p.sells) source[a.good.v] = 1;
    for (const auto& a : p.buys) use[a.good.v] = 1;
  }
  for (const auto& h : D.housing)
    for (const auto& q : h.reqs)
      for (GoodId g : q.goods) use[g.v] = 1;
  for (const auto& b : D.buildings)
    for (const auto& c : b.cost_goods) use[c.good.v] = 1;
  for (const auto& u : D.units)
    for (const auto& c : u.gear) use[c.good.v] = 1;
  for (size_t g = 0; g < D.goods.size(); ++g) {
    if (D.goods[g].food) use[g] = 1;
    CHECK_MESSAGE(source[g], D.goods[g].id << " has no source (no recipe makes it, no partner sells it)");
    CHECK_MESSAGE(use[g], D.goods[g].id << " has no use (nothing consumes, eats, builds with or buys it)");
  }
}

namespace {
std::unique_ptr<sim::World> flat_world(i32 size) {
  auto w = std::make_unique<sim::World>();
  sim::MapGenParams p;
  p.size = size;
  p.flat = true;
  p.river = false;
  sim::generate_map(w->map, p);
  w->init(sim::WorldConfig{}, defs());
  return w;
}
BuildingId put(sim::World& w, const char* id, i32 x, i32 y) {
  const BuildingDefId d = defs().building(id);
  return w.place_building(d, x, y, defs().b(d).w, defs().b(d).h, true);
}
}  // namespace

TEST_CASE("entrances follow their neighbours; a walled-in building raises a no-road alert") {
  auto w = flat_world(64);
  w->entry_tile = w->map.idx(0, 40);
  BuildingId a = put(*w, "potter", 20, 20);
  REQUIRE(a.valid());
  w->tick();
  const i32 e0 = w->buildings[a].entrance;
  REQUIRE(e0 >= 0);
  // A statue built on the potter's doorstep: the potter finds another way in.
  REQUIRE(put(*w, "statue", e0 % 64, e0 / 64).valid());
  w->tick();
  const i32 e1 = w->buildings[a].entrance;
  CHECK(e1 != e0);
  CHECK(w->nav.cost[static_cast<size_t>(e1)] != 0);
  CHECK(w->nav.connected(e1, w->entry_tile));
  CHECK((w->buildings[a].flags & sim::kBuildingCutOff) == 0);
  // A statue walled in on all sides has no way in at all.
  BuildingId s = put(*w, "statue", 40, 20);
  REQUIRE(s.valid());
  for (i32 dy = -1; dy <= 1; ++dy)
    for (i32 dx = -1; dx <= 1; ++dx)
      if (dx || dy) REQUIRE(put(*w, "wall", 40 + dx, 20 + dy).valid());
  w->tick();
  CHECK((w->buildings[s].flags & sim::kBuildingCutOff) != 0);
  bool alerted = false;
  for (const auto& al : w->alerts) alerted |= al.kind == sim::AlertKind::NoRoad && al.b == s;
  CHECK(alerted);
}

TEST_CASE("rotation: odd turns swap the footprint, the entrance prefers the front side") {
  const auto& D = defs();
  const BuildingDefId saw = D.building("sawmill");  // 3x2
  REQUIRE(saw.valid());
  REQUIRE(D.b(saw).rotatable);
  for (const char* id : {"wheat_farm", "plaza", "aqueduct", "wall", "pipe", "sewer", "bridge", "stockpile"})
    CHECK_MESSAGE(!D.b(D.building(id)).rotatable, id);
  CHECK(data::rotated_size(D.b(saw), 0).w == 3);
  CHECK(data::rotated_size(D.b(saw), 0).h == 2);
  CHECK(data::rotated_size(D.b(saw), 1).w == 2);
  CHECK(data::rotated_size(D.b(saw), 1).h == 3);
  CHECK(data::rotated_size(D.b(saw), 2).w == 3);
  CHECK(data::rotated_size(D.b(saw), 3).h == 3);

  auto w = flat_world(64);
  w->entry_tile = w->map.idx(0, 40);
  BuildingId ids[4];
  for (u8 rot = 0; rot < 4; ++rot) {
    const data::Footprint fp = data::rotated_size(D.b(saw), rot);
    const i32 x = 8 + rot * 12, y = 20;
    ids[rot] = w->place_building(saw, x, y, fp.w, fp.h, true, rot);
    REQUIRE(ids[rot].valid());
    const sim::Building& b = w->buildings[ids[rot]];
    CHECK(b.rot == rot);
    CHECK(b.w == fp.w);
    CHECK(b.h == fp.h);
    for (i32 ty = y; ty < y + fp.h; ++ty)
      for (i32 tx = x; tx < x + fp.w; ++tx) CHECK(w->map.building[static_cast<size_t>(w->map.idx(tx, ty))] == ids[rot]);
  }
  w->tick();
  for (u8 rot = 0; rot < 4; ++rot) {
    const sim::Building& b = w->buildings[ids[rot]];
    REQUIRE(b.entrance >= 0);
    const i32 ex = b.entrance % 64, ey = b.entrance / 64;
    INFO("rot " << int(rot) << " entrance " << ex << "," << ey);
    switch (rot) {
      case 0:
        CHECK((ey == b.y + b.h && ex >= b.x && ex < b.x + b.w));  // south
        break;
      case 1:
        CHECK((ex == b.x - 1 && ey >= b.y && ey < b.y + b.h));  // west
        break;
      case 2:
        CHECK((ey == b.y - 1 && ex >= b.x && ex < b.x + b.w));  // north
        break;
      default:
        CHECK((ex == b.x + b.w && ey >= b.y && ey < b.y + b.h));  // east
        break;
    }
  }
  // A blocked front: the door moves to a side before the back.
  {
    const sim::Building& b = w->buildings[ids[3]];  // faces east
    for (i32 ty = b.y - 1; ty <= b.y + b.h; ++ty) REQUIRE(put(*w, "wall", b.x + b.w, ty).valid());
    w->tick();
    const i32 ex = b.entrance % 64, ey = b.entrance / 64;
    CHECK((ey == b.y - 1 || ey == b.y + b.h));
    CHECK(ex >= b.x);
    CHECK(ex < b.x + b.w);
  }
  // Non-rotatable defs ignore the rotation.
  const BuildingDefId farm = D.building("wheat_farm");
  const BuildingId f = w->place_building(farm, 10, 40, 4, 4, true, 1);
  REQUIRE(f.valid());
  CHECK(w->buildings[f].rot == 0);
  // Players place through commands: rotation rides in flags bits 1-2.
  w->unlock_up_to(99);
  w->econ.treasury = 100000;
  sim::Command c;
  c.type = sim::CmdType::PlaceBuilding;
  c.def = saw.v;
  c.x0 = 30;
  c.y0 = 40;
  c.x1 = 2;
  c.y1 = 3;
  c.flags = static_cast<u8>(1 | (1 << 1));
  w->push(c);
  w->tick();
  const BuildingId placed = w->map.building[static_cast<size_t>(w->map.idx(30, 40))];
  REQUIRE(placed.valid());
  CHECK(w->buildings[placed].rot == 1);
  CHECK(w->buildings[placed].w == 2);
  CHECK(w->buildings[placed].h == 3);
  sim::InvariantChecker checker;
  std::vector<std::string> bad;
  checker.check(*w, bad);
  for (const auto& line : bad) CHECK_MESSAGE(line.find("rotation") == std::string::npos, line);
}

TEST_CASE("houses face the street: road_facing_rot picks the side with the most road") {
  auto w = flat_world(64);
  // A 2x2 lot at (20, 20): front lines are y 22 (rot 0), x 19 (rot 1), y 19 (rot 2), x 22 (rot 3).
  CHECK(w->road_facing_rot(20, 20, 2, 2) == 0);         // no road: rot 0
  w->place_road_line(20, 19, 21, 19, sim::Road::Dirt);  // north side, 2 tiles
  CHECK(w->road_facing_rot(20, 20, 2, 2) == 2);
  w->place_road(22, 20, sim::Road::Dirt);  // east side, 1 tile: north still wins
  CHECK(w->road_facing_rot(20, 20, 2, 2) == 2);
  w->place_road(22, 21, sim::Road::Dirt);  // east 2 = north 2: the camera-facing east wins the tie
  CHECK(w->road_facing_rot(20, 20, 2, 2) == 3);
  w->place_road_line(20, 22, 21, 22, sim::Road::Dirt);  // south 2: south first among ties
  CHECK(w->road_facing_rot(20, 20, 2, 2) == 0);
  // Corners do not count: a road only diagonal to the lot leaves it at rot 0.
  auto v = flat_world(64);
  v->place_road(19, 19, sim::Road::Dirt);
  CHECK(v->road_facing_rot(20, 20, 2, 2) == 0);
  v->place_road_line(19, 20, 19, 21, sim::Road::Dirt);  // west side
  CHECK(v->road_facing_rot(20, 20, 2, 2) == 1);
}

TEST_CASE("rotation survives save/load; older saves load with every building at rot 0") {
  const auto& D = defs();
  auto w = flat_world(64);
  w->entry_tile = w->map.idx(0, 40);
  const BuildingDefId saw = D.building("sawmill");
  const BuildingId a = w->place_building(saw, 10, 10, 2, 3, true, 1);
  const BuildingId b = w->place_building(saw, 20, 10, 3, 2, true, 2);
  const BuildingId c = w->place_building(D.building("bakery"), 30, 10, 2, 2, true, 3);
  REQUIRE(a.valid());
  REQUIRE(b.valid());
  REQUIRE(c.valid());
  for (int t = 0; t < 50; ++t) w->tick();
  const std::string path = (std::filesystem::temp_directory_path() / "urbs_test_rot.urbs").string();
  std::string err;
  REQUIRE(save::save_world(*w, path, "test", &err));
  save::SaveInfo info;
  REQUIRE(save::read_info(path, info));
  CHECK(info.version == save::kSaveVersion);
  auto l = std::make_unique<sim::World>();
  REQUIRE_MESSAGE(save::load_world(*l, D, path, &err), err);
  std::remove(path.c_str());
  CHECK(l->buildings[a].rot == 1);
  CHECK(l->buildings[b].rot == 2);
  CHECK(l->buildings[c].rot == 3);
  CHECK(l->buildings[a].entrance == w->buildings[a].entrance);
  CHECK(save::world_hash(*l) == save::world_hash(*w));

  // The v0.1 fixture (save version 1) predates rotation.
  auto old = std::make_unique<sim::World>();
  REQUIRE_MESSAGE(save::load_world(*old, D, std::string(URBS_SOURCE_DIR) + "/tests/saves/v1_capua_day25.urbs", &err),
                  err);
  u32 n = 0;
  old->buildings.for_each([&](BuildingId, const sim::Building& ob) {
    CHECK(ob.rot == 0);
    ++n;
  });
  CHECK(n > 0);
}

TEST_CASE("defences: gates let citizens through but not raiders, walls are besieged, towers shoot") {
  auto w = flat_world(64);
  w->entry_tile = w->map.idx(0, 32);
  const auto& D = defs();
  // The governor's palace is the raiders' target, ahead of the forum.
  REQUIRE(put(*w, "forum", 4, 4).valid());
  BuildingId pal = put(*w, "governor_palace", 30, 30);
  REQUIRE(pal.valid());
  w->tick();
  CHECK(w->city_target_tile() == w->buildings[pal].entrance);
  // Gate: walkable for citizens and city troops, a siege obstacle for raiders.
  REQUIRE(put(*w, "gate", 10, 10).valid());
  REQUIRE(put(*w, "wall", 12, 10).valid());
  const i32 gate = w->map.idx(10, 10), wall = w->map.idx(12, 10);
  CHECK(w->nav.walkable(10, 10));
  CHECK(w->field_cost(gate, 0) > 0);
  CHECK(w->field_cost(gate, 1) > 100);
  // Wall: blocks citizens; raiders must break through it.
  CHECK_FALSE(w->nav.walkable(12, 10));
  CHECK(w->field_cost(wall, 0) == 0);
  CHECK(w->field_cost(wall, 1) > 100);
  // A staffed tower shoots raiders in range.
  BuildingId tw = put(*w, "tower", 44, 44);
  REQUIRE(tw.valid());
  w->buildings[tw].employed = w->buildings[tw].jobs;
  const u16 u = w->spawn_unit(D.unit("barbarian_warrior"), 1, 46.0f, 50.0f, 4, BuildingId{});
  REQUIRE(u != military::kNoUnit);
  auto enemy_hp = [&] {
    i64 hp = 0;
    for (u32 j = 0; j < w->army.x.size(); ++j)
      if (w->army.unit[j] == u && w->army.hp[j] > 0) hp += w->army.hp[j];
    return hp;
  };
  const i64 hp0 = enemy_hp();
  REQUIRE(hp0 > 0);
  for (int t = 0; t < 400; ++t) w->tick();
  CHECK(enemy_hp() < hp0);
}

TEST_CASE("deliveries nobody can make expire instead of reserving goods forever") {
  // No citizens, so no porters: every haul stays unclaimed. Reservations must not outlive a day.
  auto w = flat_world(64);
  w->entry_tile = w->map.idx(0, 32);
  const auto& D = defs();
  BuildingId store = put(*w, "warehouse", 10, 30);
  REQUIRE(store.valid());
  w->add_stock(store, D.good("cut_stone"), 100);
  const BuildingDefId tj = D.building("temple_of_jupiter");
  BuildingId site = w->place_building(tj, 30, 30, D.b(tj).w, D.b(tj).h, false);
  REQUIRE(site.valid());
  bool planned = false;
  for (int t = 0; t < static_cast<int>(kTicksPerDay) * 3; ++t) {
    w->tick();
    for (u32 k = 0; k < w->econ.task_ids.high_water(); ++k) {
      if (!w->econ.task_ids.alive_index(k)) continue;
      planned = true;
      REQUIRE(w->now - w->econ.tasks[k].created <= kTicksPerDay + kTicksPerHour);
    }
  }
  CHECK(planned);
  // What the site counts as inbound is exactly what live hauls carry to it.
  u32 live = 0;
  for (u32 k = 0; k < w->econ.task_ids.high_water(); ++k)
    if (w->econ.task_ids.alive_index(k) && w->econ.tasks[k].dst == site && w->econ.tasks[k].good == D.good("cut_stone"))
      live += w->econ.tasks[k].amount;
  CHECK(w->econ.rin(w->buildings[site].inv, D.good("cut_stone")) == live);
}

TEST_CASE("goods staged for export are the city's again once stock is at the export level") {
  // 40 bricks staged at the trade post for export, a site that needs 12. Above the export level the
  // post keeps them for the caravans; at or below it they are not for sale and the site gets them.
  const auto& D = defs();
  const GoodId bricks = D.good("bricks");
  for (const i32 level : {10, 40}) {
    auto w = flat_world(64);
    w->entry_tile = w->map.idx(0, 32);
    BuildingId post = put(*w, "trade_post", 10, 30);
    REQUIRE(post.valid());
    w->add_stock(post, bricks, 40);
    w->trade_mode[bricks.v] = 2;
    w->trade_level[bricks.v] = static_cast<u16>(level);
    for (int t = 0; t < static_cast<int>(kTicksPerDay); ++t) w->tick();  // the daily count of city stock
    REQUIRE(w->econ.city_stock[bricks.v] == 40);
    const BuildingDefId bal = D.building("balneum");
    BuildingId site = w->place_building(bal, 30, 30, D.b(bal).w, D.b(bal).h, false);
    REQUIRE(site.valid());
    for (int t = 0; t < static_cast<int>(kTicksPerHour) * 6; ++t) w->tick();
    CAPTURE(level);
    CHECK((w->econ.rin(w->buildings[site].inv, bricks) > 0) == (level >= 40));
  }
}

TEST_CASE("a town stays consistent under a chaos player, and reloads exactly") {
  // Random building, demolition, policies, trade, armies and disasters for ten days: every counter and
  // reservation must still agree with a recount (sim/invariants.cpp), and a save must reload exactly.
  auto spec = sim::load_scenario(scenario("small_town.json"));
  auto w = std::make_unique<sim::World>();
  sim::setup_scenario(*w, defs(), spec);
  sim::Chaos chaos(42);
  sim::InvariantChecker checker;
  std::vector<std::string> bad;
  for (int t = 0; t < static_cast<int>(kTicksPerDay) * 10; ++t) {
    chaos.step(*w, 20);
    w->tick();
    if (w->now % kTicksPerDay == kTicksPerHour * 12) checker.check(*w, bad);
  }
  CHECK(chaos.actions > 1000);
  for (const auto& line : bad) MESSAGE(line);
  CHECK(bad.empty());
  const std::string path = (std::filesystem::temp_directory_path() / "urbs_chaos_save.urbs").string();
  std::string err;
  REQUIRE(save::save_world(*w, path, "test", &err));
  auto c = std::make_unique<sim::World>();
  REQUIRE_MESSAGE(save::load_world(*c, defs(), path, &err), err);
  for (int t = 0; t < static_cast<int>(kTicksPerHour) * 6; ++t) {
    w->tick();
    c->tick();
  }
  CHECK(save::world_hash(*w) == save::world_hash(*c));
  std::remove(path.c_str());
}
