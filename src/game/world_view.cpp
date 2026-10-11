#include "game/world_view.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/profile.hpp"
#include "game/iso_draw.hpp"

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
    ds.base_lit = atlas.find(b.sprite + "_lit");
    for (int t = 0; t < 5; ++t) ds.tier_lit[t] = atlas.find(b.sprite + "_t" + std::to_string(t) + "_lit");
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
  namespace iso = render::iso;
  const f32 level = iso::level_at(w.map, b.x, b.y);
  const Vec2 anchor = iso::footprint_anchor(b.x, b.y, b.w, b.h, level);
  const f32 cx = static_cast<f32>(b.x) + static_cast<f32>(b.w) * 0.5f;
  const f32 cy = static_cast<f32>(b.y) + static_cast<f32>(b.h) * 0.5f;
  const f32 z = iso_depth(map_h, cx, cy);
  auto tile_anchor = [&](i32 tx, i32 ty) { return iso::tile_anchor(tx, ty, iso::level_at(w.map, tx, ty)); };
  auto tile_depth = [&](i32 tx, i32 ty) {
    return iso_depth(map_h, static_cast<f32>(tx) + 0.5f, static_cast<f32>(ty) + 0.5f);
  };
  // Ground-attached sprites taller than a flat tile are y-sorted; flat ones lie on the ground.
  auto tall = [&](const Sprite& s) { return s.iso_h() > iso::kTileH + 6.0f; };
  if (b.state == sim::BState::Ruin) {
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const Sprite& s = rubble_[(tx * 7 + ty * 3) & 3];
        if (s.valid)
          r.push(iso_instance(s, tile_anchor(tx, ty), tall(s) ? tile_depth(tx, ty) : render::kDepthRoad - 0.0005f));
      }
    return;
  }
  const Sprite& s = building_sprite(b.def, b.w, b.h, b.tier);
  const f32 top_y = anchor.y - s.iso_h();  // top of the building's art
  if (b.plague > 0 && plague_.valid) {
    // Status mark: an overlay (no cast shadow), pulsing softly so a sick district reads without
    // covering every roof in solid icons.
    const f32 pulse = 0.55f + 0.3f * std::sin(r.time * 2.5f + static_cast<f32>(b.x + b.y));
    const Vec2 c = iso::from_tile(cx, cy, level);
    const f32 sz = 10.0f * kIsoK;
    r.push_translucent(iso_icon(plague_, {c.x, top_y + sz * 0.3f}, sz, sz, 0.03f,
                                render::rgba(255, 255, 255, static_cast<u8>(pulse * 255.0f))));
  }
  if (b.state == sim::BState::Burning) {
    const int frame = static_cast<int>(r.time * 8.0f);
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const int f = (frame + tx * 3 + ty * 5) & 3;
        const Vec2 foot =
            iso::from_tile(static_cast<f32>(tx) + 0.5f, static_cast<f32>(ty) + 0.5f, iso::level_at(w.map, tx, ty));
        // alpha just under opaque: flames cast no shadow (shaders/shadow.vert); drawn just in front
        // of the building so they lick its walls.
        if (fire_[f].valid)
          r.push(iso_instance(fire_[f], foot, std::min(tile_depth(tx, ty), z) - 0.0003f,
                              render::rgba(255, 255, 255, 250)));
        if (smoke_[f].valid)
          r.push(iso_instance(smoke_[f],
                              {foot.x + static_cast<f32>((f * 5) % 7 - 3) * kIsoK,
                               std::min(foot.y, top_y + 40.0f) - static_cast<f32>(f * 4) * kIsoK},
                              0.05f, render::rgba(255, 255, 255, 200)));
      }
  }
  const DefSprites* ds = b.def.v < defs_.size() ? &defs_[b.def.v] : nullptr;
  const u32 tint = render::kWhite;
  const bool building = b.state == sim::BState::Construction;

  // Painted area buildings: one sprite per tile (+ optional hut), line structures autotile.
  if (ds && ds->painted && (ds->tile[0].valid || ds->line_type)) {
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const Vec2 ta = tile_anchor(tx, ty);
        const f32 tz = tile_depth(tx, ty);
        const u8 var = w.map.variant[static_cast<size_t>(w.map.idx(tx, ty))];
        if (building) {
          const Sprite& sc = scaffold_[var & 3].valid ? scaffold_[var & 3] : foundation_;
          if (sc.valid) r.push(iso_instance(sc, ta, tall(sc) ? tz : render::kDepthRoad - 0.0005f));
          continue;
        }
        if (ds->line_type) {
          // Flat line pieces (pipes, sewers, bridge decks) lie on the ground; tall ones (walls,
          // aqueducts) are y-sorted.  Spokes draw over the centre piece.
          const f32 lz = tall(ds->line_c) ? tz : render::kDepthRoad - 0.0004f;
          r.push(iso_instance(ds->line_c, ta, lz, tint));
          for (int sd = 0; sd < 4; ++sd) {
            i32 nx = tx + kDir4X[sd], ny = ty + kDir4Y[sd];
            if (!w.map.in_bounds(nx, ny) || !ds->line[sd].valid) continue;
            BuildingId nb = w.map.building[static_cast<size_t>(w.map.idx(nx, ny))];
            if (nb.valid() && w.buildings.alive(nb) && w.buildings[nb].def == b.def)
              r.push(iso_instance(ds->line[sd], ta, lz - 0.000001f * static_cast<f32>(sd + 1), tint));
          }
          continue;
        }
        const Sprite& ts = ds->tile[var & 3];
        r.push(iso_instance(ts, ta, tall(ts) ? tz : render::kDepthRoad - 0.0005f, tint));
      }
    if (!building && ds->hut.valid) {
      // The hut stands on the footprint's front-left (south-west) tile.
      const i32 hx = b.x, hy = b.y + b.h - 1;
      r.push(iso_instance(ds->hut, tile_anchor(hx, hy), tile_depth(hx, hy) - 0.00001f, tint));
    }
    return;
  }

  if (building) {
    const f32 prog = b.build_needed ? static_cast<f32>(b.build_progress) / static_cast<f32>(b.build_needed) : 0.0f;
    // Foundation and scaffolding; the building rises from the bottom as work progresses.
    for (i32 ty = b.y; ty < b.y + b.h; ++ty)
      for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
        const u8 var = w.map.variant[static_cast<size_t>(w.map.idx(tx, ty))];
        const Vec2 ta = tile_anchor(tx, ty);
        if (foundation_.valid)
          r.push(iso_instance(foundation_, ta, render::kDepthRoad - 0.0005f));
        else
          r.push_diamond(tx, ty, 1, 1, iso::level_at(w.map, tx, ty), render::kDepthRoad - 0.0005f,
                       render::rgba(150, 120, 90, 255), false);
        if (prog > 0.05f && scaffold_[var & 3].valid)
          r.push(iso_instance(scaffold_[var & 3], ta, tile_depth(tx, ty) - 0.00001f));
      }
    if (prog > 0.0f && s.valid) {
      render::SpriteInstance si = iso_instance(s, anchor, z, render::rgba(230, 220, 205, 255));
      const u16 src_h = static_cast<u16>(static_cast<f32>(s.h) * prog);
      const f32 vis_h = si.h * static_cast<f32>(src_h) / static_cast<f32>(std::max<u16>(1, s.h));
      si.y += si.h - vis_h;
      si.h = vis_h;
      si.v = static_cast<u16>(s.v + s.h - src_h);
      si.vh = src_h;
      r.push(si);
    }
    // Progress bar floating over the footprint's front corner.
    const f32 bw = static_cast<f32>(b.w + b.h) * iso::kTileW * 0.25f;
    r.push_rect(anchor.x - bw * 0.5f, anchor.y - 6.0f * kIsoK, bw * prog, 2.0f * kIsoK, 0.01f,
                render::rgba(240, 200, 60, 230), true);
    return;
  }
  r.push(iso_instance(s, anchor, z, tint));

  // Window lights at night: lived-in houses and staffed workplaces, a stable share of them.
  const f32 night = r.grade.params[2];
  if (night > 0.05f && ds) {
    const Sprite& lit = (b.tier < 5 && ds->tier[b.tier].valid) ? ds->tier_lit[b.tier] : ds->base_lit;
    const bool occupied = b.residents > 0 || b.employed > 0 || (b.capacity == 0 && b.jobs == 0);
    const u32 hsh = static_cast<u32>(b.x) * 73856093u ^ static_cast<u32>(b.y) * 19349663u;
    if (lit.valid && occupied && hsh % 5 != 0) {
      const f32 flicker = 0.9f + 0.1f * std::sin(r.time * 3.1f + static_cast<f32>(hsh % 97));
      const u8 a = static_cast<u8>(std::clamp(night * flicker * 1.25f, 0.0f, 1.0f) * 255.0f);
      const f32 lz = z - 0.00002f;
      r.push_light(iso_instance(lit, anchor, lz, render::rgba(255, 236, 190, a)));
      // soft glow: the same mask nudged a little each way, faint
      static constexpr f32 kOff[4][2] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}};
      for (const auto& o : kOff)
        r.push_light(iso_instance(lit, {anchor.x + o[0], anchor.y + o[1]}, lz,
                                  render::rgba(255, 190, 110, static_cast<u8>(a / 5))));
    }
  }
}

