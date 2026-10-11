#pragma once
// Isometric projection shared by the renderer, the terrain layer and the game's world view.
// See docs/13-isometric.md.
//
// The simulation keeps its top-down grid: world px = tile * sim::kTilePx (16), x east, y south.
// Everything drawn on screen lives in "iso px": a 2:1 dimetric projection in which one tile is a
// 128 x 64 diamond (kTileW x kTileH) and each terrain height level lifts the ground by kLevelPx.
// Grid north (-y) points to the screen's upper right, east (+x) to the lower right.
#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "sim/map.hpp"

namespace urbs::render::iso {

inline constexpr f32 kTileW = 128.0f;
inline constexpr f32 kTileH = 64.0f;
inline constexpr f32 kLevelPx = 16.0f;  // screen lift per terrain height level (iso px)

struct Rectf {
  f32 x = 0, y = 0, w = 0, h = 0;
};

// Tile coordinates (fractional, top-down grid) + height level -> iso px.
inline Vec2 from_tile(f32 tx, f32 ty, f32 level = 0.0f) {
  return {(tx - ty) * (kTileW * 0.5f), (tx + ty) * (kTileH * 0.5f) - level * kLevelPx};
}
// Sim world px -> iso px.
inline Vec2 project(f32 wx, f32 wy, f32 level = 0.0f) {
  constexpr f32 k = 1.0f / static_cast<f32>(sim::kTilePx);
  return from_tile(wx * k, wy * k, level);
}
inline Vec2 project(Vec2 w, f32 level = 0.0f) { return project(w.x, w.y, level); }

// Iso px on the plane of the given height level -> fractional tile coordinates.
inline Vec2 to_tile(Vec2 iso, f32 level = 0.0f) {
  const f32 a = iso.x / (kTileW * 0.5f);
  const f32 b = (iso.y + level * kLevelPx) / (kTileH * 0.5f);
  return {(a + b) * 0.5f, (b - a) * 0.5f};
}
// Iso px -> sim world px on the given level.
inline Vec2 unproject(Vec2 iso, f32 level = 0.0f) {
  const Vec2 t = to_tile(iso, level);
  return {t.x * static_cast<f32>(sim::kTilePx), t.y * static_cast<f32>(sim::kTilePx)};
}

// Anchors (see the sprite conventions in docs/13-isometric.md).
// Ground-attached sprites (buildings, terrain, roads, farm tiles, line pieces) are anchored at the
// footprint's bottom (south-east) corner: grid corner (x0 + fw, y0 + fh).
inline Vec2 footprint_anchor(i32 x0, i32 y0, i32 fw, i32 fh, f32 level = 0.0f) {
  return from_tile(static_cast<f32>(x0 + fw), static_cast<f32>(y0 + fh), level);
}
inline Vec2 tile_anchor(i32 x, i32 y, f32 level = 0.0f) { return footprint_anchor(x, y, 1, 1, level); }

// Height level of a tile (0 outside the map).
inline f32 level_at(const sim::TileMap& m, i32 x, i32 y) {
  if (x < 0 || y < 0 || x >= m.w || y >= m.h) return 0.0f;
  return static_cast<f32>(m.height[static_cast<size_t>(y) * static_cast<size_t>(m.w) + static_cast<size_t>(x)]);
}
// Height level under a sim world px position.
inline f32 level_at_world(const sim::TileMap& m, f32 wx, f32 wy) {
  return level_at(m, static_cast<i32>(std::floor(wx / static_cast<f32>(sim::kTilePx))),
                  static_cast<i32>(std::floor(wy / static_cast<f32>(sim::kTilePx))));
}

// Painter's order: larger iso y (nearer the viewer) draws in front.  Depth is 0 (near) .. 1 (far)
// for y-sorted sprites, in the same band the top-down renderer used (0.1 .. 0.9).
inline f32 map_iso_h(const sim::TileMap& m) { return (static_cast<f32>(m.w + m.h)) * (kTileH * 0.5f); }
inline f32 depth_for_iso(f32 foot_iso_y, f32 map_h) {
  return std::clamp(0.9f - 0.8f * ((foot_iso_y + 2.0f * kTileH) / (map_h + 4.0f * kTileH)), 0.1f, 0.9f);
}

// Bounding box of the whole map in iso px (level 0 ground; tall sprites and levels extend above).
inline Rectf map_bounds(const sim::TileMap& m) {
  const f32 left = -static_cast<f32>(m.h) * kTileW * 0.5f;
  const f32 right = static_cast<f32>(m.w) * kTileW * 0.5f;
  return {left, 0.0f, right - left, map_iso_h(m)};
}

// Inclusive tile range whose diamonds can touch an iso rect, padded by `pad` tiles on every side
// (pad covers tall sprites and raised levels).  Clamped to the map.
struct TileRange {
  i32 x0, y0, x1, y1;
};
inline TileRange tiles_in_rect(const sim::TileMap& m, Rectf r, i32 pad) {
  const Vec2 c[4] = {to_tile({r.x, r.y}), to_tile({r.x + r.w, r.y}), to_tile({r.x, r.y + r.h}),
                     to_tile({r.x + r.w, r.y + r.h})};
  f32 x0 = c[0].x, x1 = c[0].x, y0 = c[0].y, y1 = c[0].y;
  for (const Vec2& p : c) {
    x0 = std::min(x0, p.x), x1 = std::max(x1, p.x);
    y0 = std::min(y0, p.y), y1 = std::max(y1, p.y);
  }
  return {std::clamp(static_cast<i32>(std::floor(x0)) - pad, 0, m.w - 1),
          std::clamp(static_cast<i32>(std::floor(y0)) - pad, 0, m.h - 1),
          std::clamp(static_cast<i32>(std::floor(x1)) + pad, 0, m.w - 1),
          std::clamp(static_cast<i32>(std::floor(y1)) + pad, 0, m.h - 1)};
}

// Tile under an iso point, honouring terrain height: tries the highest level first, so a raised
// tile in front hides the ground behind it.  Returns false outside the map.
inline bool pick_tile(const sim::TileMap& m, Vec2 iso_pt, i32& out_x, i32& out_y) {
  for (int lv = sim::kMaxHeight; lv >= 0; --lv) {
    const Vec2 t = to_tile(iso_pt, static_cast<f32>(lv));
    const i32 x = static_cast<i32>(std::floor(t.x)), y = static_cast<i32>(std::floor(t.y));
    if (x < 0 || y < 0 || x >= m.w || y >= m.h) continue;
    if (static_cast<int>(level_at(m, x, y)) == lv) {
      out_x = x, out_y = y;
      return true;
    }
  }
  const Vec2 t = to_tile(iso_pt, 0.0f);
  const i32 x = static_cast<i32>(std::floor(t.x)), y = static_cast<i32>(std::floor(t.y));
  if (x < 0 || y < 0 || x >= m.w || y >= m.h) return false;
  out_x = x, out_y = y;
  return true;
}

}  // namespace urbs::render::iso
