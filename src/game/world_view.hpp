#pragma once
#include <vector>

#include "render/renderer.hpp"
#include "sim/world.hpp"

namespace urbs::game {

// Turns sim state into sprites each frame (buildings, citizens). Read-only view of the world.
class WorldView {
 public:
  void init(const render::Atlas& atlas, const data::Defs& defs);
  void draw(const sim::World& w, render::Renderer& r, f32 alpha);

  [[nodiscard]] const render::Sprite& building_sprite(BuildingDefId def, i32 w, i32 h) const;
  render::Sprite dot, white;

 private:
  std::vector<render::Sprite> bld_;            // per def (invalid -> placeholder)
  render::Sprite placeholder_[4][4];
  render::Sprite people_[static_cast<int>(sim::CitizenClass::Count)][4][4];
  render::Sprite idle_[static_cast<int>(sim::CitizenClass::Count)][4];
  render::Sprite child_[4][4];
  render::Sprite construction_;
};

}  // namespace urbs::game
