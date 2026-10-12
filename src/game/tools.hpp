#pragma once
#include <vector>

#include "render/renderer.hpp"
#include "sim/world.hpp"

namespace urbs::game {

class WorldView;

enum class ToolKind : u8 { Inspect, Build, Road, Demolish };

// Player construction tools: turns mouse input into sim Commands, draws placement ghosts.
class Tools {
 public:
  ToolKind kind = ToolKind::Inspect;
  BuildingDefId def;
  sim::Road road = sim::Road::Dirt;
  u8 god = 0;  // dedication for temples

  // Picking a house from the menu returns it to auto-facing (see rot()).
  void select_building(BuildingDefId d) {
    kind = ToolKind::Build;
    def = d;
    dragging_ = false;
    if (d.v < rot_.size()) rot_[d.v] = kAutoRot;
  }
  void select_road(sim::Road r) {
    kind = ToolKind::Road;
    road = r;
    dragging_ = false;
  }
  void select_demolish() {
    kind = ToolKind::Demolish;
    dragging_ = false;
  }
  void cancel() {
    kind = ToolKind::Inspect;
    dragging_ = false;
  }

  // Mouse in tile coordinates. Returns true if the event was consumed.
  bool mouse_down(Vec2i tile);
  void mouse_move(Vec2i tile) { hover_ = tile; }
  bool mouse_up(Vec2i tile, sim::World& w);
  void draw(const sim::World& w, const WorldView& view, render::Renderer& r) const;
  [[nodiscard]] bool dragging() const { return dragging_; }

  // Footprint the build tool would place given the current hover/drag.
  [[nodiscard]] Recti footprint(const sim::World& w) const;
  [[nodiscard]] i64 cost(const sim::World& w) const;

  // Rotation of the building in hand (Building::rot), remembered per building type for the session.
  // Houses face the street under the cursor by themselves (World::road_facing_rot) until the player
  // turns them.  rotate() turns it by +1 (clockwise) or -1; returns false when the tool holds nothing
  // rotatable.
  [[nodiscard]] bool can_rotate(const sim::World& w) const;
  bool rotate(const sim::World& w, int dir);
  [[nodiscard]] u8 rot(const sim::World& w) const;

 private:
  Vec2i hover_{0, 0}, start_{0, 0};
  bool dragging_ = false;
  static constexpr u8 kAutoRot = 0xFF;  // not turned by the player: houses auto-face, others rot 0
  std::vector<u8> rot_;                 // by building def
};

}  // namespace urbs::game
