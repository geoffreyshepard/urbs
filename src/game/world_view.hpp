#pragma once
#include <vector>

#include "render/renderer.hpp"
#include "sim/world.hpp"

namespace urbs::game {

// Turns sim state into sprites each frame (buildings, citizens). Read-only view of the world.
class WorldView {
 public:
  // region: a province art style; sprites named "<name>@<region>" replace the Italian ones where they exist.
  void init(const render::Atlas& atlas, const data::Defs& defs, const std::string& region = {});
  void draw(const sim::World& w, render::Renderer& r, f32 alpha);

  [[nodiscard]] const render::Sprite& building_sprite(BuildingDefId def, i32 w, i32 h, u8 tier = 0) const;
  [[nodiscard]] const render::Sprite& good_icon(GoodId g, bool big) const;
  render::Sprite dot, white;

 private:
  struct DefSprites {
    render::Sprite base;
    render::Sprite tier[5];
    render::Sprite base_lit, tier_lit[5];  // night window lights (bld/<name>_lit)
    render::Sprite tile[4];
    render::Sprite hut;
    render::Sprite line_c, line[4];  // n e s w
    bool painted = false, line_type = false;
  };
  void draw_building(const sim::World& w, render::Renderer& r, const sim::Building& b, f32 map_h) const;

  std::vector<DefSprites> defs_;
  std::vector<render::Sprite> goods_, goods16_;
  render::Sprite placeholder_[4][4];
  render::Sprite people_[static_cast<int>(sim::CitizenClass::Count)][4][4];
  render::Sprite child_[4][4];
  render::Sprite scaffold_[4], foundation_;
  render::Sprite ship_[4];
  render::Sprite fire_[4], smoke_[4], rubble_[4], plague_;
  struct UnitSprites {
    render::Sprite walk[4][4];
    render::Sprite atk[4][2];
  };
  std::vector<UnitSprites> units_;
  void draw_army(const sim::World& w, render::Renderer& r, f32 alpha) const;
};

}  // namespace urbs::game
