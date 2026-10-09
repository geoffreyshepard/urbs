#pragma once
#include <map>
#include <string>
#include <vector>

#include "sim/world.hpp"

namespace urbs::sim {

// A simple city-building bot that plays through Commands only (like a player). Used to check that
// missions can be won and to soak-test the simulation. Not part of the game itself.
class Autoplay {
 public:
  void init(World& w);
  // Call once per tick before World::tick(); acts every game hour.
  void step(World& w);
  [[nodiscard]] const std::string& last_action() const { return last_; }
  u32 actions = 0;

 private:
  struct Block {
    i32 x, y;  // top-left interior tile (blocks are 8x8 inside a 9-tile road grid)
    u8 active = 0, bad = 0;
  };
  // max_dist > 0: only lots within that many tiles (Manhattan) of (near_x, near_y).
  bool place(World& w, BuildingDefId def, i32 near_x, i32 near_y, bool painted_block = false, i32 max_dist = 0);
  bool activate_block(World& w, i32 near_x, i32 near_y);
  bool build_for_requirement(World& w, const data::HousingReq& r, i32 cx, i32 cy);
  bool build_producer_of(World& w, GoodId g, i32 cx, i32 cy, int depth);
  BuildingDefId first_unlocked(World& w, std::initializer_list<const char*> ids) const;
  u32 count(World& w, const char* id) const;
  void do_trade_and_requests(World& w);

  i32 ox_ = 0, oy_ = 0;  // grid origin
  std::vector<Block> blocks_;
  std::string last_;
  Tick last_block_ = 0;
  Tick last_enemy_ = 0;  // last hour raiders were on the map
  Tick water_cooldown_ = 0;
  GoodId request_good_{};  // a good Rome asked for that we should be producing
  std::map<u32, Tick> blocker_cooldown_;
  std::map<u32, Tick> stuck_since_;  // construction site (raw id) -> first hour seen with no builders  // requirement key -> tick when it may be chased again
};

}  // namespace urbs::sim
