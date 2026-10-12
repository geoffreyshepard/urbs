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

  // w, h: the footprint on the map (already rotated); rot: Building::rot (docs/13-isometric.md).
  [[nodiscard]] const render::Sprite& building_sprite(BuildingDefId def, i32 w, i32 h, u8 tier = 0, u8 rot = 0) const;
  [[nodiscard]] const render::Sprite& good_icon(GoodId g, bool big) const;
  render::Sprite dot, white;

 private:
  // Footprint sprites are indexed by rotation (bld/<name>, bld/<name>_r1 .. _r3).  Resolved once at
  // init: a missing rotated sprite falls back to the unrotated one for square footprints and is left
  // invalid (-> size-matched placeholder) for the others.
  struct DefSprites {
    render::Sprite base[4];
    render::Sprite tier[4][5];
    render::Sprite base_lit[4], tier_lit[4][5];  // night window lights (bld/<name>[_r<k>]_lit)
    render::Sprite tile[4];
    render::Sprite hut;
    render::Sprite line_c, line[4];  // n e s w
    bool painted = false, line_type = false;
  };
  void draw_building(const sim::World& w, render::Renderer& r, const sim::Building& b, f32 map_h) const;

  std::vector<DefSprites> defs_;
  std::vector<render::Sprite> goods_, goods16_;
  render::Sprite placeholder_[4][4];
  // Citizens: clothing variants per class (people/<class>[_v<k>]_<dir>_<frame>, tools/artgen3d/
  // people.py), picked per citizen by a hash of its index.  [class][variant][dir][frame].
  static constexpr int kPeopleVariants = 10;
  render::Sprite people_[static_cast<int>(sim::CitizenClass::Count)][kPeopleVariants][4][4];
  u8 people_variants_[static_cast<int>(sim::CitizenClass::Count)] = {};
  render::Sprite child_[kPeopleVariants][4][4];
  u8 child_variants_ = 1;
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
