#include "game/overlays.hpp"

#include <algorithm>

#include "render/gpu.hpp"

namespace urbs::game {

namespace {
bool g_colorblind = false;

u32 ramp(i32 v, i32 lo, i32 hi) {
  const f32 t = std::clamp(static_cast<f32>(v - lo) / static_cast<f32>(std::max(1, hi - lo)), 0.0f, 1.0f);
  if (g_colorblind) {
    // orange (bad) -> pale grey -> blue (good): readable with red-green colour blindness
    const auto mix = [](f32 a, f32 b, f32 k) { return static_cast<u8>(a + (b - a) * k); };
    if (t < 0.5f) return render::rgba(mix(235, 200, t * 2), mix(120, 200, t * 2), mix(20, 200, t * 2), 150);
    return render::rgba(mix(200, 40, (t - 0.5f) * 2), mix(200, 120, (t - 0.5f) * 2), mix(200, 235, (t - 0.5f) * 2), 150);
  }
  // red (bad) -> yellow -> green (good)
  const u8 r = static_cast<u8>(t < 0.5f ? 230 : 230 - (t - 0.5f) * 2 * 170);
  const u8 g = static_cast<u8>(t < 0.5f ? 60 + t * 2 * 160 : 220);
  return render::rgba(r, g, 50, 140);
}
}  // namespace

void set_overlay_colorblind(bool on) { g_colorblind = on; }

void build_overlay(const sim::World& w, Overlay mode, ServiceId service, std::vector<u32>& out) {
  const auto& m = w.map;
  out.assign(static_cast<size_t>(m.w * m.h), 0);
  switch (mode) {
    case Overlay::Desirability:
      for (size_t i = 0; i < out.size(); ++i)
        if (m.desir[i] != 0) out[i] = ramp(m.desir[i], -12, 12);
      break;
    case Overlay::Water: {
      // Best water access cost (well or fountain), in walking ticks.
      for (ServiceId s : w.need_services[static_cast<int>(data::NeedKind::Water)]) {
        if (!w.services.ready(s.v)) continue;
        const u16 range = w.defs->services[s.v].range;
        for (size_t i = 0; i < out.size(); ++i) {
          const u16 c = w.services.cost_ticks(s.v, static_cast<i32>(i));
          if (c > range) continue;
          const u32 col = render::rgba(60, 140, 230, static_cast<u8>(170 - 120 * c / std::max<u16>(1, range)));
          if ((out[i] >> 24) < (col >> 24)) out[i] = col;
        }
      }
      break;
    }
    case Overlay::Pipes:
      for (size_t i = 0; i < out.size(); ++i) {
        const u8 u = m.under[i], wv = m.water[i];
        if (u & 1) out[i] = (wv & 2) ? render::rgba(60, 150, 255, 200) : render::rgba(120, 120, 140, 180);
        if (u & 2) out[i] = (wv & 4) ? render::rgba(150, 100, 50, 200) : render::rgba(100, 80, 70, 180);
        if (wv & 1) out[i] = render::rgba(80, 220, 255, 200);
      }
      break;
    case Overlay::Service:
      if (service.valid() && w.services.ready(service.v)) {
        const u16 range = w.defs->services[service.v].range;
        for (size_t i = 0; i < out.size(); ++i) {
          const u16 c = w.services.cost_ticks(service.v, static_cast<i32>(i));
          if (c == 0xFFFF) continue;
          out[i] = c <= range ? ramp(range - c, 0, range) : (ramp(0, 0, 1) & 0x00FFFFFFu) | (60u << 24);
        }
      }
      break;
    case Overlay::Housing:
      w.buildings.for_each([&](BuildingId, const sim::Building& b) {
        const auto& d = w.defs->b(b.def);
        if (d.kind != data::BuildingKind::House) return;
        const u8 max_t = w.defs->max_tier(d.house_line);
        const u32 col = d.house_line == 1 ? render::rgba(200, 120, 230, 170) : ramp(b.tier, 0, std::max<u8>(1, max_t));
        for (i32 y = b.y; y < b.y + b.h; ++y)
          for (i32 x = b.x; x < b.x + b.w; ++x) out[static_cast<size_t>(m.idx(x, y))] = col;
      });
      break;
    case Overlay::Elevation:
      for (size_t i = 0; i < out.size(); ++i) {
        const u8 v = static_cast<u8>(40 + m.height[i] * 30);
        out[i] = render::rgba(v, v, v, 150);
      }
      break;
    case Overlay::Fertility:
      for (size_t i = 0; i < out.size(); ++i)
        if (!sim::terrain_water(m.terrain[i])) out[i] = ramp(m.fertility[i], 60, 220);
      break;
    case Overlay::Groundwater:
      for (size_t i = 0; i < out.size(); ++i)
        if (!sim::terrain_water(m.terrain[i])) out[i] = ramp(m.groundwater[i], 40, 220);
      break;
    default: break;
  }
}

}  // namespace urbs::game
