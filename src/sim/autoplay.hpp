#pragma once
#include <map>
#include <string>
#include <utility>
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
  // Actions since the last call (for traces).
  std::vector<std::string> take_log() {
    for (const auto& [why, n] : idle_hours_) log_.push_back("[idle " + std::to_string(n) + "h: " + why + "]");
    idle_hours_.clear();
    return std::exchange(log_, {});
  }
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
  bool repair_chain(World& w, GoodId g, i32 cx, i32 cy, int depth);
  // Make sure g is produced or imported; true if a building was placed for it.
  bool supply(World& w, GoodId g, u32 amount, i32 x, i32 y);
  int supplying_ = 0;
  std::vector<u16> pending_;  // building defs ordered this hour
  [[nodiscard]] u32 producers_of(World& w, GoodId g) const;
  bool plumb(World& w);
  std::map<u32, Tick> pipe_tried_;
  Tick res_cooldown_ = 0;
  u32 noted_ = 0;           // actions taken (counts note() calls)
  void act(World& w);       // one decision; at most one action
  BuildingDefId first_unlocked(World& w, std::initializer_list<const char*> ids) const;
  u32 count(World& w, const char* id) const;
  void do_trade_and_requests(World& w);

  i32 ox_ = 0, oy_ = 0;  // grid origin
  std::vector<Block> blocks_;
  std::string last_;
  std::vector<std::string> log_;
  const char* idle_ = "";
  std::map<std::string, u32> idle_hours_;
  void note(const std::string& s) {
    last_ = s;
    ++noted_;
    if (log_.size() < 64) log_.push_back(s);
  }
  Tick last_block_ = 0;
  Tick last_enemy_ = 0;  // last hour raiders were on the map
  Tick water_cooldown_ = 0;
  Tick fire_cooldown_ = 0;
  Tick close_cooldown_ = 0;
  GoodId request_good_{};  // a good Rome asked for that we should be producing
  std::map<u32, Tick> blocker_cooldown_;
  std::map<u32, Tick> stuck_since_;  // construction site (raw id) -> first hour seen with no builders  // requirement key -> tick when it may be chased again
};

}  // namespace urbs::sim
