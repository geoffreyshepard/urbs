#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

namespace urbs::data {

enum class GoodCategory : u8 { RawFood, Food, Material, Metal, Crafted, Luxury, Special };

struct GoodDef {
  std::string id, name, icon;
  GoodCategory category = GoodCategory::Material;
  bool food = false;      // counts toward calories / food variety
  u16 base_price = 5;     // denarii per unit
  u8 nutrition = 0;       // food units provided per item
  u8 unlock = 1;
};

enum class BuildingKind : u8 {
  House,
  Farm,
  Workplace,  // generic producer (workshop, mine, ...)
  Storage,    // warehouse / granary / stockpile
  Market,
  Service,    // visit-based service (temple, baths, tavern, ...)
  Patrol,     // patrol-based service (vigiles, engineers, guard)
  Water,      // well, fountain, reservoir
  Decor,      // garden, statue, plaza
  Civic,      // forum, palace, basilica, curia
  Military,   // barracks, fort, walls...
  Monument,
  Infrastructure,  // aqueduct, bridge, sewer
  TradePost,
  Count
};

enum class ClassMask : u8 {
  None = 0,
  Patrician = 1,
  Eques = 2,
  Pleb = 4,
  Libertus = 8,
  Slave = 16,
  Soldier = 32,
  Free = 1 | 2 | 4 | 8,
  Workers = 4 | 8 | 16,
  All = 63
};
inline constexpr u8 class_bit(int c) { return static_cast<u8>(1u << c); }

struct GoodAmount {
  GoodId good;
  u16 amount = 0;
};

struct RecipeDef {
  std::string id;
  std::vector<GoodAmount> inputs, outputs;
  u16 work = 100;  // worker-ticks per batch
};

// Citizen needs a service can satisfy (matches sim::Need for the first six).
enum class NeedKind : i8 { None = -1, Food = 0, Water, Rest, Hygiene, Religion, Entertainment, Health, Education };

struct ServiceDef {
  std::string id, name;
  bool patrol = false;
  u16 visit_ticks = 50;  // time a citizen spends inside
  u16 range = 60;        // max walking time (ticks) for a house to count as covered
  NeedKind need = NeedKind::None;
  u8 quality = 100;      // how well a visit satisfies the need (0..255)
};

struct HousingReq {
  enum Kind : u8 { Service, FoodKinds, Food, Goods, Desirability, Sewer, PipedWater, Patrol } kind = Service;
  std::vector<ServiceId> services;  // Service: any of; Patrol: one
  std::vector<GoodId> goods;        // Goods: all of; Food: one
  i32 value = 0;                    // FoodKinds / Desirability
};

struct HousingTierDef {
  std::string id, name;
  u8 line = 0;  // 0 plebeian, 1 patrician
  u8 tier = 0;
  u16 capacity = 5;
  std::vector<HousingReq> reqs;
};

struct TradePartnerDef {
  std::string id, name;
  bool sea = false;
  i32 open_cost = 0;
  u16 interval_days = 5;
  std::vector<GoodAmount> buys, sells;  // amount = yearly cap
};

struct TraitDef {
  std::string id, name, desc;
  f32 religion_rate = 1, rest_rate = 1, entertainment_rate = 1, hygiene_rate = 1, education_rate = 1;
  f32 work = 1, skill_rate = 1, goal_rate = 1, spend = 1, health = 1, lifespan = 1, crime = 1, fertility = 1;
  i8 happiness = 0;
};

struct GodDef {
  std::string id, name, domain, blessing, wrath, blessing_text, wrath_text;
  u8 festival_day = 0;  // day of year of the god's feast
};

struct UnitDef {
  std::string id, name, sprite;
  bool enemy = false;
  u16 hp = 100, attack = 10, defense = 5;
  f32 speed = 0.09f;  // tiles per tick
  f32 range = 1.2f;   // tiles; > 2 means ranged
  u16 size = 32;      // soldiers per unit
  u16 upkeep = 3;     // denarii per soldier per day
  std::vector<GoodAmount> gear;  // per soldier when raised
  u8 morale = 60;
  f32 charge = 1.0f, siege = 0.5f;
  u8 unlock = 1;
  [[nodiscard]] bool ranged() const { return range > 2.0f; }
};

struct NameLists {
  std::vector<std::string> praenomina, nomina, cognomina, slave;
};

struct BuildingDef {
  std::string id, name, category, sprite;
  BuildingKind kind = BuildingKind::Workplace;
  u8 w = 1, h = 1;
  bool painted = false;  // player paints an area; capacity scales with tiles
  u8 min_w = 1, min_h = 1, max_w = 16, max_h = 16;
  i32 cost_denarii = 0;
  std::vector<GoodAmount> cost_goods;
  u16 build_work = 50;  // builder-ticks
  u16 workers = 0;      // job slots (per tile for painted buildings: workers_per_10_tiles / 10)
  u8 worker_classes = static_cast<u8>(ClassMask::Workers);
  RecipeId recipe;
  ServiceId service;
  u16 service_capacity = 0;  // simultaneous visitors
  u16 housing = 0;           // residents at base tier
  u8 house_line = 0;         // 0 plebeian, 1 patrician
  u16 storage = 0;           // goods units
  std::vector<GoodId> accepts;  // storage/market: goods accepted (empty = all)
  i8 desirability = 0;
  u8 desirability_range = 0;
  u8 fire_risk = 0, collapse_risk = 0;
  u8 unlock = 1;          // campaign mission that unlocks it
  u8 resource = 0;        // sim::Resource required under the footprint (0 = none)
  bool needs_water_adjacent = false;
  bool needs_road = true;
  u8 god = 0xFF;          // temples: god index
  u16 water_radius = 0;   // wells/fountains
  u8 patrol_range = 0;
  u8 layer = 0;           // 0 building, 1 pipe (underground), 2 sewer (underground), 3 bridge (road over water)
  bool passable = false;  // citizens can walk through (aqueduct arches)
  bool needs_pipe = false;  // only works with piped water (fountain, thermae)
};

struct Defs {
  std::vector<GoodDef> goods;
  std::vector<RecipeDef> recipes;
  std::vector<ServiceDef> services;
  std::vector<BuildingDef> buildings;
  std::vector<HousingTierDef> housing;  // ordered by line, tier
  std::vector<TradePartnerDef> partners;
  std::vector<TraitDef> traits;
  std::vector<GodDef> gods;
  std::vector<UnitDef> units;
  [[nodiscard]] UnitDefId unit(const std::string& id) const {
    for (size_t k = 0; k < units.size(); ++k)
      if (units[k].id == id) return UnitDefId{static_cast<u16>(k)};
    return UnitDefId{};
  }
  [[nodiscard]] u8 god(const std::string& id) const {
    for (size_t k = 0; k < gods.size(); ++k)
      if (gods[k].id == id) return static_cast<u8>(k);
    return 0xFF;
  }
  NameLists names;
  [[nodiscard]] const HousingTierDef* tier(u8 line, u8 t) const {
    for (const auto& h : housing)
      if (h.line == line && h.tier == t) return &h;
    return nullptr;
  }
  [[nodiscard]] u8 max_tier(u8 line) const {
    u8 m = 0;
    for (const auto& h : housing)
      if (h.line == line && h.tier > m) m = h.tier;
    return m;
  }

  std::unordered_map<std::string, u16> good_ids, recipe_ids, service_ids, building_ids;

  [[nodiscard]] GoodId good(const std::string& id) const;
  [[nodiscard]] RecipeId recipe(const std::string& id) const;
  [[nodiscard]] ServiceId service(const std::string& id) const;
  [[nodiscard]] BuildingDefId building(const std::string& id) const;
  [[nodiscard]] const BuildingDef& b(BuildingDefId id) const { return buildings[id.v]; }
  [[nodiscard]] const GoodDef& g(GoodId id) const { return goods[id.v]; }
};

// Loads <root>/defs/*.json, then each mod dir's defs/*.json, merging by id.
// Throws std::runtime_error with file and reason on validation failure (load time only).
Defs load_defs(const std::string& data_root, const std::vector<std::string>& mod_roots = {});

}  // namespace urbs::data
