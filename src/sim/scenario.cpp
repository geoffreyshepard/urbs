#include "sim/scenario.hpp"

#include <algorithm>
#include <filesystem>
#include "save/save.hpp"
#include "script/lua_host.hpp"
#include <cmath>
#include <cstdio>
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
  return parse_scenario(ss.str(), path);
}

ScenarioSpec parse_scenario(const std::string& text, const std::string& path) {
  ScenarioSpec s;
  s.json = text;
  s.path = path;
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
    s.map.arid = m.value("arid", 0.0f);
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
  s.industry = j.value("industry", false);
  s.ticks = j.value("ticks", 0ull);
  if (j.contains("test")) {
    auto& t = j["test"];
    s.expect_min_citizens = t.value("min_citizens", 0u);
    s.expect_max_tick_ms = t.value("max_tick_ms", 0.0);
    s.expect_max_alloc_violations = t.value("max_alloc_violations", 0xFFFFFFFFu);
    if (t.contains("stock"))
      for (auto& [k, v] : t["stock"].items()) s.expect_stock.emplace_back(k, v.get<u32>());
    s.min_treasury_end = t.value("min_treasury", s.min_treasury_end);
    s.expect_min_births = t.value("min_births", 0u);
    s.expect_min_deaths = t.value("min_deaths", 0u);
    s.expect_max_ruins = t.value("max_ruins", 0xFFFFFFFFu);
    if (t.contains("produced"))
      for (auto& [k, v] : t["produced"].items()) s.expect_produced.emplace_back(k, v.get<u32>());
    if (t.contains("active"))
      for (auto& [k, v] : t["active"].items()) s.expect_active.emplace_back(k, v.get<u32>());
    s.expect_mission = t.value("mission", std::string());
    s.expect_lost_reason = t.value("lost_reason", std::string());
  }
  s.map.flat = j.contains("map") && j["map"].value("flat", false);
  if (j.contains("map_file"))
    s.map_file = (std::filesystem::path(path).parent_path() / j["map_file"].get<std::string>()).string();
  return s;
}

namespace {
Road parse_road(const std::string& s) {
  if (s == "dirt") return Road::Dirt;
  if (s == "bridge") return Road::Bridge;
  return Road::Paved;
}
}  // namespace

void push_due_orders(World& w, const ScenarioSpec& spec) {
  for (const auto& [t, c] : spec.orders)
    if (t == w.now + 1) w.push(c);
}

bool check_expectations(const World& w, const ScenarioSpec& spec) {
  bool ok = true;
  if (!spec.expect_mission.empty()) {
    const char* state = w.mission == MissionState::Won ? "won" : w.mission == MissionState::Lost ? "lost" : "running";
    if (spec.expect_mission != state) {
      std::fprintf(stderr, "FAIL: mission %s, expected %s (%s)\n", state, spec.expect_mission.c_str(), w.lost_reason.c_str());
      ok = false;
    }
  }
  if (!spec.expect_lost_reason.empty() && w.lost_reason.find(spec.expect_lost_reason) == std::string::npos) {
    std::fprintf(stderr, "FAIL: loss reason \"%s\" does not mention \"%s\"\n", w.lost_reason.c_str(),
                 spec.expect_lost_reason.c_str());
    ok = false;
  }
  for (const auto& [gid, minv] : spec.expect_stock) {
    GoodId g = w.defs->good(gid);
    u64 total = 0;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      if (b.inv != econ::kNoInv && g.valid()) total += w.econ.amt(b.inv, g);
    });
    if (total < minv) {
      std::fprintf(stderr, "FAIL: stock of %s is %llu < %u\n", gid.c_str(), static_cast<unsigned long long>(total), minv);
      ok = false;
    }
  }
  if (w.births_total < spec.expect_min_births || w.deaths_total < spec.expect_min_deaths) {
    std::fprintf(stderr, "FAIL: births %u (min %u), deaths %u (min %u)\n", w.births_total, spec.expect_min_births,
                 w.deaths_total, spec.expect_min_deaths);
    ok = false;
  }
  {
    u32 ruins = 0;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      if (b.state == BState::Ruin) ++ruins;
    });
    if (ruins > spec.expect_max_ruins) {
      std::fprintf(stderr, "FAIL: %u ruins > %u\n", ruins, spec.expect_max_ruins);
      ok = false;
    }
  }
  if (w.econ.treasury < spec.min_treasury_end) {
    std::fprintf(stderr, "FAIL: treasury %lld < %lld\n", static_cast<long long>(w.econ.treasury),
                 static_cast<long long>(spec.min_treasury_end));
    ok = false;
  }
  for (const auto& [gid, minv] : spec.expect_produced) {
    GoodId g = w.defs->good(gid);
    u32 n = g.valid() ? w.econ.produced_yday[g.v] + w.econ.produced_today[g.v] : 0;
    if (n < minv) {
      std::fprintf(stderr, "FAIL: produced %s = %u < %u\n", gid.c_str(), n, minv);
      ok = false;
    }
  }
  for (const auto& [bid, minv] : spec.expect_active) {
    BuildingDefId d = w.defs->building(bid);
    u32 n = 0;
    w.buildings.for_each([&](BuildingId, const Building& b) {
      if (b.def == d && b.state == BState::Active) ++n;
    });
    if (n < minv) {
      std::fprintf(stderr, "FAIL: active %s = %u < %u\n", bid.c_str(), n, minv);
      ok = false;
    }
  }
  return ok;
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

