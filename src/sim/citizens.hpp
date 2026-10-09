#pragma once
#include <vector>

#include "core/pool.hpp"
#include "core/types.hpp"
#include "path/path_store.hpp"

namespace urbs::sim {

enum class Activity : u8 {
  None,
  Sleep,
  Home,      // at home, idle / eating / household chores
  Work,
  Shop,      // household shopping errand at a market
  Water,     // fetching water
  Visit,     // visiting a service building (temple, baths, tavern, theater, ...)
  School,
  Walk,      // walking to `dest`, then `next` begins
  Wander,    // homeless / looking for housing
  Build,     // builder at a construction site
  Haul,      // porter carrying goods
  Patrol,    // vigiles, engineers, guards
  Leave,     // emigrating: walking to the map edge
  Dead,
  Trade,     // caravan trader at a trade post
  Count
};

inline constexpr const char* activity_name(Activity a) {
  constexpr const char* n[] = {"none", "sleeping", "at home", "working", "shopping", "fetching water", "visiting",
                               "at school", "walking", "wandering", "building", "hauling", "patrolling", "leaving",
                               "dead", "trading"};
  return n[static_cast<int>(a)];
}

// Why a citizen is doing what it does (shown in the inspector).
enum class Reason : u8 {
  None, Night, Tired, Shift, Thirsty, Hungry, BuyFood, FetchWater, Worship, Bathe, Entertainment, Idle, Homeless,
  NoPath, Hauling, Building, Immigrating, Emigrating, ServiceFull, Midday, Unemployed, Child, NoFood, NoWater,
  Healing, School, Patrolling, Count
};
inline constexpr const char* reason_text(Reason r) {
  constexpr const char* t[] = {"",
                               "it is night: sleeping",
                               "exhausted: resting at home",
                               "on shift",
                               "thirsty: drinking at home",
                               "hungry: eating at home",
                               "the household is low on food: shopping at the market",
                               "the household has no water: fetching water",
                               "wants to worship",
                               "wants to bathe",
                               "wants entertainment",
                               "free time at home",
                               "homeless: looking for housing",
                               "cannot reach the destination",
                               "carrying goods",
                               "working on a construction site",
                               "arriving in the city",
                               "unhappy: leaving the city",
                               "the place was full: going home",
                               "midday meal at home",
                               "unemployed: free time at home",
                               "a child: playing at home",
                               "hungry, but the household has no food",
                               "thirsty, but there is no water source nearby",
                               "sick: seeing a physician",
                               "at school",
                               "patrolling the streets"};
  return t[static_cast<int>(r)];
}

// Lazily evaluated needs. 255 = fully satisfied, 0 = desperate.
enum class Need : u8 { Food, Water, Rest, Hygiene, Religion, Entertainment, Education, Count };
inline constexpr int kNeedCount = static_cast<int>(Need::Count);
inline constexpr const char* need_name(Need n) {
  constexpr const char* names[] = {"food", "water", "rest", "hygiene", "religion", "entertainment", "education"};
  return names[static_cast<int>(n)];
}

enum class CitizenClass : u8 { Patrician, Eques, Pleb, Libertus, Slave, Soldier, Count };
inline constexpr const char* class_name(CitizenClass c) {
  constexpr const char* n[] = {"patrician", "eques", "pleb", "libertus", "slave", "soldier"};
  return n[static_cast<int>(c)];
}

inline constexpr u8 kFlagVisitor = 1;  // non-resident (trader, envoy): excluded from population and labor
inline constexpr u8 kFlagSchooled = 2;  // attended school as a child (raises skill caps)
inline constexpr int kMaxTraits = 3;
inline constexpr int kSkillCount = 6;  // farming, crafting, building, trading, soldiering, scholarship
inline constexpr int kMemoryEntries = 8;

struct MemoryEntry {
  u32 day = 0;
  u8 kind = 0;
  u8 pad = 0;
  u16 arg = 0;
};

// Struct-of-arrays citizen pool. Hot fields first; cold fields below.
struct Citizens {
  core::SlotAlloc<CitizenId> ids;

