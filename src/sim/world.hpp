#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/pool.hpp"
#include "core/rng.hpp"
#include "data/defs.hpp"
#include "econ/econ.hpp"
#include "path/nav.hpp"
#include "path/path_system.hpp"
#include "sim/buildings.hpp"
#include "sim/citizens.hpp"
#include "sim/map.hpp"
#include "sim/scheduler.hpp"
#include "sim/services.hpp"
#include "sim/time.hpp"

namespace enki {
class TaskScheduler;
}
namespace urbs::save {
class Writer;
class Reader;
}  // namespace urbs::save

namespace urbs::sim {

struct WorldConfig {
  u32 max_citizens = 16384;
  u32 max_buildings = 4096;
  u32 max_households = 8192;
  u32 threads = 0;  // 0 = hardware concurrency
  u32 path_budget = 256;
  u32 decision_budget = 1200;
  u32 service_budget = 60000;  // tiles settled per tick across service maps
  u64 seed = 1;
};

// ---- Commands: the only way the outside world mutates the sim (applied at tick start) -----------
enum class CmdType : u8 { PlaceBuilding, PlaceRoad, Demolish, SetPolicy, OpenRoute, SetTrade, PlaceUnder, Debug };
// SetPolicy ids (Command::def): value in Command::value.
enum class PolicyId : u16 { Wage, TaxPatrician, TaxEques, TaxPleb, TaxLibertus, Rations, SlaveTreatment, Manumission,
                            Curfew, Conscription, MaxInsula, Immigration, Count };

struct TradeRouteState {
  u8 open = 0;
  u8 available = 1;    // offered in this scenario
  Tick next_visit = 0;
  Tick ship_until = 0;  // sea: ship docked until this tick
  BuildingId ship_at;
  u8 caravan_traded = 1;  // land: set once the current caravan has exchanged goods
  std::vector<u16> bought, sold;  // per good, this year
};
struct Command {
  CmdType type = CmdType::Debug;
  u16 def = 0;    // building def / road type / policy id
  i32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  i32 value = 0;
  u8 flags = 0;   // PlaceBuilding: bit0 = instant (no construction)
};

enum class CitizenEvent : u16 { Decide = 1, Arrive = 2 };
enum class BuildingEvent : u16 { Hourly = 1 };

enum class Sector : u8 { Construction, Food, Industry, Distribution, Services, Government, Military, Count };
inline constexpr int kSectorCount = static_cast<int>(Sector::Count);
inline constexpr const char* sector_name(Sector s) {
  constexpr const char* n[] = {"Construction", "Food", "Industry", "Distribution", "Services", "Government", "Military"};
  return n[static_cast<int>(s)];
}

struct Policy {
  u8 sector_priority[kSectorCount] = {1, 2, 4, 3, 4, 5, 5};  // 1 = highest
  u8 tax_rate[6] = {10, 8, 6, 6, 0, 0};                      // percent of income per class
  u16 wage = 5;                                              // denarii per worker per day (paid by the city)
  u8 rations = 1;        // grain dole (annona): 0 none, 1 staples free for the poor, 2 staples free for all
  u8 slave_treatment = 1;  // 0 harsh, 1 fair, 2 kind
  u8 manumission = 1;      // 0 forbidden, 1 occasional, 2 encouraged
  u8 curfew = 0;
  u8 conscription = 0;
  u8 max_insula_tier = 4;  // building code
  u8 immigration_open = 1;
};

enum class AlertKind : u8 {
  HouseDevolved, HouseNeeds, NoWorkers, InputsMissing, MarketEmpty, Homeless, Unemployment, NoRoad, StorageFull,
  Immigrants, Emigrants, TreasuryLow, Fire, Collapse, Disease, Crime, Riot, Revolt, Raid, GodAngry, Request, Event,
  Count
};

struct Alert {
  Tick t = 0;
  AlertKind kind = AlertKind::Event;
  u16 arg = 0;
  BuildingId b;
  u32 count = 1;
};

struct TickStats {
  u32 events = 0, decisions = 0, arrivals = 0, path_requests = 0, path_results = 0;
  u32 walking = 0;
  f64 tick_ms = 0;
  // Per-system time this tick (ms): commands, services, daily passes, labor, matcher, events, paths.
  f64 sys_ms[8] = {};
};

struct World {
  WorldConfig cfg;
  const data::Defs* defs = nullptr;
  Tick now = 0;

