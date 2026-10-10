#include "game/world_view.hpp"

#include <algorithm>
#include <string>

#include "core/profile.hpp"

namespace urbs::game {

using render::Sprite;

void WorldView::init(const render::Atlas& atlas, const data::Defs& defs, const std::string& region) {
  auto find = [&](const std::string& name) {
    if (!region.empty()) {
      const Sprite v = atlas.find(name + "@" + region);
      if (v.valid) return v;
    }
    return atlas.find(name);
  };
  for (int w = 1; w <= 4; ++w)
    for (int h = 1; h <= 4; ++h)
      placeholder_[w - 1][h - 1] = atlas.get("bld/placeholder_" + std::to_string(w) + "x" + std::to_string(h));
  defs_.clear();
  static const char* kSides[4] = {"n", "e", "s", "w"};
  for (const auto& b : defs.buildings) {
    DefSprites ds;
    ds.base = find(b.sprite);
    for (int t = 0; t < 5; ++t) ds.tier[t] = find(b.sprite + "_t" + std::to_string(t));
    for (int v = 0; v < 4; ++v) ds.tile[v] = find(b.sprite + "_tile_" + std::to_string(v));
    ds.hut = find(b.sprite + "_hut");
    ds.line_c = find(b.sprite + "_c");
    for (int s = 0; s < 4; ++s) ds.line[s] = find(b.sprite + "_" + kSides[s]);
    ds.painted = b.painted;
    ds.line_type = ds.line_c.valid;
    defs_.push_back(ds);
  }
  goods_.clear();
  goods16_.clear();
  for (const auto& g : defs.goods) {
    goods_.push_back(atlas.find("goods/" + g.id));
    goods16_.push_back(atlas.find("goods/" + g.id + "_16"));
  }
  static const char* kDirs[4] = {"n", "e", "s", "w"};
  for (int c = 0; c < static_cast<int>(sim::CitizenClass::Count); ++c) {
    std::string cn = sim::class_name(static_cast<sim::CitizenClass>(c));
    if (cn == "libertus") cn = "liberti";
    for (int d = 0; d < 4; ++d)
      for (int f = 0; f < 4; ++f)
        people_[c][d][f] = atlas.get("people/" + cn + "_" + kDirs[d] + "_" + std::to_string(f));
  }
  for (int d = 0; d < 4; ++d)
    for (int f = 0; f < 4; ++f) child_[d][f] = atlas.get("people/child_" + std::string(kDirs[d]) + "_" + std::to_string(f));
  for (int v = 0; v < 4; ++v) scaffold_[v] = atlas.find("fx/scaffold_" + std::to_string(v));
  foundation_ = atlas.find("fx/foundation");
  for (int d = 0; d < 4; ++d) ship_[d] = atlas.find(std::string("fx/ship_") + kDirs[d]);
  for (int f = 0; f < 4; ++f) {
    fire_[f] = atlas.find("fx/fire_" + std::to_string(f));
    smoke_[f] = atlas.find("fx/smoke_" + std::to_string(f));
    rubble_[f] = atlas.find("fx/rubble_" + std::to_string(f));
  }
  plague_ = atlas.find("fx/plague_mark");
  units_.clear();
  for (const auto& u : defs.units) {
    UnitSprites us;
    std::string base = u.sprite == "pleb" || u.sprite == "slave" ? u.sprite : u.sprite;
    for (int d = 0; d < 4; ++d) {
      for (int f = 0; f < 4; ++f) {
        us.walk[d][f] = atlas.find("people/" + base + "_" + kDirs[d] + "_" + std::to_string(f));
        if (!us.walk[d][f].valid) us.walk[d][f] = atlas.get("people/soldier_" + std::string(kDirs[d]) + "_" + std::to_string(f));
      }
      for (int f = 0; f < 2; ++f) {
        us.atk[d][f] = atlas.find("people/" + base + "_atk_" + kDirs[d] + "_" + std::to_string(f));
        if (!us.atk[d][f].valid) us.atk[d][f] = us.walk[d][f];
      }
    }
    if (u.sprite == "ballista") {
      for (int d = 0; d < 4; ++d) {
        const render::Sprite s = atlas.get("people/ballista_" + std::string(kDirs[d]));
        for (int f = 0; f < 4; ++f) us.walk[d][f] = s;
        for (int f = 0; f < 2; ++f) us.atk[d][f] = s;
      }
    }
    units_.push_back(us);
  }
  dot = atlas.get("fx/dot");
  white = atlas.get("fx/white");
}

const Sprite& WorldView::building_sprite(BuildingDefId def, i32 w, i32 h, u8 tier) const {
  if (def.valid() && def.v < defs_.size()) {
    const DefSprites& d = defs_[def.v];
    if (tier < 5 && d.tier[tier].valid) return d.tier[tier];
    if (d.base.valid) return d.base;
  }
  return placeholder_[std::clamp(w, 1, 4) - 1][std::clamp(h, 1, 4) - 1];
}

const Sprite& WorldView::good_icon(GoodId g, bool big) const {
  static Sprite none;
  if (!g.valid() || g.v >= goods_.size()) return none;
  return big ? goods16_[g.v] : goods_[g.v];
}

void WorldView::draw_building(const sim::World& w, render::Renderer& r, const sim::Building& b, f32 map_h) const {
  const f32 T = static_cast<f32>(sim::kTilePx);
  const f32 px = static_cast<f32>(b.x) * T, py = static_cast<f32>(b.y) * T;
  const f32 fw = static_cast<f32>(b.w) * T, fh = static_cast<f32>(b.h) * T;
  const f32 foot = py + fh;
  if (b.state == sim::BState::Ruin) {
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const Sprite& s = rubble_[(tx * 7 + ty * 3) & 3];
        if (s.valid) r.push_sprite(s, static_cast<f32>(tx) * T, static_cast<f32>(ty) * T, render::kDepthRoad - 0.0005f);
      }
    return;
  }
  if (b.plague > 0 && plague_.valid)
    r.push_sprite(plague_, px + fw / 2 - 4, py - 10, render::depth_for_foot(foot, map_h) - 0.0002f);
  if (b.state == sim::BState::Burning) {
    const int frame = static_cast<int>(r.time * 8.0f);
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const int f = (frame + tx * 3 + ty * 5) & 3;
        const f32 x = static_cast<f32>(tx) * T, y = static_cast<f32>(ty + 1) * T;
        if (fire_[f].valid) r.push_sprite(fire_[f], x, y - 1, render::depth_for_foot(y, map_h) - 0.0003f);
        if (smoke_[f].valid)
          r.push_sprite(smoke_[f], x + static_cast<f32>((f * 5) % 7) - 3.0f, y - 30.0f - static_cast<f32>(f * 4),
                        0.05f, render::rgba(255, 255, 255, 200));
      }
  }
  const DefSprites* ds = b.def.v < defs_.size() ? &defs_[b.def.v] : nullptr;
  u32 tint = render::kWhite;
  if (b.state == sim::BState::Ruin) tint = render::rgba(90, 80, 70, 255);
  const bool building = b.state == sim::BState::Construction;

  // Painted area buildings: one sprite per tile (+ optional hut), line structures autotile.
  if (ds && ds->painted && (ds->tile[0].valid || ds->line_type)) {
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const f32 x = static_cast<f32>(tx) * T, y = static_cast<f32>(ty) * T;
        const u8 var = w.map.variant[static_cast<size_t>(w.map.idx(tx, ty))];
        if (building) {
          const Sprite& s = scaffold_[var & 3].valid ? scaffold_[var & 3] : foundation_;
          if (s.valid) r.push_sprite(s, x, y, render::kDepthRoad - 0.0005f);
          continue;
        }
        if (ds->line_type) {
          const f32 z = render::depth_for_foot(y + T - 1, map_h);
          r.push_sprite(ds->line_c, x, y + T - 1, z, tint);
          for (int s = 0; s < 4; ++s) {
            i32 nx = tx + kDir4X[s], ny = ty + kDir4Y[s];
            if (!w.map.in_bounds(nx, ny)) continue;
            BuildingId nb = w.map.building[static_cast<size_t>(w.map.idx(nx, ny))];
            if (nb.valid() && w.buildings.alive(nb) && w.buildings[nb].def == b.def)
              r.push_sprite(ds->line[s], x, y + T - 1, z, tint);
          }
          continue;
        }
        r.push_sprite(ds->tile[var & 3], x, y, render::kDepthRoad - 0.0005f, tint);
      }
    if (!building && ds->hut.valid)
      r.push_sprite(ds->hut, px, foot - 1, render::depth_for_foot(foot - 1, map_h), tint);
    return;
  }

  const Sprite& s = building_sprite(b.def, b.w, b.h, b.tier);
  const f32 z = render::depth_for_foot(foot - 1, map_h);
  const f32 scale = fw / static_cast<f32>(std::max<u16>(1, s.w));
  const f32 sh = static_cast<f32>(s.h) * scale;
  if (building) {
    const f32 prog = b.build_needed ? static_cast<f32>(b.build_progress) / static_cast<f32>(b.build_needed) : 0.0f;
    // Foundation and scaffolding; the building rises from the bottom as work progresses.
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const u8 var = w.map.variant[static_cast<size_t>(w.map.idx(tx, ty))];
        const Sprite& f = foundation_.valid ? foundation_ : white;
        r.push_sprite(f, static_cast<f32>(tx) * T, static_cast<f32>(ty) * T, render::kDepthRoad - 0.0005f,
                      foundation_.valid ? render::kWhite : render::rgba(150, 120, 90, 255));
        if (prog > 0.05f && scaffold_[var & 3].valid)
          r.push_sprite(scaffold_[var & 3], static_cast<f32>(tx) * T, static_cast<f32>(ty) * T,
                        render::depth_for_foot(static_cast<f32>(ty) * T + T - 1, map_h));
      }
    if (prog > 0.0f) {
      const f32 vis_h = sh * prog;
      const u16 src_h = static_cast<u16>(static_cast<f32>(s.h) * prog);
      r.push(render::SpriteInstance{px, foot - vis_h, z, fw, vis_h, s.u, static_cast<u16>(s.v + s.h - src_h), s.w, src_h,
                                    render::rgba(230, 220, 205, 255)});
    }
    r.push_rect(px + 2, foot - 4, (fw - 4) * prog, 2, 0.01f, render::rgba(240, 200, 60, 230), true);
    return;
  }
  r.push(render::SpriteInstance{px, foot - sh, z, fw, sh, s.u, s.v, s.w, s.h, tint});
}