namespace {
void setup_scripted(World& w, const data::Defs& defs, ScenarioSpec& s) {
  json j = json::parse(s.json, nullptr, true, true);
  if (!j.contains("start")) return;
  auto& st = j["start"];
  w.econ.treasury = st.value("treasury", 0);
  if (st.contains("roads"))
    for (auto& r : st["roads"])
      w.place_road_line(r["from"][0], r["from"][1], r["to"][0], r["to"][1], parse_road(r.value("type", "paved")));
  std::vector<BuildingId> placed;
  if (st.contains("buildings"))
    for (auto& b : st["buildings"]) {
      BuildingDefId d = defs.building(b["def"].get<std::string>());
      if (!d) throw std::runtime_error("scenario: unknown building " + b["def"].get<std::string>());
      const auto& bd = defs.b(d);
      // "rot": quarter turns clockwise (0 = front faces south); odd turns swap the def's size.
      const u8 rot = static_cast<u8>(b.value("rot", 0) & 3);
      const data::Footprint fp = data::rotated_size(bd, rot);
      i32 bw = b.contains("size") ? b["size"][0].get<i32>() : fp.w,
          bh = b.contains("size") ? b["size"][1].get<i32>() : fp.h;
      BuildingId id = w.place_building(d, b["x"], b["y"], bw, bh, true, rot);
      if (!id) throw std::runtime_error("scenario: cannot place " + bd.id + " at " + b["x"].dump() + "," + b["y"].dump());
      if (b.contains("stock"))
        for (auto& [g, n] : b["stock"].items()) w.add_stock(id, defs.good(g), n.get<u16>());
      placed.push_back(id);
    }
  if (st.contains("households")) {
    rng::Stream& r = w.stream(rng::StreamId::World);
    for (auto& h : st["households"]) {
      BuildingId home = placed.at(h["building"].get<size_t>());
      u32 count = h.value("count", 1u), members = h.value("members", 4u);
      for (u32 k = 0; k < count; ++k) {
        HouseholdId hh = w.create_household(home, CitizenClass::Pleb);
        w.households[hh].food = static_cast<u16>(h.value("food", 8u));
        w.households[hh].savings = h.value("savings", 100);
        for (u32 m = 0; m < members; ++m) w.spawn_citizen(CitizenClass::Pleb, home, hh, m < 2 ? 20 + r.below(20) : 15 + r.below(30));
      }
    }
  }
}
}  // namespace

