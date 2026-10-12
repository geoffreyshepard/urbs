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

  // w, h: the footprint on the map (already rotated); rot: Building::rot (docs/13-isometric.md);
  // variant: a tier look (bld/<name>_t<k>_v<n>), wrapped to the variants that exist (0 = the plain one,
  // as the build menu shows it).
  [[nodiscard]] const render::Sprite& building_sprite(BuildingDefId def, i32 w, i32 h, u8 tier = 0, u8 rot = 0,
                                                      u8 variant = 0) const;
  // The variant a building at (x, y) shows: a hash of its position, render-only (not in the sim or saves).
  [[nodiscard]] static u8 variant_at(i32 x, i32 y, u8 count);
  static constexpr int kVariants = 4;
  [[nodiscard]] const render::Sprite& good_icon(GoodId g, bool big) const;
  render::Sprite dot, white;

 private:
  // Footprint sprites are indexed by rotation (bld/<name>, bld/<name>_r1 .. _r3).  Resolved once at
  // init: a missing rotated sprite falls back to the unrotated one for square footprints and is left
  // invalid (-> size-matched placeholder) for the others.
  // Tier sprites have up to kVariants looks (bld/<name>_t<k>_v<n>[_r<k>]); a variant missing in some
  // rotation (or in the province's style) falls back to variant 0 there.
  struct DefSprites {
    render::Sprite base[4];
    render::Sprite tier[kVariants][4][5];                   // [variant][rot][tier]
    render::Sprite base_lit[4], tier_lit[kVariants][4][5];  // night window lights (bld/<name>[_r<k>]_lit)
    u8 variants[5] = {1, 1, 1, 1, 1};                       // looks per tier (variants 0 .. n-1 have unrotated art)
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
