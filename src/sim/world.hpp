#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/pool.hpp"
#include "core/rng.hpp"
#include "data/defs.hpp"
#include "econ/econ.hpp"
#include "military/army.hpp"
#include "path/nav.hpp"
#include "path/path_system.hpp"
#include "path/search.hpp"
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
  u32 max_soldiers = 6000;
  u32 threads = 0;  // 0 = hardware concurrency
  u32 path_budget = 256;
  u32 decision_budget = 1200;
  u32 service_budget = 60000;  // tiles settled per tick across service maps
  u64 seed = 1;
};

// ---- Commands: the only way the outside world mutates the sim (applied at tick start) -----------
enum class CmdType : u8 { PlaceBuilding, PlaceRoad, Demolish, SetPolicy, OpenRoute, SetTrade, PlaceUnder, BuySlaves,
                          Festival, Trigger, RaiseUnit, UnitOrder, UnitFormation, UnitStance, DisbandUnit, Campaign,
                          FulfillRequest, TakeLoan, Answer, Research, Grant, Debug };
// Trigger ids (Command::def) for debug/scripted threats; Command::x0,y0 = target tile when relevant.
enum class TriggerId : u16 { Fire, Collapse, Plague, Riot, Revolt, Flood, Earthquake, Drought, Storm, Blight,
                             Blessing, Wrath, Raid, Count };
enum class Weather : u8 { Clear, Rain, Heat, Storm, Snow, Count };
inline constexpr const char* weather_name(Weather w) {
  constexpr const char* n[] = {"clear", "rain", "heat", "storm", "snow"};
  return n[static_cast<int>(w)];
}
// Active timed effects (blessings, wrath, disasters). Ticks until they end.
// Civic research techniques with code hooks (ids in data/defs/techniques.json).
enum class Tech : u8 { CropRotation, WaterMills, Hypocaust, RomanConcrete, Arches, InsulaCodes, LegionDrill, Medicine,
                       Cursus, LatinSchools, Count };
enum class Effect : u8 { Favor, Births, Barren, Morale, Desertion, Happy, Discontent, Harvest, Blight, SeaTrade,
                         TradePrices, NoTrade, Workshops, Fireproof, Hearth, Health, Drought, Count };
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
  u8 sector_priority[kSectorCount] = {1, 2, 3, 2, 3, 4, 4};  // 1 = highest
  u8 tax_rate[6] = {10, 8, 6, 6, 0, 0};                      // percent of income per class
  u16 wage = 4;                                              // denarii per worker per day (paid by the city)
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
  Dedicated, Count
};

struct Alert {
  Tick t = 0;
  AlertKind kind = AlertKind::Event;
  u16 arg = 0;
  BuildingId b;
  u32 count = 1;
};

struct Ratings {
  u8 culture = 0, prosperity = 0, security = 50, favor = 50;
};

// A request from Rome: send goods, money or troops before the deadline.
struct Request {
  u8 active = 0;
  u8 kind = 0;  // 0 goods, 1 denarii, 2 troops (soldiers)
  GoodId good;
  u16 amount = 0;
  Tick deadline = 0;
  i32 reward = 0;      // denarii on success
  i8 favor_win = 8, favor_lose = -12;
  u16 id = 0;
};

struct Loan {
  i64 remaining = 0;
  i64 installment = 0;
};

struct Goals {
  u32 population = 0;
  u8 culture = 0, prosperity = 0, security = 0, favor = 0;
  u16 requests = 0;  // requests fulfilled
  u16 survive_days = 0;
  struct BuildingGoal {
    BuildingDefId def;
    u16 count = 0;
  };
  std::vector<BuildingGoal> buildings;
  // Failure (Civitas/Imperium): favor at 0, too long in debt, population collapse.
  u16 max_debt_days = 32;
  u8 fail_on_favor = 1;
};

// Narrative messages and choices raised by scripts or the game.
struct Message {
  u16 id = 0;
  Tick t = 0;
  u8 advisor = 0;  // 0 none, 1 engineer, 2 quaestor, 3 haruspex, 4 legate, 5 aedile
  u8 answered = 0;
  std::string title, text;
  std::vector<std::string> options;
  std::string callback;  // Lua global function called with the chosen option (1-based)
};

