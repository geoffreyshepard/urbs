#pragma once
#include "core/types.hpp"

namespace urbs::render {

// A sprite in the atlas.  u/v/w/h and ax/ay are atlas texels.  The atlas is a stack of pages
// (texture-array layers) of Atlas::height() rows each, and v counts rows through the whole stack:
// page = v / height(), row in page = v % height().  CPU code can index Atlas::pixels() with
// (v + y) * width() + u + x directly; GPU instances pass v through unchanged (the shaders split it).
struct Sprite {
  u16 u = 0, v = 0, w = 0, h = 0;
  i16 ax = 0, ay = 0;     // anchor inside the sprite (texels)
  f32 iso_per_texel = 1;  // iso px per atlas texel (1 / the sprite's packing scale)
  bool valid = false;

  [[nodiscard]] f32 iso_w() const { return static_cast<f32>(w) * iso_per_texel; }
  [[nodiscard]] f32 iso_h() const { return static_cast<f32>(h) * iso_per_texel; }
  [[nodiscard]] f32 iso_ax() const { return static_cast<f32>(ax) * iso_per_texel; }
  [[nodiscard]] f32 iso_ay() const { return static_cast<f32>(ay) * iso_per_texel; }
};

}  // namespace urbs::render
