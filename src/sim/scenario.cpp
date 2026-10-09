#include "sim/scenario.hpp"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

#include "core/log.hpp"

namespace urbs::sim {

using json = nlohmann::json;

ScenarioSpec load_scenario(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open scenario " + path);
  std::stringstream ss;
  ss << f.rdbuf();
  ScenarioSpec s;
  s.json = ss.str();
  json j = json::parse(s.json, nullptr, true, true);
  s.name = j.value("name", path);
  if (j.contains("map")) {
    auto& m = j["map"];
    s.map.size = m.value("size", 192);
    s.map.seed = m.value("seed", 1ull);
    s.map.coast = m.value("coast", -1);
    s.map.river = m.value("river", true);
    s.map.hills = m.value("hills", 0.5f);
    s.map.forest = m.value("forest", 0.5f);
    s.map.marsh = m.value("marsh", 0.3f);
  }
  s.world.seed = j.value("seed", s.map.seed);
  if (j.contains("world")) {
    auto& w = j["world"];
    s.world.max_citizens = w.value("max_citizens", s.world.max_citizens);
    s.world.max_buildings = w.value("max_buildings", s.world.max_buildings);
    s.world.max_households = w.value("max_households", s.world.max_households);
    s.world.threads = w.value("threads", 0u);
    s.world.path_budget = w.value("path_budget", s.world.path_budget);
  }
  s.generator = j.value("generator", "empty");
  s.citizens = j.value("citizens", 0u);
  s.houses = j.value("houses", 0u);
  s.workplaces = j.value("workplaces", 0u);
  s.markets = j.value("markets", 0u);
  s.house_capacity = j.value("house_capacity", 0u);
  s.ticks = j.value("ticks", 0ull);
  if (j.contains("test")) {
    auto& t = j["test"];
    s.expect_min_citizens = t.value("min_citizens", 0u);
    s.expect_max_tick_ms = t.value("max_tick_ms", 0.0);
    s.expect_max_alloc_violations = t.value("max_alloc_violations", 0xFFFFFFFFu);
  }
  return s;
}

namespace {

// Lays a grid of paved roads and fills blocks (centre outwards) with the requested buildings.
void build_grid_city(World& w, const data::Defs& defs, const ScenarioSpec& s) {
  const i32 W = w.map.w, H = w.map.h;
  const i32 spacing = 9;  // road every 9 tiles: 8-tile blocks
  // Flatten the city area so the grid is regular: clear water/rock from the core.
  for (i32 y = 0; y < H; ++y)
    for (i32 x = 0; x < W; ++x) {
      size_t i = static_cast<size_t>(w.map.idx(x, y));
      if (w.map.terrain[i] != Terrain::Grass && w.map.terrain[i] != Terrain::Fertile) w.map.terrain[i] = Terrain::Grass;
      w.map.resource[i] = Resource::None;
      w.map.height[i] = 2;
    }
  for (i32 y = 0; y < H; y += spacing)
    for (i32 x = 0; x < W; ++x) w.map.road[static_cast<size_t>(w.map.idx(x, y))] = Road::Paved;
  for (i32 x = 0; x < W; x += spacing)
    for (i32 y = 0; y < H; ++y) w.map.road[static_cast<size_t>(w.map.idx(x, y))] = Road::Paved;
  w.map.mark_all_dirty();
}

struct Slot {
  i32 x, y;
  i32 d2;
};

}  // namespace

void setup_scenario(World& w, const data::Defs& defs, const ScenarioSpec& s) {
  generate_map(w.map, s.map);
  if (s.generator == "stress_city") build_grid_city(w, defs, s);
  w.init(s.world, defs);
  if (s.generator != "stress_city") return;

  const BuildingDefId house = defs.building("house_plot");
  const BuildingDefId work = defs.building("weaver");
  const BuildingDefId market = defs.building("market");
  // Candidate 2x2 / 3x3 slots inside blocks, sorted by distance from the map centre.
  const i32 spacing = 9;
  std::vector<Slot> slots;
  const i32 cx = w.map.w / 2, cy = w.map.h / 2;
  for (i32 by = 0; by + spacing < w.map.h; by += spacing)
    for (i32 bx = 0; bx + spacing < w.map.w; bx += spacing) {
      // Each 8x8 block holds four 3x3 lots along the roads.
      const i32 ox[4] = {1, 5, 1, 5}, oy[4] = {1, 1, 5, 5};
      for (int k = 0; k < 4; ++k) {
        i32 x = bx + ox[k], y = by + oy[k];
        i32 dx = x - cx, dy = y - cy;
        slots.push_back({x, y, dx * dx + dy * dy});
      }
    }
  std::stable_sort(slots.begin(), slots.end(), [](const Slot& a, const Slot& b) { return a.d2 < b.d2; });
  std::vector<BuildingId> houses, works, markets;
  rng::Stream& r = w.stream(rng::StreamId::World);
  size_t si = 0;
  const u32 total = s.houses + s.workplaces + s.markets;
  for (u32 n = 0; n < total && si < slots.size(); ++n, ++si) {
    // Interleave so every neighbourhood has a mix: 1 market per (total/markets) buildings.
    BuildingDefId def;
    std::vector<BuildingId>* list;
    u32 pick = n % 10;
    if (markets.size() < s.markets && pick == 0) {
      def = market;
      list = &markets;
    } else if (works.size() < s.workplaces && (pick % 3 == 1 || houses.size() >= s.houses)) {
      def = work;
      list = &works;
    } else if (houses.size() < s.houses) {
      def = house;
      list = &houses;
    } else if (works.size() < s.workplaces) {
      def = work;
      list = &works;
    } else {
      def = market;
      list = &markets;
    }
    const auto& bd = defs.b(def);
    BuildingId id = w.place_building(def, slots[si].x, slots[si].y, bd.w, bd.h, true);
    if (!id) continue;
    if (def == house && s.house_capacity) w.buildings[id].capacity = static_cast<u16>(s.house_capacity);
    if (def == work) w.buildings[id].jobs = 400;
    list->push_back(id);
  }
  URBS_LOG_INFO("stress city: {} houses, {} workplaces, {} markets", houses.size(), works.size(), markets.size());
  if (houses.empty()) return;

  // Households of ~5 in each house; 70% of adults work at one of the 4 nearest workplaces.
  u32 spawned = 0;
  size_t hi = 0;
  while (spawned < s.citizens) {
    BuildingId h = houses[hi % houses.size()];
    ++hi;
    Building& hb = w.buildings[h];
    if (hb.residents >= hb.capacity) {
      if (hi > houses.size() * 4) break;
      continue;
    }
    HouseholdId hh = w.create_household(h, CitizenClass::Pleb);
    if (!hh) break;
    w.households[hh].food = 10;
    // Nearest workplaces to this house.
    BuildingId near[4];
    i32 nd[4] = {1 << 30, 1 << 30, 1 << 30, 1 << 30};
    for (BuildingId wk : works) {
      const Building& b = w.buildings[wk];
      i32 d = std::abs(b.x - hb.x) + std::abs(b.y - hb.y);
      for (int k = 0; k < 4; ++k)
        if (d < nd[k]) {
          for (int m = 3; m > k; --m) {
            nd[m] = nd[m - 1];
            near[m] = near[m - 1];
          }
          nd[k] = d;
          near[k] = wk;
          break;
        }
    }
    for (int m = 0; m < 5 && spawned < s.citizens && hb.residents < hb.capacity; ++m) {
      CitizenId c = w.spawn_citizen(CitizenClass::Pleb, h, hh, 16 + r.below(40));
      if (!c) break;
      ++spawned;
      if (!works.empty() && r.chance(0.7f)) {
        BuildingId wk = near[r.below(std::min<u32>(4, static_cast<u32>(works.size())))];
        if (wk.valid()) {
          w.citizens.work[c.index()] = wk;
          ++w.buildings[wk].employed;
        }
      }
    }
  }
  URBS_LOG_INFO("stress city: {} citizens", spawned);
}

}  // namespace urbs::sim