void WorldView::draw(const sim::World& w, render::Renderer& r, f32 alpha) {
  URBS_ZONE("world_view");
  const render::Camera& cam = r.camera();
  const Recti vis = cam.visible_world(96);
  const f32 map_h = r.map_px_h();
  const bool far = cam.far();
  const f32 T = static_cast<f32>(sim::kTilePx);

  w.buildings.for_each([&](BuildingId, const sim::Building& b) {
    const Recti br{b.x * sim::kTilePx, b.y * sim::kTilePx - 96, b.w * sim::kTilePx, b.h * sim::kTilePx + 96};
    if (vis.intersects(br)) draw_building(w, r, b, map_h);
  });

  // Ships docked at harbors.
  for (const auto& rs : w.routes) {
    if (rs.ship_until <= w.now || !rs.ship_at.valid() || !w.buildings.alive(rs.ship_at)) continue;
    const auto& hb = w.buildings[rs.ship_at];
    // Moor on the first water tile next to the harbor.
    for (i32 ty = hb.y - 1; ty <= hb.y + hb.h; ++ty)
      for (i32 tx = hb.x - 1; tx <= hb.x + hb.w; ++tx) {
        if (!w.map.in_bounds(tx, ty) || !sim::terrain_water(w.map.terrain[static_cast<size_t>(w.map.idx(tx, ty))])) continue;
        const Sprite& s = ship_[1];
        if (s.valid)
          r.push_sprite(s, (static_cast<f32>(tx) + 0.5f) * T, (static_cast<f32>(ty) + 1.0f) * T,
                        render::depth_for_foot((static_cast<f32>(ty) + 1.0f) * T, map_h));
        ty = hb.y + hb.h + 1;
        break;
      }
  }

  const auto& C = w.citizens;
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.activity[i] != sim::Activity::Walk || C.path[i] == path::kNoHandle) continue;
    Vec2 p;
    int facing = 2;
    if (!w.citizen_position(i, alpha, p, &facing)) continue;
    if (p.x < static_cast<f32>(vis.x) || p.y < static_cast<f32>(vis.y) || p.x > static_cast<f32>(vis.x + vis.w) ||
        p.y > static_cast<f32>(vis.y + vis.h))
      continue;
    const f32 off = static_cast<f32>(static_cast<i32>(i * 2654435761u >> 28) - 8) * 0.45f;
    p.x += facing == 0 || facing == 2 ? off : 0;
    p.y += facing == 1 || facing == 3 ? off * 0.5f : 0;
    const f32 z = render::depth_for_foot(p.y, map_h);
    if (far) {
      r.push(render::SpriteInstance{p.x - 1, p.y - 2, z, 2, 2, dot.u, dot.v, dot.w, dot.h, render::rgba(250, 240, 220)});
      continue;
    }
    const f32 el = static_cast<f32>(w.now - C.walk_start[i]) + alpha;
    const int frame = static_cast<int>(el / 2.5f) & 3;
    const bool child = C.age_years(i, w.now) < 14;
    const Sprite& s = child ? child_[facing][frame] : people_[static_cast<int>(C.cls[i])][facing][frame];
    r.push_sprite(s, p.x, p.y + 4, z);
    if (C.carrying[i]) {
      const Sprite& g = good_icon(GoodId{static_cast<u16>(C.carrying[i] - 1)}, false);
      if (g.valid) r.push_sprite(g, p.x - static_cast<f32>(g.w) / 2, p.y - 9 - static_cast<f32>(g.h), z - 0.00001f);
    }
  }
  (void)T;
  draw_army(w, r, alpha);
}