  TileMap map;
  path::NavGrid nav;
  path::PathSystem paths;
  Scheduler sched;
  ServiceMaps services;
  Buildings buildings;
  Citizens citizens;
  Households households;
  rng::Stream rng[static_cast<size_t>(rng::StreamId::Count)];
  econ::Econ econ;
  // Household food slots: index -> GoodId of a food good (resolved from defs at init).
  std::vector<GoodId> food_slots;
  std::vector<GoodId> hh_goods;  // household goods slots (pottery, clothing, oil, wine, ...)
  std::vector<i8> good_food_slot, good_hh_slot;  // per good: slot index or -1

  Policy policy;
  std::vector<TradeRouteState> routes;  // one per data::TradePartnerDef
  std::vector<u8> trade_mode;           // per good: 0 none, 1 import, 2 export
  std::vector<u16> trade_level;         // import below / export above this city stock
  u32 visitors = 0;                     // traders currently in the city
  [[nodiscard]] u32 population() const { return citizens.ids.count() - visitors; }
  std::vector<Alert> alerts;  // newest last; capped
  i32 entry_tile = -1;        // where immigrants arrive and emigrants leave
  u32 immigrants_today = 0, emigrants_today = 0, immigrants_total = 0, emigrants_total = 0;
  i32 appeal = 0;             // last computed city appeal
  std::vector<ServiceId> need_services[8];  // services satisfying each data::NeedKind

  std::vector<Command> commands;  // pending, applied at next tick start
  TickStats stats;
  u32 walking = 0;

  World();
  ~World();
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  // Sets up all systems for an existing map (map must be generated/loaded first).
  void init(const WorldConfig& cfg, const data::Defs& defs);
  void tick();
  void push(const Command& c) { commands.push_back(c); }

  rng::Stream& stream(rng::StreamId id) { return rng[static_cast<size_t>(id)]; }

  // --- Structure (used by commands, scenarios and tests) ---
  BuildingId place_building(BuildingDefId def, i32 x, i32 y, i32 w, i32 h, bool instant);
  bool can_place(BuildingDefId def, i32 x, i32 y, i32 w, i32 h) const;
  void place_road(i32 x, i32 y, Road type);
  void place_road_line(i32 x0, i32 y0, i32 x1, i32 y1, Road type);
  void demolish(i32 x0, i32 y0, i32 x1, i32 y1);
  void remove_building(BuildingId id);
  void complete_construction(BuildingId id);
  [[nodiscard]] i32 entrance_tile(BuildingId id) const { return buildings[id].entrance; }

  // --- Citizens ---
  // Spawns at `home`, or walking in from `at_tile` when at_tile >= 0.
  CitizenId spawn_citizen(CitizenClass cls, BuildingId home, HouseholdId hh, u32 age_years, i32 at_tile = -1);
  void despawn_citizen(CitizenId id);
  HouseholdId create_household(BuildingId home, CitizenClass cls);
  // World-px position for rendering at fractional tick `alpha` past `now`. Returns false if indoors.
  bool citizen_position(u32 i, f32 alpha, Vec2& out_px, int* facing) const;

  // --- Services ---
  void refresh_service_providers();

  enki::TaskScheduler* jobs() { return jobs_.get(); }

  // Save/load of dynamic state (map and config are handled by save::save_world).
  void serialize(save::Writer& w) const;
  void deserialize(save::Reader& r);

  // Event helpers
  void schedule_citizen(u32 i, Tick when, CitizenEvent ev);
  void request_walk(u32 i, i32 from_tile, BuildingId to, i32 to_tile, Activity next);