namespace {
bool in_rect(const Recti& v, Vec2 p) {
  return p.x >= static_cast<f32>(v.x) && p.y >= static_cast<f32>(v.y) && p.x <= static_cast<f32>(v.x + v.w) &&
         p.y <= static_cast<f32>(v.y + v.h);
}
}  // namespace

void WorldView::draw(const sim::World& w, render::Renderer& r, f32 alpha) {
  URBS_ZONE("world_view");
  namespace iso = render::iso;
  const render::Camera& cam = r.camera();
  const Recti vis = cam.visible_world(static_cast<i32>(iso::kTileW));
  const f32 map_h = iso::map_iso_h(w.map);
  const bool far = cam.far();
  const f32 T = static_cast<f32>(sim::kTilePx);

  // Buildings: cull on the footprint's screen box, extended upward for tall art.
  constexpr f32 kTallest = 900.0f;
  w.buildings.for_each([&](BuildingId, const sim::Building& b) {
    const f32 lv = iso::level_at(w.map, b.x, b.y);
    const f32 left = iso::from_tile(static_cast<f32>(b.x), static_cast<f32>(b.y + b.h)).x;
    const f32 right = iso::from_tile(static_cast<f32>(b.x + b.w), static_cast<f32>(b.y)).x;
    const f32 bottom = iso::from_tile(static_cast<f32>(b.x + b.w), static_cast<f32>(b.y + b.h), lv).y;
    const Recti br{static_cast<i32>(left), static_cast<i32>(bottom - kTallest), static_cast<i32>(right - left),
                   static_cast<i32>(kTallest)};
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
          r.push(iso_instance(s, iso::from_tile(static_cast<f32>(tx) + 0.5f, static_cast<f32>(ty) + 0.5f),
                              iso_depth(map_h, static_cast<f32>(tx) + 0.5f, static_cast<f32>(ty) + 0.5f)));
        ty = hb.y + hb.h + 1;
        break;
      }
  }

  const auto& C = w.citizens;
  const f32 dot_sz = 2.0f * kIsoK;
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.activity[i] != sim::Activity::Walk || C.path[i] == path::kNoHandle) continue;
    Vec2 p;
    int facing = 2;
    if (!w.citizen_position(i, alpha, p, &facing)) continue;
    // Spread walkers sideways across the road so crowds don't stack on one line.
    const f32 off = static_cast<f32>(static_cast<i32>(i * 2654435761u >> 28) - 8) * 0.45f;
    p.x += facing == 0 || facing == 2 ? off : 0;
    p.y += facing == 1 || facing == 3 ? off * 0.5f : 0;
    const Vec2 foot = iso::project(p, iso::level_at_world(w.map, p.x, p.y));
    if (!in_rect(vis, foot)) continue;
    const f32 z = iso_depth(map_h, p.x / T, p.y / T);
    if (far) {
      r.push(render::SpriteInstance{foot.x - dot_sz * 0.5f, foot.y - dot_sz, z, dot_sz, dot_sz, dot.u, dot.v, dot.w,
                                    dot.h, render::rgba(250, 240, 220)});
      continue;
    }
    const f32 el = static_cast<f32>(w.now - C.walk_start[i]) + alpha;
    const int frame = static_cast<int>(el / 2.5f) & 3;
    const bool child = C.age_years(i, w.now) < 14;
    const Sprite& s = child ? child_[facing][frame] : people_[static_cast<int>(C.cls[i])][facing][frame];
    r.push(iso_instance(s, foot, z));
    if (C.carrying[i]) {
      const Sprite& g = good_icon(GoodId{static_cast<u16>(C.carrying[i] - 1)}, false);
      if (g.valid) {
        // Carried goods keep their icon art; size them for the iso figure.
        const f32 head = s.valid ? s.iso_h() : 40.0f;
        const f32 gw = static_cast<f32>(g.w) * 2.5f, gh = static_cast<f32>(g.h) * 2.5f;
        r.push(iso_icon(g, {foot.x, foot.y - head + gh * 0.35f}, gw, gh, z - 0.00001f));
      }
    }
  }
  draw_army(w, r, alpha);
}

