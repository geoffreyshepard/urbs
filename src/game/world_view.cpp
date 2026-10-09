#include "game/world_view.hpp"

#include <algorithm>
#include <string>

#include "core/profile.hpp"

namespace urbs::game {

using render::Sprite;

void WorldView::init(const render::Atlas& atlas, const data::Defs& defs) {
  for (int w = 1; w <= 4; ++w)
    for (int h = 1; h <= 4; ++h)
      placeholder_[w - 1][h - 1] = atlas.get("bld/placeholder_" + std::to_string(w) + "x" + std::to_string(h));
  bld_.clear();
  for (const auto& b : defs.buildings) bld_.push_back(atlas.find(b.sprite));
  static const char* kDirs[4] = {"n", "e", "s", "w"};
  for (int c = 0; c < static_cast<int>(sim::CitizenClass::Count); ++c) {
    std::string cn = sim::class_name(static_cast<sim::CitizenClass>(c));
    if (cn == "libertus") cn = "liberti";
    for (int d = 0; d < 4; ++d) {
      for (int f = 0; f < 4; ++f)
        people_[c][d][f] = atlas.get("people/" + cn + "_" + kDirs[d] + "_" + std::to_string(f));
      idle_[c][d] = atlas.get("people/" + cn + "_idle_" + kDirs[d]);
    }
  }
  for (int d = 0; d < 4; ++d)
    for (int f = 0; f < 4; ++f) child_[d][f] = atlas.get("people/child_" + std::string(kDirs[d]) + "_" + std::to_string(f));
  dot = atlas.get("fx/dot");
  white = atlas.get("fx/white");
  construction_ = atlas.find("bld/construction");
}

const Sprite& WorldView::building_sprite(BuildingDefId def, i32 w, i32 h) const {
  if (def.valid() && def.v < bld_.size() && bld_[def.v].valid) return bld_[def.v];
  return placeholder_[std::clamp(w, 1, 4) - 1][std::clamp(h, 1, 4) - 1];
}

void WorldView::draw(const sim::World& w, render::Renderer& r, f32 alpha) {
  URBS_ZONE("world_view");
  const render::Camera& cam = r.camera();
  const Recti vis = cam.visible_world(96);
  const f32 map_h = r.map_px_h();
  const bool far = cam.far();

  // Buildings.
  w.buildings.for_each([&](BuildingId, const sim::Building& b) {
    const f32 px = static_cast<f32>(b.x * sim::kTilePx), py = static_cast<f32>(b.y * sim::kTilePx);
    const f32 fw = static_cast<f32>(b.w * sim::kTilePx), fh = static_cast<f32>(b.h * sim::kTilePx);
    if (!vis.intersects(Recti{static_cast<i32>(px), static_cast<i32>(py) - 64, static_cast<i32>(fw), static_cast<i32>(fh) + 64}))
      return;
    const Sprite& s = building_sprite(b.def, b.w, b.h);
    const f32 foot = py + fh;
    const f32 z = render::depth_for_foot(foot - 1, map_h);
    u32 tint = render::kWhite;
    if (b.state == sim::BState::Construction) tint = render::rgba(200, 190, 170, 255);
    if (b.state == sim::BState::Ruin) tint = render::rgba(90, 80, 70, 255);
    // Scale the sprite to the footprint width; keep its aspect ratio for the roof/front face height.
    const f32 scale = fw / static_cast<f32>(std::max<u16>(1, s.w));
    const f32 sh = static_cast<f32>(s.h) * scale;
    r.push(render::SpriteInstance{px, foot - sh, z, fw, sh, s.u, s.v, s.w, s.h, tint});
    if (b.state == sim::BState::Construction) {
      const f32 prog = b.build_needed ? static_cast<f32>(b.build_progress) / static_cast<f32>(b.build_needed) : 0.0f;
      r.push_rect(px + 2, foot - 4, (fw - 4) * prog, 2, z - 0.0001f, render::rgba(240, 200, 60, 230), true);
    }
  });

  // Citizens (only walking ones are outdoors).
  const auto& C = w.citizens;
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.activity[i] != sim::Activity::Walk || C.path[i] == path::kNoHandle) continue;
    Vec2 p;
    int facing = 2;
    if (!w.citizen_position(i, alpha, p, &facing)) continue;
    if (p.x < static_cast<f32>(vis.x) || p.y < static_cast<f32>(vis.y) || p.x > static_cast<f32>(vis.x + vis.w) ||
        p.y > static_cast<f32>(vis.y + vis.h))
      continue;
    // Spread citizens sharing a path across the road a little.
    const f32 off = static_cast<f32>(static_cast<i32>(i * 2654435761u >> 28) - 8) * 0.45f;
    p.x += facing == 0 || facing == 2 ? off : 0;
    p.y += facing == 1 || facing == 3 ? off * 0.5f : 0;
    const f32 z = render::depth_for_foot(p.y, map_h);
    const int cls = static_cast<int>(C.cls[i]);
    if (far) {
      r.push(render::SpriteInstance{p.x - 1, p.y - 2, z, 2, 2, dot.u, dot.v, dot.w, dot.h, render::rgba(250, 240, 220)});
      continue;
    }
    const f32 el = static_cast<f32>(w.now - C.walk_start[i]) + alpha;
    const int frame = static_cast<int>(el / 2.5f) & 3;
    const bool child = C.age_years(i, w.now) < 14;
    const Sprite& s = child ? child_[facing][frame] : people_[cls][facing][frame];
    r.push_sprite(s, p.x, p.y + 4, z);
  }
}

}  // namespace urbs::game