namespace {
// Procedural town: grid roads, 8x8 blocks. Ring lots (2x2) on block edges, some blocks hold 3x3 lots
// for big buildings, the outer ring holds farms. Seeds `citizens` people into the houses.
void setup_town(World& w, const data::Defs& defs, const ScenarioSpec& s) {
  const i32 sp = 9, W = w.map.w, H = w.map.h;
  const i32 blocks = std::max<i32>(2, static_cast<i32>(std::sqrt(static_cast<double>(s.houses) / 10.0)) + 2);
  const i32 x0 = W / 2 - blocks * sp / 2, y0 = H / 2 - blocks * sp / 2;
  for (i32 k = 0; k <= blocks; ++k) {
    w.place_road_line(x0, y0 + k * sp, x0 + blocks * sp, y0 + k * sp, Road::Dirt);
    w.place_road_line(x0 + k * sp, y0, x0 + k * sp, y0 + blocks * sp, Road::Dirt);
  }
  // Entry road to the west edge.
  w.place_road_line(0, y0 + (blocks / 2) * sp, x0, y0 + (blocks / 2) * sp, Road::Dirt);
  auto B = [&](const char* id) { return defs.building(id); };
  std::vector<BuildingId> houses;
  u32 placed_houses = 0;
  const char* small_cycle[] = {"house_plot", "house_plot", "house_plot", "well", "house_plot", "house_plot",
                               "shrine",     "house_plot", "vigiles_post", "well", "latrina",  "house_plot",
                               "house_plot", "tavern",     "house_plot", "well", "engineers_post", "physician"};
  const char* big_plain[] = {"market", "granary", "warehouse", "bakery", "mill", "domus_plot", "temple", "lumber_camp",
                             "market", "slave_quarters", "domus_plot", "forum"};
  const char* big_ind[] = {"market", "granary", "forum",    "potter",   "clay_pit", "trade_post", "potter", "bakery",
                           "mill",   "clay_pit", "warehouse", "temple",  "forum",    "potter",     "weaver", "balneum",
                           "market", "clay_pit", "potter",    "granary", "lumber_camp", "forum",   "sawmill", "school"};
  const char** big_cycle = s.industry ? big_ind : big_plain;
  const u32 big_n = s.industry ? 24 : 12;
  u32 sc = 0, bc = 0;
  for (i32 by = 0; by < blocks; ++by)
    for (i32 bx = 0; bx < blocks; ++bx) {
      const i32 ox = x0 + bx * sp + 1, oy = y0 + by * sp + 1;
      const bool outer = bx == 0 || by == 0 || bx == blocks - 1 || by == blocks - 1;
      if (outer && (bx + by) % 2 == 0) {
        const char* farm = (bx + by) % 4 == 0 ? "wheat_farm" : "vegetable_garden";
        if (s.industry && (bx * 3 + by) % 7 == 0) farm = "sheep_farm";
        w.place_building(B(farm), ox, oy, 8, 8, true);
        continue;
      }
      if ((bx * 7 + by * 3) % 5 == 0) {
        const i32 lx[4] = {0, 5, 0, 5}, ly[4] = {0, 0, 5, 5};
        for (int k = 0; k < 4; ++k) {
          BuildingDefId d = B(big_cycle[bc++ % big_n]);
          const auto& bd = defs.b(d);
          if (bd.w > 3 || bd.h > 3) {  // too big for a 3x3 lot: use a market instead
            d = B("market");
          }
          const auto& bd2 = defs.b(d);
          if (bd2.resource)
            for (i32 ty = oy + ly[k]; ty < oy + ly[k] + bd2.h; ++ty)
              for (i32 tx = ox + lx[k]; tx < ox + lx[k] + bd2.w; ++tx)
                w.map.resource[static_cast<size_t>(w.map.idx(tx, ty))] = static_cast<Resource>(bd2.resource);
          w.place_building(d, ox + lx[k], oy + ly[k], bd2.w, bd2.h, true);
        }
        continue;
      }
      // Ring of 2x2 lots: along the four edges of the 8x8 block.
      const i32 ring[12][2] = {{0, 0}, {2, 0}, {4, 0}, {6, 0}, {6, 2}, {6, 4}, {6, 6}, {4, 6}, {2, 6}, {0, 6}, {0, 4}, {0, 2}};
      for (auto& l : ring) {
        const char* id = small_cycle[sc++ % 18];
        BuildingDefId d = B(id);
        const auto& bd = defs.b(d);
        BuildingId b = w.place_building(d, ox + l[0], oy + l[1], bd.w, bd.h, true);
        if (b && bd.kind == data::BuildingKind::House) {
          houses.push_back(b);
          ++placed_houses;
        }
      }
    }
  // Starting stock and population.
  w.buildings.for_each([&](BuildingId id, const Building& b) {
    const auto& d = defs.b(b.def);
    if (d.id == "granary") w.add_stock(id, defs.good("wheat"), 150);
    if (d.id == "warehouse") {
      w.add_stock(id, defs.good("wood"), 80);
      w.add_stock(id, defs.good("pottery"), 40);
    }
  });
  rng::Stream& r = w.stream(rng::StreamId::World);
  u32 spawned = 0;
  for (size_t hi = 0; spawned < s.citizens && hi < houses.size() * 3; ++hi) {
    BuildingId h = houses[hi % houses.size()];
    if (w.buildings[h].residents + 4 > w.buildings[h].capacity) continue;
    HouseholdId hh = w.create_household(h, CitizenClass::Pleb);
    w.households[hh].food = 12;
    w.households[hh].water = 4;
    w.households[hh].savings = 120;
    const u32 size = 3 + r.below(2);
    for (u32 m = 0; m < size && spawned < s.citizens; ++m, ++spawned)
      w.spawn_citizen(CitizenClass::Pleb, h, hh, m < 2 ? 18 + r.below(30) : r.below(18));
  }
  w.econ.treasury = 5000;
  URBS_LOG_INFO("town: {} houses, {} citizens", placed_houses, spawned);
}
}  // namespace

