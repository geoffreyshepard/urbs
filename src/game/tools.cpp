#include "game/tools.hpp"

#include <algorithm>

#include "game/iso_draw.hpp"
#include "game/world_view.hpp"

namespace urbs::game {

bool Tools::mouse_down(Vec2i tile) {
  if (kind == ToolKind::Inspect) return false;
  start_ = tile;
  hover_ = tile;
  dragging_ = true;
  return true;
}

Recti Tools::footprint(const sim::World& w) const {
  if (kind == ToolKind::Demolish) {
    Vec2i a = dragging_ ? start_ : hover_, b = hover_;
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x) + 1, std::abs(a.y - b.y) + 1};
  }
  if (kind != ToolKind::Build || !def.valid()) return {};
  const auto& d = w.defs->b(def);
  if (d.painted) {
    Vec2i a = dragging_ ? start_ : hover_, b = hover_;
    i32 x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
    i32 ww = std::clamp(std::abs(a.x - b.x) + 1, static_cast<i32>(d.min_w), static_cast<i32>(d.max_w));
    i32 hh = std::clamp(std::abs(a.y - b.y) + 1, static_cast<i32>(d.min_h), static_cast<i32>(d.max_h));
    if (!dragging_) {
      ww = std::max<i32>(d.min_w, d.w);
      hh = std::max<i32>(d.min_h, d.h);
    }
    return {x0, y0, ww, hh};
  }
  return {hover_.x - (d.w - 1) / 2, hover_.y - (d.h - 1) / 2, d.w, d.h};
}

i64 Tools::cost(const sim::World& w) const {
  if (kind == ToolKind::Road) {
    Vec2i a = dragging_ ? start_ : hover_;
    i64 len = std::max(std::abs(hover_.x - a.x), std::abs(hover_.y - a.y)) + 1;
    return len * (road == sim::Road::Paved ? 4 : 1);
  }
  if (kind != ToolKind::Build || !def.valid()) return 0;
  const auto& d = w.defs->b(def);
  Recti f = footprint(w);
  if (!d.painted) return d.cost_denarii;
  return std::max<i64>(1, static_cast<i64>(d.cost_denarii) * f.w * f.h / std::max(1, d.w * d.h));
}

bool Tools::mouse_up(Vec2i tile, sim::World& w) {
  if (!dragging_) return false;
  hover_ = tile;
  sim::Command c;
  switch (kind) {
    case ToolKind::Build: {
      const auto& d = w.defs->b(def);
      if (d.layer == 1 || d.layer == 2) {
        c.type = sim::CmdType::PlaceUnder;
        c.def = d.layer;
        c.value = 1;
        c.x0 = start_.x;
        c.y0 = start_.y;
        c.x1 = tile.x;
        c.y1 = tile.y;
        w.push(c);
        break;
      }
      if (d.layer == 3) {
        c.type = sim::CmdType::PlaceRoad;
        c.def = static_cast<u16>(sim::Road::Bridge);
        c.x0 = start_.x;
        c.y0 = start_.y;
        c.x1 = tile.x;
        c.y1 = tile.y;
        w.push(c);
        break;
      }
      Recti f = footprint(w);
      c.type = sim::CmdType::PlaceBuilding;
      c.def = def.v;
      c.value = god;
      c.x0 = f.x;
      c.y0 = f.y;
      c.x1 = f.w;
      c.y1 = f.h;
      w.push(c);
      break;
    }
    case ToolKind::Road:
      c.type = sim::CmdType::PlaceRoad;
      c.def = static_cast<u16>(road);
      c.x0 = start_.x;
      c.y0 = start_.y;
      c.x1 = tile.x;
      c.y1 = tile.y;
      w.push(c);
      break;
    case ToolKind::Demolish: {
      Recti f = footprint(w);
      c.type = sim::CmdType::Demolish;
      c.x0 = f.x;
      c.y0 = f.y;
      c.x1 = f.x + f.w - 1;
      c.y1 = f.y + f.h - 1;
      w.push(c);
      break;
    }
    default: break;
  }
  dragging_ = false;
  return true;
}

void Tools::draw(const sim::World& w, const WorldView& view, render::Renderer& r) const {
  namespace iso = render::iso;
  const u32 ok_col = render::rgba(80, 220, 90, 110), bad_col = render::rgba(230, 60, 50, 120);
  auto tile = [&](i32 x, i32 y, u32 col) { r.push_diamond(x, y, 1, 1, iso::level_at(w.map, x, y), 0.03f, col); };
  switch (kind) {
    case ToolKind::Build: {
      if (!def.valid()) return;
      const auto& ld = w.defs->b(def);
      if (ld.layer != 0) {
        // Line tools: pipes and sewers go 4-connected (x then y), bridges like roads.
        Vec2i a = dragging_ ? start_ : hover_, b = hover_;
        i32 x = a.x, y = a.y;
        for (int n = 0; n < 2048; ++n) {
          tile(x, y, ld.layer == 2 ? render::rgba(150, 100, 60, 140) : render::rgba(60, 140, 230, 140));
          if (x == b.x && y == b.y) break;
          if (ld.layer == 3) {
            x += (b.x > x) - (b.x < x);
            y += (b.y > y) - (b.y < y);
          } else if (x != b.x) {
            x += (b.x > x) - (b.x < x);
          } else {
            y += (b.y > y) - (b.y < y);
          }
        }
        break;
      }
      Recti f = footprint(w);
      const bool ok = w.can_place(def, f.x, f.y, f.w, f.h) && w.econ.treasury >= cost(w);
      const f32 level = iso::level_at(w.map, f.x, f.y);
      r.push_diamond(f.x, f.y, f.w, f.h, level, 0.03f, ok ? ok_col : bad_col);
      const auto& d = w.defs->b(def);
      if (!d.painted) {
        const render::Sprite& s = view.building_sprite(def, f.w, f.h);
        if (s.valid)
          r.push_translucent(iso_instance(s, iso::footprint_anchor(f.x, f.y, f.w, f.h, level), 0.02f,
                                          render::rgba(255, 255, 255, 170)));
      }
      break;
    }
    case ToolKind::Road: {
      Vec2i a = dragging_ ? start_ : hover_, b = hover_;
      i32 x = a.x, y = a.y;
      for (int n = 0; n < 2048; ++n) {
        bool ok = w.map.in_bounds(x, y) && !w.map.building[static_cast<size_t>(w.map.idx(x, y))].valid() &&
                  !sim::terrain_water(w.map.terrain[static_cast<size_t>(w.map.idx(x, y))]);
        tile(x, y, ok ? ok_col : bad_col);
        if (x == b.x && y == b.y) break;
        x += (b.x > x) - (b.x < x);
        y += (b.y > y) - (b.y < y);
      }
      break;
    }
    case ToolKind::Demolish: {
      Recti f = footprint(w);
      r.push_diamond(f.x, f.y, f.w, f.h, iso::level_at(w.map, f.x, f.y), 0.03f, render::rgba(230, 60, 50, 90));
      break;
    }
    default: break;
  }
}

}  // namespace urbs::game
