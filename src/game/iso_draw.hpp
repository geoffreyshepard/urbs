#pragma once
// Game-side helpers for drawing in the isometric view (docs/13-isometric.md): anchored sprite
// instances, footprint diamonds and outlines, and painter's-order depths.
#include <algorithm>
#include <cmath>

#include "render/iso.hpp"
#include "render/renderer.hpp"
#include "sim/map.hpp"

namespace urbs::game {

// Old top-down screen sizes map to iso px at about this factor (zoom levels shrank by 4).
inline constexpr f32 kIsoK = 4.0f;

// Depth of something whose footprint centre is the tile point (tx, ty): y-sorting on the centre
// keeps citizens on the roads around a big building in front of its near walls.
// map_h = render::iso::map_iso_h(map).
inline f32 iso_depth(f32 map_h, f32 tx, f32 ty) {
  return render::iso::depth_for_iso(render::iso::from_tile(tx, ty).y, map_h);
}

// Iso px of a tile's centre, on its own height level.
inline Vec2 tile_center_iso(const sim::TileMap& m, i32 tx, i32 ty) {
  return render::iso::from_tile(static_cast<f32>(tx) + 0.5f, static_cast<f32>(ty) + 0.5f,
                                render::iso::level_at(m, tx, ty));
}

// Keeps an iso camera centre over the map's diamond.
inline Vec2 clamp_to_map(const sim::TileMap& m, Vec2 c) {
  const Vec2 t = render::iso::to_tile(c);
  return render::iso::from_tile(std::clamp(t.x, 0.0f, static_cast<f32>(m.w)),
                                std::clamp(t.y, 0.0f, static_cast<f32>(m.h)));
}

// Sprite instance with the sprite's anchor at iso point a (sizes in iso px).
inline render::SpriteInstance iso_instance(const render::Sprite& s, Vec2 a, f32 z, u32 color = render::kWhite) {
  return render::Renderer::instance(s, a.x, a.y, z, color);
}

// Sprite instance of fixed iso size (w x h) centred horizontally on a, bottom at a.y.  For UI-ish
// world marks (goods carried, status icons) whose art keeps its own pixel scale.
inline render::SpriteInstance iso_icon(const render::Sprite& s, Vec2 a, f32 w, f32 h, f32 z,
                                       u32 color = render::kWhite) {
  return {a.x - w * 0.5f, a.y - h, z, w, h, s.u, s.v, s.w, s.h, color};
}

// Outline of a footprint parallelogram, `t` iso px thick.
inline void push_diamond_outline(render::Renderer& r, i32 x0, i32 y0, i32 fw, i32 fh, f32 level, f32 z, u32 color,
                                 f32 t = 3.0f) {
  using namespace render::iso;
  const Vec2 p[4] = {from_tile(static_cast<f32>(x0), static_cast<f32>(y0), level),
                     from_tile(static_cast<f32>(x0 + fw), static_cast<f32>(y0), level),
                     from_tile(static_cast<f32>(x0 + fw), static_cast<f32>(y0 + fh), level),
                     from_tile(static_cast<f32>(x0), static_cast<f32>(y0 + fh), level)};
  for (int e = 0; e < 4; ++e) {
    const Vec2 a = p[e], b = p[(e + 1) & 3];
    const f32 len = std::fabs(b.x - a.x);
    const int n = std::max(1, static_cast<int>(len / t));
    for (int k = 0; k < n; ++k) {
      const f32 f = (static_cast<f32>(k) + 0.5f) / static_cast<f32>(n);
      r.push_rect(a.x + (b.x - a.x) * f - t * 0.5f, a.y + (b.y - a.y) * f - t * 0.5f, t, t, z, color, true);
    }
  }
}

}  // namespace urbs::game