namespace {
void setup_trade(World& w, const data::Defs& defs, const ScenarioSpec& s) {
  json j = json::parse(s.json, nullptr, true, true);
  if (!j.contains("trade")) return;
  auto& t = j["trade"];
  if (t.contains("partners")) {
    for (auto& r : w.routes) r.available = 0;
    for (auto& p : t["partners"])
      for (size_t k = 0; k < defs.partners.size(); ++k)
        if (defs.partners[k].id == p.get<std::string>()) w.routes[k].available = 1;
  }
  if (t.contains("open"))
    for (auto& p : t["open"])
      for (size_t k = 0; k < defs.partners.size(); ++k)
        if (defs.partners[k].id == p.get<std::string>()) {
          w.routes[k].open = 1;
          w.routes[k].available = 1;
          w.routes[k].next_visit = kTicksPerHour * 8;
        }
  auto set = [&](const char* key, u8 mode) {  // "import", "export", "stockpile"
    if (!t.contains(key)) return;
    for (auto& [g, lvl] : t[key].items()) {
      GoodId gid = defs.good(g);
      if (!gid) throw std::runtime_error("scenario trade: unknown good " + g);
      w.trade_mode[gid.v] = mode;
      w.trade_level[gid.v] = lvl.get<u16>();
    }
  };
  set("import", 1);
  set("export", 2);
  set("stockpile", 3);
}

void setup_orders(const data::Defs& defs, ScenarioSpec& s) {
  json j = json::parse(s.json, nullptr, true, true);
  if (j.contains("orders"))
    for (auto& o : j["orders"]) {
      Command c;
      Tick t = o.value("tick", 1ull);
      if (o.contains("place")) {
        BuildingDefId d = defs.building(o["place"].get<std::string>());
        if (!d) throw std::runtime_error("scenario order: unknown building " + o["place"].get<std::string>());
        c.type = CmdType::PlaceBuilding;
        c.def = d.v;
        c.x0 = o["x"];
        c.y0 = o["y"];
        const u8 rot = static_cast<u8>(o.value("rot", 0) & 3);
        const data::Footprint fp = data::rotated_size(defs.b(d), rot);
        c.x1 = o.value("w", fp.w);
        c.y1 = o.value("h", fp.h);
        c.flags = static_cast<u8>(rot << 1);
      } else if (o.contains("road")) {
        c.type = CmdType::PlaceRoad;
        c.def = static_cast<u16>(parse_road(o.value("type", "paved")));
        c.x0 = o["road"][0];
        c.y0 = o["road"][1];
        c.x1 = o["road"][2];
        c.y1 = o["road"][3];
      } else if (o.contains("trigger")) {
        static const char* kTrig[] = {"fire", "collapse", "plague", "riot", "revolt", "flood", "earthquake",
                                      "drought", "storm", "blight", "blessing", "wrath"};
        c.type = CmdType::Trigger;
        c.def = 0xFFFF;
        for (u16 k = 0; k < 12; ++k)
          if (o["trigger"].get<std::string>() == kTrig[k]) c.def = k;
        if (c.def == 0xFFFF) throw std::runtime_error("scenario order: unknown trigger " + o["trigger"].get<std::string>());
        c.x0 = o.value("x", -1);
        c.y0 = o.value("y", -1);
        c.value = o.value("value", 0);
      } else if (o.contains("demolish")) {
        c.type = CmdType::Demolish;
        c.x0 = o["demolish"][0];
        c.y0 = o["demolish"][1];
        c.x1 = o["demolish"][2];
        c.y1 = o["demolish"][3];
      }
      s.orders.emplace_back(t, c);
    }
}

void setup_policy(World& w, const ScenarioSpec& s) {
  json j = json::parse(s.json, nullptr, true, true);
  if (j.contains("policy")) {
    auto& p = j["policy"];
    w.policy.slave_treatment = p.value("slave_treatment", w.policy.slave_treatment);
    w.policy.manumission = p.value("manumission", w.policy.manumission);
    w.policy.rations = p.value("rations", w.policy.rations);
    w.policy.wage = p.value("wage", w.policy.wage);
  }
  if (j.contains("start_slaves")) w.buy_public_slaves(j["start_slaves"].get<u32>());
  if (j.contains("battle")) {
    auto& b = j["battle"];
    const i32 c = w.city_target_tile();
    i32 k = 0;
    if (b.contains("city"))
      for (auto& e : b["city"]) {
        UnitDefId d = w.defs->unit(e[0].get<std::string>());
        for (i32 n = 0; n < e[1].get<i32>(); ++n, ++k) {
          const f32 x = static_cast<f32>(c % w.map.w) + static_cast<f32>((k % 8) * 8 - 28);
          const f32 y = static_cast<f32>(c / w.map.w) + static_cast<f32>((k / 8) * 6 - 12);
          w.spawn_unit(d, 0, std::clamp(x, 2.0f, static_cast<f32>(w.map.w - 3)), std::clamp(y, 2.0f, static_cast<f32>(w.map.h - 3)),
                       w.defs->units[d.v].size, BuildingId{});
        }
      }
    if (b.contains("enemy"))
      for (auto& e : b["enemy"]) w.spawn_raid(w.defs->unit(e[0].get<std::string>()), e[1].get<u16>(), b.value("side", 3));
  }
}
}  // namespace

