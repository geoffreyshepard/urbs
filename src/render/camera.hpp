#pragma once
#include <cmath>

#include "core/math.hpp"

namespace urbs::render {

// Screen points per iso px (a tile is 128 iso px wide): index 3 shows a tile 64 points wide.
inline constexpr f32 kZoomLevels[] = {0.0625f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f};
inline constexpr int kZoomCount = 6;
inline constexpr int kFarZoomMax = 1;  // zoom indices <= this use the simplified far representation

// All positions are iso px (see render/iso.hpp); "world" in the names below means iso px.
struct Camera {
  Vec2 center{0, 0};     // iso px
  int zoom = 3;          // index into kZoomLevels
  f32 density = 1.0f;    // physical pixels per window point
  Vec2 screen{1, 1};     // target size in physical px

  [[nodiscard]] f32 scale() const { return kZoomLevels[zoom] * density; }
  [[nodiscard]] bool far() const { return zoom <= kFarZoomMax; }
  [[nodiscard]] Vec2 origin() const {
    f32 s = scale();
    Vec2 o{center.x - screen.x / (2 * s), center.y - screen.y / (2 * s)};
    // Snap to whole screen pixels so sprites never shimmer while scrolling.
    o.x = std::floor(o.x * s) / s;
    o.y = std::floor(o.y * s) / s;
    return o;
  }
  // Window-point coordinates -> iso px.
  [[nodiscard]] Vec2 screen_to_world(Vec2 pt) const {
    Vec2 o = origin();
    f32 s = scale();
    return {o.x + pt.x * density / s, o.y + pt.y * density / s};
  }
  [[nodiscard]] Vec2 world_to_screen(Vec2 w) const {
    Vec2 o = origin();
    f32 s = scale();
    return {(w.x - o.x) * s / density, (w.y - o.y) * s / density};
  }
  // Visible iso rect.
  [[nodiscard]] Recti visible_world(i32 margin_px = 0) const {
    Vec2 o = origin();
    f32 s = scale();
    return {static_cast<i32>(o.x) - margin_px, static_cast<i32>(o.y) - margin_px,
            static_cast<i32>(screen.x / s) + 2 * margin_px + 1, static_cast<i32>(screen.y / s) + 2 * margin_px + 1};
  }
};

}  // namespace urbs::render