void WorldView::draw_army(const sim::World& w, render::Renderer& r, f32 alpha) const {
  namespace iso = render::iso;
  const auto& A = w.army;
  if (A.deployed == 0) return;
  const Recti vis = r.camera().visible_world(static_cast<i32>(iso::kTileW));
  const f32 map_h = iso::map_iso_h(w.map);
  const bool far = r.camera().far();
  const f32 dot_sz = 2.0f * kIsoK;
  for (u32 i = 0; i < A.ids.high_water(); ++i) {
    if (!A.ids.alive_index(i)) continue;
    const f32 tx = A.px[i] + (A.x[i] - A.px[i]) * alpha, ty = A.py[i] + (A.y[i] - A.py[i]) * alpha;
    const Vec2 foot =
        iso::from_tile(tx, ty, iso::level_at(w.map, static_cast<i32>(std::floor(tx)), static_cast<i32>(std::floor(ty))));
    if (!in_rect(vis, foot)) continue;
    const auto& un = A.units[A.unit[i]];
    const f32 z = iso_depth(map_h, tx, ty);
    if (far) {
      const u32 col = un.side == 0 ? render::rgba(230, 60, 50) : render::rgba(90, 160, 230);
      r.push(render::SpriteInstance{foot.x - dot_sz * 0.5f, foot.y - dot_sz, z, dot_sz, dot_sz, dot.u, dot.v, dot.w,
                                    dot.h, col});
      continue;
    }
    const UnitSprites& us = units_[un.def.v];
    const int d = A.facing[i] & 3;
    const Sprite& s = A.attacking[i] ? us.atk[d][(A.attacking[i] / 3) & 1]
                                     : us.walk[d][static_cast<int>(A.walked[i] * 3.0f) & 3];
    // Enemy soldiers get a slight cold tint so the sides read apart.
    r.push(iso_instance(s, foot, z, un.side == 1 ? render::rgba(215, 225, 255) : render::kWhite));
  }
  for (const auto& p : A.projectiles) {
    if (!p.alive) continue;
    // Arrows and bolts fly a little above the ground.
    const Vec2 a = iso::from_tile(p.x, p.y, iso::level_at(w.map, static_cast<i32>(p.x), static_cast<i32>(p.y)));
    if (!in_rect(vis, a)) continue;
    r.push_rect(a.x - 1.5f * kIsoK, a.y - 12.0f * kIsoK, 3.0f * kIsoK, 1.0f * kIsoK, 0.03f,
                render::rgba(70, 50, 30, 255), false);
  }
}

}  // namespace urbs::game