  // --- Economy (sim/economy.cpp) ---
  bool pay(i64 denarii, econ::Money category = econ::Money::Construction);
  void earn(i64 denarii, econ::Money category);
  void spend(i64 denarii, econ::Money category);  // may go into debt
  // Partner price for one unit in tenths of a denarius. buying = city buys from partner.
  [[nodiscard]] u32 trade_price(u32 partner, GoodId g, bool buying) const;
  void trade_daily();
  void trade_exchange(u32 partner, BuildingId post);
  [[nodiscard]] BuildingId nearest_trade_post(bool sea, i32 near_tile) const;
  [[nodiscard]] bool is_porter_job(BuildingId b) const;
  void cancel_task(u32 task, bool goods_lost);
  void household_eat(u32 citizen);
  void household_shop(u32 citizen, BuildingId market);
  [[nodiscard]] u32 household_food(const Household& h) const;
  void add_stock(BuildingId b, GoodId g, u16 n);
  u32 labor_hourly_runs = 0;

  // --- Infrastructure (sim/infrastructure.cpp) ---
  bool water_dirty = true, desir_dirty = true;
  void place_underground_line(i32 x0, i32 y0, i32 x1, i32 y1, u8 bit, bool place);
  void recompute_water();
  void recompute_desirability();
  [[nodiscard]] i32 building_desirability(const Building& b) const;

  // --- Society (sim/housing.cpp) ---
  [[nodiscard]] Sector sector_of(const Building& b) const;
  void add_alert(AlertKind k, BuildingId b, u16 arg = 0);
  [[nodiscard]] bool requirement_met(const Building& b, const data::HousingReq& r) const;
  [[nodiscard]] int first_unmet(const Building& b, u8 tier) const;  // index into tier reqs or -1
  BuildingId find_vacant_house(i32 near_tile, u16 space, u8 line = 0) const;
  void start_emigration(HouseholdId hh);
  struct HappinessParts {
    i16 food = 0, water = 0, rest = 0, hygiene = 0, religion = 0, entertainment = 0, variety = 0, housing = 0,
        employment = 0, taxes = 0, gods = 0, events = 0, health = 0, education = 0, prestige = 0;
    [[nodiscard]] i32 total() const {
      return 128 + food + water + rest + hygiene + religion + entertainment + variety + housing + employment + taxes +
             gods + events + health + education + prestige;
    }
  };
  [[nodiscard]] HappinessParts happiness_parts(u32 citizen) const;
  [[nodiscard]] u8 food_kinds(const Household& h) const;
  [[nodiscard]] std::string citizen_name(u32 i) const;  // UI only (allocates)
  BuildingId best_service_for(data::NeedKind need, i32 from_tile, u16* cost = nullptr) const;
  i32 happy_sum_today = 0;
  u32 happy_count_today = 0;
  u8 avg_happiness = 128;

 private:
  void apply_commands();
  void process_events();
  void apply_path_results();
  void on_citizen_event(const Event& e);
  void on_building_event(const Event& e);
  void building_hourly(BuildingId id);
  void labor_allocate();
  void match_good(GoodId g);
  bool claim_task(u32 i);
  void haul_step(u32 i);
  void daily_update();
  void release_worker(u32 i);
  void house_daily(BuildingId id);
  void citizen_daily(u32 i);
  void immigration_hourly();
  void finish_leave(u32 i);
  void decide(u32 i);
  void arrive(u32 i);
  void start_activity(u32 i, Activity a);
  [[nodiscard]] i32 current_tile(u32 i) const;
  [[nodiscard]] i32 jitter(u32 i) const;
  void compute_entrance(Building& b);
  void update_nav_rect(i32 x, i32 y, i32 w, i32 h);

  std::unique_ptr<enki::TaskScheduler> jobs_;
  core::FixedVec<Event> due_;
  core::FixedVec<path::PathResult> results_;
  std::vector<BuildingId> scratch_ids_;
  std::vector<i32> scratch_tiles_;
  std::vector<u32> scratch_u32_, scratch_u32b_;
  std::vector<u32> labor_head_, labor_fill_;
  std::vector<i32> water_queue_;
  std::vector<u8> reservoir_full_;
  std::vector<i16> desir_acc_;
  bool providers_dirty_ = true;
};

}  // namespace urbs::sim