namespace {
// Lays a dirt road from the map edge (side: 0 n, 1 e, 2 s, 3 w) to the most central walkable tile.
void entry_road(World& w, i32 side) {
  const i32 W = w.map.w, H = w.map.h;
  // The heart of the map: the most central walkable tile.
  i32 goal = -1;
  for (i32 r = 0; r < W / 2 && goal < 0; ++r)
    for (i32 dy = -r; dy <= r && goal < 0; ++dy)
      for (i32 dx = -r; dx <= r && goal < 0; ++dx)
        if (w.nav.walkable(W / 2 + dx, H / 2 + dy)) goal = w.map.idx(W / 2 + dx, H / 2 + dy);
  // The edge tile nearest the middle of the requested side that is connected to it (never an island);
  // other sides are tried if that one has none.
  i32 start = -1;
  for (i32 s2 = 0; s2 < 4 && start < 0 && goal >= 0; ++s2) {
    const i32 sd = (side + s2) % 4;
    for (i32 k = 0; k < (sd % 2 == 0 ? W : H) / 2 && start < 0; ++k)
      for (i32 sgn : {1, -1}) {
        const i32 a = (sd % 2 == 0 ? W : H) / 2 + sgn * k;
        const i32 x = sd == 1 ? W - 1 : sd == 3 ? 0 : a, y = sd == 0 ? 0 : sd == 2 ? H - 1 : a;
        if (w.map.in_bounds(x, y) && w.nav.walkable(x, y) && w.nav.connected(w.map.idx(x, y), goal)) {
          start = w.map.idx(x, y);
          break;
        }
      }
  }
  if (start < 0 || goal < 0) return;
  bool rev = false;
  const u32 h = w.paths.find_now(start, goal, w.now, &rev);
  if (h == path::kNoHandle) return;
  const u32 n = w.paths.store().length(h);
  for (u32 k = 0; k < n; ++k) {
    const Vec2i t = w.paths.store().tile(h, k);
    w.place_road(t.x, t.y, Road::Dirt);
  }
  w.paths.store().unref(h);
  w.entry_tile = start;
}

void setup_mission(World& w, const data::Defs& defs, ScenarioSpec& s) {
  json j = json::parse(s.json, nullptr, true, true);
  if (j.contains("entry_road")) entry_road(w, j["entry_road"].get<i32>());
  if (j.contains("start_stock")) {
    // A stockpile beside the end of the entry road holding the colonists' supplies.
    i32 cx = w.map.w / 2, cy = w.map.h / 2, best = 1 << 30;
    for (i32 i = 0; i < w.map.w * w.map.h; ++i)
      if (w.map.road[static_cast<size_t>(i)] != Road::None) {
        const i32 d = std::abs(i % w.map.w - w.map.w / 2) + std::abs(i / w.map.w - w.map.h / 2);
        if (d < best) {
          best = d;
          cx = i % w.map.w;
          cy = i / w.map.w;
        }
      }
    const BuildingDefId sp = defs.building("stockpile");
    BuildingId id;
    for (i32 r = 1; r < 12 && !id.valid(); ++r)
      for (i32 dy = -r; dy <= r && !id.valid(); ++dy)
        for (i32 dx = -r; dx <= r && !id.valid(); ++dx)
          if (w.can_place(sp, cx + dx, cy + dy, 3, 3)) id = w.place_building(sp, cx + dx, cy + dy, 3, 3, true);
    if (id.valid())
      for (auto& [g, n] : j["start_stock"].items()) w.add_stock(id, defs.good(g), n.get<u16>());
  }
  w.mission_id = j.value("id", s.name);
  w.region = j.value("region", std::string());
  if (j.contains("mission")) w.unlock_up_to(j["mission"].get<u8>());
  w.difficulty = j.value("difficulty", static_cast<u8>(1));
  w.empire = j.value("era", std::string("republic")) == "empire" ? 1 : 0;
  w.nile = j.value("nile", false) ? 1 : 0;
  w.favor = j.value("favor", 50);
  if (j.contains("unlock"))
    for (auto& b : j["unlock"]) {
      BuildingDefId d = defs.building(b.get<std::string>());
      if (d.valid()) w.unlocked[d.v] = 1;
    }
  if (j.contains("gods")) {
    std::fill(w.god_available.begin(), w.god_available.end(), u8{0});
    for (auto& g : j["gods"]) {
      const u8 k = defs.god(g.get<std::string>());
      if (k != 0xFF) w.god_available[k] = 1;
    }
  }
  if (j.contains("goals")) {
    auto& g = j["goals"];
    w.goals.population = g.value("population", 0u);
    w.goals.culture = g.value("culture", static_cast<u8>(0));
    w.goals.prosperity = g.value("prosperity", static_cast<u8>(0));
    w.goals.security = g.value("security", static_cast<u8>(0));
    w.goals.favor = g.value("favor", static_cast<u8>(0));
    w.goals.requests = g.value("requests", static_cast<u16>(0));
    w.goals.survive_days = g.value("survive_days", static_cast<u16>(0));
    w.goals.max_debt_days = g.value("max_debt_days", w.goals.max_debt_days);
    if (g.contains("buildings"))
      for (auto& [b, n] : g["buildings"].items()) {
        BuildingDefId d = defs.building(b);
        if (!d) throw std::runtime_error("scenario goals: unknown building " + b);
        w.goals.buildings.push_back(Goals::BuildingGoal{d, n.get<u16>()});
      }
  }
  if (j.contains("script")) {
    std::filesystem::path sp = std::filesystem::path(s.path).parent_path() / j["script"].get<std::string>();
    std::ifstream f(sp);
    if (!f) throw std::runtime_error("cannot open script " + sp.string());
    std::stringstream ss;
    ss << f.rdbuf();
    w.script_source = ss.str();
    w.script_name = sp.filename().string();
    auto host = std::make_unique<script::LuaHost>(w, w.script_source, w.script_name);
    if (!host->start()) throw std::runtime_error("script error: " + host->last_error());
    script::LuaHost* raw = host.get();
    w.script = std::move(host);
    raw->on_start();
  }
}
}  // namespace

void setup_scenario(World& w, const data::Defs& defs, ScenarioSpec& s) {
  if (!s.map_file.empty()) {
    std::string err;
    if (!save::load_map(w.map, s.map_file, &err)) throw std::runtime_error("scenario map: " + err);
    s.map.size = w.map.w;
  } else {
    generate_map(w.map, s.map);
  }
  {
    json j = json::parse(s.json, nullptr, true, true);
    w.now = static_cast<Tick>(j.value("start_hour", 0)) * kTicksPerHour;
  }
  if (s.generator == "stress_city") build_grid_city(w, defs, s);
  w.init(s.world, defs);
  if (s.generator == "scripted") setup_scripted(w, defs, s);
  if (s.generator == "town") setup_town(w, defs, s);
  setup_trade(w, defs, s);
  setup_policy(w, s);
  setup_orders(defs, s);
  setup_mission(w, defs, s);
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
    w.households[hh].savings = 100;
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
