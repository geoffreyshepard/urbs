#pragma once
#include <vector>

#include "core/pool.hpp"
#include "core/types.hpp"

namespace urbs::military {

enum class Formation : u8 { Line, Wedge, Testudo, Loose, Count };
enum class Stance : u8 { Aggressive, Defensive };
enum class Order : u8 { Hold, Move, AttackCity, Return, Campaign, Away, Rout };
inline constexpr const char* formation_name(Formation f) {
  constexpr const char* n[] = {"line", "wedge", "testudo", "loose"};
  return n[static_cast<int>(f)];
}
inline constexpr const char* order_name(Order o) {
  constexpr const char* n[] = {"holding", "marching", "attacking", "returning", "leaving on campaign", "on campaign",
                               "routing"};
  return n[static_cast<int>(o)];
}

inline constexpr u16 kNoUnit = 0xFFFF;
inline constexpr u32 kNoSoldier = 0xFFFFFFFFu;
inline constexpr int kMaxUnits = 96;
inline constexpr int kMaxFields = 16;

struct Unit {
  u8 alive = 0;
  u8 side = 0;  // 0 city, 1 enemy
  UnitDefId def;
  Formation formation = Formation::Line;
  Stance stance = Stance::Aggressive;
  Order order = Order::Hold;
  i32 target_tile = -1;
  f32 ax = 0, ay = 0;  // formation anchor (tile units)
  f32 fx = 0, fy = 1;  // facing (unit vector)
  u16 count = 0, initial = 0;
  u8 morale = 60;
  u8 exp = 0;
  BuildingId home;      // barracks / fort
  i16 field = -1;       // flow field index (march target)
  Tick away_until = 0;  // campaign return tick
  u8 campaign_level = 0;
  u16 losses_window = 0, kills_window = 0;
  u16 number = 0;  // display number (Cohors I, II, ...)
};

struct FlowField {
  i32 target = -1;
  u8 side = 0;
  u8 valid = 0;
  Tick computed = 0;
  Tick last_used = 0;
  std::vector<u32> dist;
};

struct Projectile {
  f32 x = 0, y = 0, tx = 0, ty = 0;
  u32 target = kNoSoldier;
  BuildingId target_building;
  u16 damage = 0;
  u8 side = 0;
  u8 alive = 0;
};

// Struct-of-arrays soldiers. Unlike citizens, soldiers in the field tick every tick (battles are
// the one place where many entities do), so their data is kept compact.
struct Army {
  core::SlotAlloc<SoldierId> ids;
  std::vector<f32> x, y, px, py;  // tile units; previous tick for interpolation
  std::vector<i16> hp;
  std::vector<u16> unit, slot;
  std::vector<u16> cooldown;
  std::vector<u32> target;
  std::vector<u8> facing;  // 0 n, 1 e, 2 s, 3 w
  std::vector<f32> walked;  // animation distance
  std::vector<u8> attacking;  // ticks left showing attack frames

  Unit units[kMaxUnits];
  FlowField fields[kMaxFields];
  std::vector<Projectile> projectiles;

  // Spatial hash (cells of kCell tiles), rebuilt each tick without allocation.
  static constexpr i32 kCell = 4;
  i32 cells_x = 0, cells_y = 0;
  std::vector<u32> cell_head, next_in_cell;

  u32 deployed = 0;  // soldiers alive
  u16 next_number = 1;

  void init(u32 max_soldiers, i32 map_w, i32 map_h);
  [[nodiscard]] i32 cell_of(f32 tx, f32 ty) const {
    i32 cx = static_cast<i32>(tx) / kCell, cy = static_cast<i32>(ty) / kCell;
    cx = cx < 0 ? 0 : (cx >= cells_x ? cells_x - 1 : cx);
    cy = cy < 0 ? 0 : (cy >= cells_y ? cells_y - 1 : cy);
    return cy * cells_x + cx;
  }
  void rebuild_hash();
};

}  // namespace urbs::military