enum class MissionState : u8 { Running, Won, Lost };

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
  // Religion (sim/religion.cpp)
  std::vector<u8> god_mood;        // 0..100 per god
  std::vector<u8> god_available;   // worshipped in this city
  std::vector<u16> god_offerings;  // temple visits today per god
  Tick effect_until[static_cast<int>(Effect::Count)] = {};
  Tick festival_until = 0;
  u8 festival_god = 0xFF;
  Weather weather = Weather::Clear;
  u32 fires_total = 0, collapses_total = 0, plague_houses = 0;
  std::vector<TradeRouteState> routes;  // one per data::TradePartnerDef
  std::vector<u8> trade_mode;           // per good: 0 none, 1 import, 2 export, 3 stockpile (kept in storage: no markets or workshops)
  std::vector<u16> trade_level;         // import below / export above this city stock
  u32 visitors = 0;                     // traders currently in the city
  [[nodiscard]] u32 population() const { return citizens.ids.count() - visitors; }
  std::vector<Alert> alerts;  // newest last; capped
  i32 entry_tile = -1;        // where immigrants arrive and emigrants leave
  u32 immigrants_today = 0, emigrants_today = 0, immigrants_total = 0, emigrants_total = 0;
  i32 appeal = 0;             // last computed city appeal
  std::vector<ServiceId> need_services[8];  // services satisfying each data::NeedKind

  // Governance (sim/governance.cpp)
  Ratings ratings;
  i32 favor = 50;  // 0..100 (Senate in the Republic, Emperor in the Empire)
  u8 empire = 0;
  std::vector<Request> requests;
  u16 next_request_id = 1, requests_fulfilled = 0, requests_failed = 0;
  Loan loan;
  u16 debt_days = 0;
  Goals goals;
  MissionState mission = MissionState::Running;
  u16 mission_days = 0;
  std::string mission_id, lost_reason;
  std::vector<u8> unlocked;  // per building def
  u8 mission_number = 16;    // buildings with unlock <= this are available
  u8 difficulty = 1;         // 0 Otium, 1 Civitas, 2 Imperium
  // Sets the global difficulty; at the start of a mission it also scales the starting treasury.
  void set_difficulty(u8 d);
  // Multiplier for threat frequency and strength: Otium 0.5, Civitas 1, Imperium 1.5.
  [[nodiscard]] f32 threat_scale() const { return difficulty == 0 ? 0.5f : difficulty == 2 ? 1.5f : 1.0f; }
  std::vector<Message> messages;
  // Civic research
  u32 scientia = 0;
  std::vector<u8> tech_known;          // per data technique
  u8 tech_flags[static_cast<int>(Tech::Count)] = {};
  [[nodiscard]] bool tech(Tech t) const { return tech_flags[static_cast<int>(t)] != 0; }
  bool research(u16 technique);
  u16 next_message_id = 1;

  military::Army army;
  u32 soldiers_lost_ = 0, enemies_killed_ = 0;
  u16 buy_captives_ = 0;
  Tick nav_changed_tick_ = 0;

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
  // Load size for a haul between two buildings: cart loads for long hauls while carters work.
  [[nodiscard]] u16 carry_for(u32 src_bi, u32 dst_bi) const;
  void count_carters();
  u32 carters_ = 0;
  u8 crime_cut_ = 0;  // % from staffed courts and councils (recomputed daily; saved)
  void cancel_task(u32 task, bool goods_lost);
  void household_eat(u32 citizen);
  void household_shop(u32 citizen, BuildingId market);
  [[nodiscard]] u32 household_food(const Household& h) const;
  void add_stock(BuildingId b, GoodId g, u16 n);
  u32 labor_hourly_runs = 0;

  // --- Lifecycle and society (sim/lifecycle.cpp) ---
  [[nodiscard]] f32 trait_mul(u32 i, f32 data::TraitDef::*field) const {
    f32 m = 1.0f;
    for (int k = 0; k < kMaxTraits; ++k) {
      const u8 t = citizens.traits[static_cast<size_t>(i) * kMaxTraits + k];
      if (t < defs->traits.size()) m *= defs->traits[t].*field;
    }
    return m;
  }
  [[nodiscard]] bool has_trait(u32 i, const char* id) const;
  [[nodiscard]] static int skill_of_sector(Sector s) {
    switch (s) {
      case Sector::Food: return 0;
      case Sector::Industry: return 1;
      case Sector::Construction: return 2;
      case Sector::Distribution: return 3;
      case Sector::Military: return 4;
      default: return 5;
    }
  }
  void buy_public_slaves(u32 count, bool paid = true);
  u32 births_today = 0, deaths_today = 0, marriages_today = 0, crimes_today = 0, crimes_yday = 0, freed_today = 0;
  u32 births_total = 0, deaths_total = 0, riots_total = 0;

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
  bool ensure_entry_tile();
  void start_emigration(HouseholdId hh);
  struct HappinessParts {
    i16 food = 0, water = 0, rest = 0, hygiene = 0, religion = 0, entertainment = 0, variety = 0, housing = 0,
        employment = 0, taxes = 0, gods = 0, events = 0, health = 0, education = 0, prestige = 0, safety = 0,
        freedom = 0, traits = 0;
    [[nodiscard]] i32 total() const {
      return 128 + food + water + rest + hygiene + religion + entertainment + variety + housing + employment + taxes +
             gods + events + health + education + prestige + safety + freedom + traits;
    }
  };
  [[nodiscard]] HappinessParts happiness_parts(u32 citizen) const;
  [[nodiscard]] u8 food_kinds(const Household& h) const;
  [[nodiscard]] std::string citizen_name(u32 i) const;  // UI only (allocates)
  BuildingId best_service_for(data::NeedKind need, i32 from_tile, u16* cost = nullptr) const;
  i32 happy_sum_today = 0;
  u32 happy_count_today = 0;
  u8 avg_happiness = 128;

  // Governance (sim/governance.cpp)
  void governance_daily();
  void compute_ratings();
  u16 add_request(u8 kind, GoodId good, u16 amount, u16 days, i32 reward, i8 favor_win, i8 favor_lose);
  bool fulfill_request(u16 id);
  void dedicate(BuildingId id);  // a monument's completion effects
  [[nodiscard]] bool reservoir_full(BuildingId id) const {
    return id.index() < reservoir_full_.size() && reservoir_full_[id.index()] != 0;
  }
  [[nodiscard]] bool patrol_covers(const char* service, const Building& b) const;
  void building_hourly(BuildingId id);  // construction, production, hazards (public for tests)
  [[nodiscard]] bool site_ready(const Building& b) const;
  // Monuments rise in kMonumentStages stages; each stage needs its share of the materials.
  static constexpr u32 kMonumentStages = 4;
  [[nodiscard]] u32 construction_stage(const Building& b) const;
  [[nodiscard]] u32 materials_needed_now(const Building& b, u32 amount) const;
  // An open land (or sea) route still buys g this year.
  [[nodiscard]] bool open_route_buys(GoodId g, bool sea) const;
  // Unreserved amount of g in warehouses, granaries, stockpiles and trade posts (what a request can send).
  [[nodiscard]] u32 stored_amount(GoodId g) const;
  void take_loan(i64 amount);
  void change_favor(i32 delta);
  u16 post_message(u8 advisor, const std::string& title, const std::string& text,
                   const std::vector<std::string>& options = {}, const std::string& callback = "");
  void answer(u16 message_id, i32 option);
  [[nodiscard]] bool is_unlocked(BuildingDefId d) const { return d.v < unlocked.size() && unlocked[d.v]; }
  [[nodiscard]] bool is_unlocked_unit(UnitDefId u) const { return u.v < defs->units.size() && defs->units[u.v].unlock <= mission_number; }
  void unlock_up_to(u8 mission);
  [[nodiscard]] u32 count_active(BuildingDefId d) const;
  [[nodiscard]] bool goals_met() const;
  // Scripting (src/script): set by the scenario loader; called at event points only.
  struct ScriptHost {
    virtual ~ScriptHost() = default;
    virtual void on_day() = 0;
    virtual void on_choice(const std::string& callback, i32 option) = 0;
    virtual void on_request(u16 id, bool fulfilled) = 0;
    [[nodiscard]] virtual std::string export_state() const = 0;
    virtual void import_state(const std::string& s) = 0;
  };
  std::unique_ptr<ScriptHost> script;
  std::string script_source, script_name;  // kept to recreate the script on load
  std::string pending_script_state_;       // restored into the script after load

  // Military (sim/military.cpp)
  [[nodiscard]] u32 field_cost(i32 tile, u8 side) const;
  void compute_field(military::FlowField& f, i32 target, u8 side);
  i16 flow_field(i32 target, u8 side);
  [[nodiscard]] i32 city_target_tile() const;
  [[nodiscard]] i32 nearest_edge_tile(i32 from) const;
  u16 spawn_unit(UnitDefId def, u8 side, f32 x, f32 y, u16 count, BuildingId home);
  bool raise_unit(UnitDefId def, BuildingId barracks);
  void disband_unit(u16 u);
  void order_unit(u16 u, military::Order o, i32 tile);
  void spawn_raid(UnitDefId def, u16 units, i32 side);
  void military_tick();
  void military_daily();
  // Religion and threats (sim/religion.cpp, sim/threats.cpp)
  void religion_daily();
  void weather_daily();
  void hazards_hourly(BuildingId id);
  void ignite(BuildingId id);
  void ruin(BuildingId id);
  void disease_daily(BuildingId id);
  void revolt_check(bool forced);
  void trigger(TriggerId t, i32 x, i32 y, i32 value);
  void hold_festival(u8 god, i32 size);
  void god_blessing(u8 god);
  void god_wrath(u8 god);
  [[nodiscard]] bool effect(Effect e) const { return effect_until[static_cast<int>(e)] > now; }
  void set_effect(Effect e, Tick duration) { effect_until[static_cast<int>(e)] = now + duration; }
 private:
  void apply_commands();
  void process_events();
  void apply_path_results();
  void on_citizen_event(const Event& e);
  void on_building_event(const Event& e);
  void labor_allocate();
  void match_good(GoodId g);
  bool claim_task(u32 i);
  void haul_step(u32 i);
  void daily_update();
  void release_worker(u32 i);
  void house_daily(BuildingId id);
  void lifecycle_daily(u32 i);
  void kill_citizen(u32 i);
  void marry(u32 a, u32 b);
  void give_birth(u32 mother);
  void move_to_household(u32 i, HouseholdId hh);
  void assign_traits(u32 i, u32 parent_a, u32 parent_b);
  void assign_goal(u32 i);
  void free_slave(u32 i);
  void riot_check();
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
  std::vector<u8> labor_done_;  // per building index: no reachable candidate this run
  std::vector<i32> water_queue_;
  std::vector<u8> reservoir_full_;
  std::vector<i16> desir_acc_;
  data::HousingReq req_physician_, req_fountain_;
  path::IndexedHeap field_heap_;
  bool providers_dirty_ = true;
};

}  // namespace urbs::sim