void WorldView::draw_army(const sim::World& w, render::Renderer& r, f32 alpha) const {
  const auto& A = w.army;
  if (A.deployed == 0) return;
  const f32 T = static_cast<f32>(sim::kTilePx);
  const Recti vis = r.camera().visible_world(32);
  const f32 map_h = r.map_px_h();
  const bool far = r.camera().far();
  for (u32 i = 0; i < A.ids.high_water(); ++i) {
    if (!A.ids.alive_index(i)) continue;
    const f32 x = (A.px[i] + (A.x[i] - A.px[i]) * alpha) * T, y = (A.py[i] + (A.y[i] - A.py[i]) * alpha) * T;
    if (x < static_cast<f32>(vis.x) || y < static_cast<f32>(vis.y) || x > static_cast<f32>(vis.x + vis.w) ||
        y > static_cast<f32>(vis.y + vis.h))
      continue;
    const auto& un = A.units[A.unit[i]];
    const f32 z = render::depth_for_foot(y, map_h);
    if (far) {
      const u32 col = un.side == 0 ? render::rgba(230, 60, 50) : render::rgba(90, 160, 230);
      r.push(render::SpriteInstance{x - 1, y - 2, z, 2, 2, dot.u, dot.v, dot.w, dot.h, col});
      continue;
    }
    const UnitSprites& us = units_[un.def.v];
    const int d = A.facing[i] & 3;
    const Sprite& s = A.attacking[i] ? us.atk[d][(A.attacking[i] / 3) & 1]
                                     : us.walk[d][static_cast<int>(A.walked[i] * 3.0f) & 3];
    // Enemy soldiers get a slight cold tint so the sides read apart.
    r.push_sprite(s, x, y + 4, z, un.side == 1 ? render::rgba(215, 225, 255) : render::kWhite);
  }
  for (const auto& p : A.projectiles) {
    if (!p.alive) continue;
    const f32 x = p.x * T, y = p.y * T;
    if (x < static_cast<f32>(vis.x) || y < static_cast<f32>(vis.y) || x > static_cast<f32>(vis.x + vis.w) ||
        y > static_cast<f32>(vis.y + vis.h))
      continue;
    r.push_rect(x - 1, y, 3, 1, 0.03f, render::rgba(70, 50, 30, 255), false);
  }
}

}  // namespace urbs::game