  // --- hot ---
  std::vector<u16> token;
  std::vector<Activity> activity;
  std::vector<Activity> next;        // activity to start on arrival
  std::vector<BuildingId> loc;       // building the citizen is in (invalid while walking/outdoors)
  std::vector<BuildingId> dest;      // destination building of the current walk
  std::vector<i32> dest_tile;        // destination tile (for non-building destinations)
  std::vector<u32> path;             // path handle while walking
  std::vector<u8> path_rev;
  std::vector<Tick> walk_start;
  std::vector<Tick> activity_end;
  std::vector<BuildingId> home, work;
  std::vector<HouseholdId> household;
  std::vector<u8> need_base;   // [i * kNeedCount + n]
  std::vector<i8> need_rate;   // per game hour
  std::vector<Tick> need_t0;
  std::vector<u8> carrying;    // good id + 1 (0 = nothing), for rendering/hauling
  std::vector<u16> carry_amount;
  std::vector<u32> task;       // current haul task / patrol state (kNoIndex if none)
  std::vector<Reason> reason;  // why the current activity was chosen

  // --- cold ---
  std::vector<CitizenClass> cls;
  std::vector<u8> sex;  // 0 male, 1 female
  std::vector<i64> born;  // tick of birth; negative = before the scenario started
  std::vector<u16> name0, name1, name2;  // praenomen / nomen / cognomen indices
  std::vector<u8> traits;   // [i * kMaxTraits + k], 0xFF = none
  std::vector<u8> skills;   // [i * kSkillCount + k]
  std::vector<u8> happiness, health;
  std::vector<u8> goal;     // life goal id
  std::vector<u16> goal_progress;
  std::vector<CitizenId> spouse, mother, father;
  std::vector<MemoryEntry> memory;  // [i * kMemoryEntries + k], ring
  std::vector<u8> memory_head;
  std::vector<u8> flags;

  void init(u32 capacity);
  [[nodiscard]] u32 capacity() const { return ids.capacity(); }
  [[nodiscard]] bool alive(CitizenId id) const { return ids.alive(id); }

  [[nodiscard]] u8 need(u32 i, Need n, Tick now) const {
    const size_t k = static_cast<size_t>(i) * kNeedCount + static_cast<size_t>(n);
    i64 hours_x = static_cast<i64>(now - need_t0[i]);
    i64 v = static_cast<i64>(need_base[k]) + need_rate[k] * hours_x / static_cast<i64>(kTicksPerHour);
    return static_cast<u8>(v < 0 ? 0 : (v > 255 ? 255 : v));
  }
  // Re-bases all needs at `now` (call before changing rates).
  void rebase_needs(u32 i, Tick now) {
    for (int n = 0; n < kNeedCount; ++n) need_base[static_cast<size_t>(i) * kNeedCount + n] = need(i, static_cast<Need>(n), now);
    need_t0[i] = now;
  }
  void set_need(u32 i, Need n, u8 v) { need_base[static_cast<size_t>(i) * kNeedCount + static_cast<size_t>(n)] = v; }
  void add_need(u32 i, Need n, int dv, Tick now) {
    int v = need(i, n, now) + dv;
    rebase_needs(i, now);
    set_need(i, n, static_cast<u8>(v < 0 ? 0 : (v > 255 ? 255 : v)));
  }
  [[nodiscard]] u32 age_years(u32 i, Tick now) const;
  void remember(u32 i, u32 day, u8 kind, u16 arg);
};

struct Household {
  BuildingId home;
  u32 next_in_house = 0xFFFFFFFFu;  // intrusive list of households in the same building
  CitizenId head;
  u16 members = 0;
  CitizenClass cls = CitizenClass::Pleb;
  i32 savings = 0;
  u16 food = 0;           // abstract food units (scenario seed stock; eaten before slot food)
  u8 meals = 0;           // meals left from the last opened food unit
  u16 income_today = 0;   // wages earned since the last tax collection
  u16 fstock[8] = {};     // food stock per food slot (World::food_slots)
  u16 water = 0;          // water units at home
  u16 goods[8] = {};      // household goods stock per slot (World::hh_goods)
  CitizenId shopper;      // member currently on a shopping errand
  CitizenId fetcher;      // member currently fetching water
  u16 nomen = 0;          // family name index (shared by members)
  u8 unhappy_days = 0;
  u32 happy_sum = 0;  // accumulated by the staggered daily citizen pass
  u16 happy_n = 0;
  u8 leaving = 0;
  CitizenId owner;        // for slaves: owning household head (servi privati); invalid = public
  u8 flags = 0;
};

struct Households {
  core::SlotAlloc<HouseholdId> ids;
  std::vector<Household> h;
  void init(u32 capacity) {
    ids.init(capacity);
    h.assign(capacity, Household{});
  }
  Household& operator[](HouseholdId id) { return h[id.index()]; }
  const Household& operator[](HouseholdId id) const { return h[id.index()]; }
};

}  // namespace urbs::sim
