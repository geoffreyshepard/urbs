#include "game/lighting.hpp"

#include <algorithm>
#include <cmath>

namespace urbs::game {

namespace {

struct Preset {
  f32 hour;
  f32 st[3], exposure;  // shadow tint
  f32 lt[3], sat;       // light tint
  f32 lift[3], vignette;
  f32 contrast;
  f32 shadow;      // shadow strength 0..1
  f32 shadow_len;  // shear per px of height
};

// Keyframes over the day; the grade is interpolated between neighbours.
constexpr Preset kDay[] = {
    {0.0f,
     {0.60f, 0.70f, 1.16f},
     0.56f,
     {0.80f, 0.88f, 1.10f},
     0.55f,
     {0.015f, 0.020f, 0.045f},
     0.45f,
     1.00f,
     0.0f,
     0.30f},
    {4.6f,
     {0.60f, 0.70f, 1.16f},
     0.56f,
     {0.80f, 0.88f, 1.10f},
     0.55f,
     {0.015f, 0.020f, 0.045f},
     0.45f,
     1.00f,
     0.0f,
     0.30f},
    {5.8f,
     {0.80f, 0.82f, 1.08f},
     0.74f,
     {1.14f, 0.95f, 0.86f},
     0.82f,
     {0.030f, 0.020f, 0.040f},
     0.36f,
     1.00f,
     0.45f,
     0.80f},
    {7.5f,
     {0.92f, 0.94f, 1.06f},
     0.96f,
     {1.07f, 1.00f, 0.91f},
     0.92f,
     {0.020f, 0.020f, 0.035f},
     0.28f,
     1.03f,
     0.85f,
     0.45f},
    {10.0f,
     {0.94f, 0.96f, 1.05f},
     1.00f,
     {1.04f, 1.01f, 0.94f},
     0.93f,
     {0.020f, 0.020f, 0.030f},
     0.24f,
     1.04f,
     1.0f,
     0.24f},
    {15.5f,
     {0.94f, 0.96f, 1.05f},
     1.00f,
     {1.04f, 1.01f, 0.94f},
     0.93f,
     {0.020f, 0.020f, 0.030f},
     0.24f,
     1.04f,
     1.0f,
     0.26f},
    {18.2f,
     {0.86f, 0.86f, 1.06f},
     0.95f,
     {1.14f, 0.98f, 0.80f},
     0.95f,
     {0.030f, 0.020f, 0.030f},
     0.32f,
     1.04f,
     0.85f,
     0.60f},
    {20.0f,
     {0.72f, 0.75f, 1.08f},
     0.68f,
     {1.10f, 0.88f, 0.82f},
     0.74f,
     {0.025f, 0.020f, 0.045f},
     0.40f,
     1.00f,
     0.35f,
     0.85f},
    {21.6f,
     {0.60f, 0.70f, 1.16f},
     0.56f,
     {0.80f, 0.88f, 1.10f},
     0.55f,
     {0.015f, 0.020f, 0.045f},
     0.45f,
     1.00f,
     0.0f,
     0.30f},
    {24.0f,
     {0.60f, 0.70f, 1.16f},
     0.56f,
     {0.80f, 0.88f, 1.10f},
     0.55f,
     {0.015f, 0.020f, 0.045f},
     0.45f,
     1.00f,
     0.0f,
     0.30f},
};

f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

Preset at_hour(f32 h) {
  int i = 0;
  while (i + 2 < static_cast<int>(std::size(kDay)) && kDay[i + 1].hour <= h) ++i;
  const Preset& a = kDay[i];
  const Preset& b = kDay[i + 1];
  f32 t = std::clamp((h - a.hour) / std::max(0.001f, b.hour - a.hour), 0.0f, 1.0f);
  t = t * t * (3 - 2 * t);
  Preset p{};
  for (int k = 0; k < 3; ++k) {
    p.st[k] = lerp(a.st[k], b.st[k], t);
    p.lt[k] = lerp(a.lt[k], b.lt[k], t);
    p.lift[k] = lerp(a.lift[k], b.lift[k], t);
  }
  p.exposure = lerp(a.exposure, b.exposure, t);
  p.sat = lerp(a.sat, b.sat, t);
  p.vignette = lerp(a.vignette, b.vignette, t);
  p.contrast = lerp(a.contrast, b.contrast, t);
  p.shadow = lerp(a.shadow, b.shadow, t);
  p.shadow_len = lerp(a.shadow_len, b.shadow_len, t);
  return p;
}

void mul3(f32* v, f32 r, f32 g, f32 b) {
  v[0] *= r;
  v[1] *= g;
  v[2] *= b;
}

}  // namespace

void Lighting::update(const sim::World& w, sim::Weather weather, f32 alpha, f32 dt) {
  const f32 hour = (static_cast<f32>(sim::tick_of_day(w.now)) + alpha) / static_cast<f32>(kTicksPerDay) * 24.0f;
  Preset p = at_hour(hour);

  switch (sim::season(w.now)) {
    case sim::Season::Spring:
      p.sat *= 1.03f;
      break;
    case sim::Season::Summer:
      mul3(p.lt, 1.03f, 1.0f, 0.95f);
      break;
    case sim::Season::Autumn:
      mul3(p.lt, 1.04f, 0.99f, 0.92f);
      break;
    case sim::Season::Winter:
      p.sat *= 0.92f;
      mul3(p.st, 0.97f, 0.98f, 1.04f);
      break;
  }
  f32 overcast = 0.0f;
  switch (weather) {
    case sim::Weather::Rain:
      p.exposure *= 0.88f;
      p.sat *= 0.80f;
      p.contrast *= 0.95f;
      mul3(p.st, 0.95f, 0.97f, 1.04f);
      overcast = 0.65f;
      break;
    case sim::Weather::Storm:
      p.exposure *= 0.74f;
      p.sat *= 0.60f;
      p.contrast *= 0.93f;
      mul3(p.st, 0.92f, 0.95f, 1.06f);
      overcast = 0.85f;
      break;
    case sim::Weather::Snow:
      p.exposure *= 1.0f;
      p.sat *= 0.84f;
      mul3(p.lt, 0.97f, 1.0f, 1.05f);
      overcast = 0.6f;
      break;
    case sim::Weather::Heat:
      mul3(p.lt, 1.05f, 1.0f, 0.90f);
      p.lift[0] += 0.02f;
      p.lift[1] += 0.015f;
      break;
    default:
      break;
  }

  render::Renderer::Grade g;
  render::Renderer::Shadows s;
  for (int k = 0; k < 3; ++k) {
    g.shadow_tint[k] = p.st[k];
    g.light_tint[k] = p.lt[k];
    g.lift[k] = p.lift[k];
  }
  g.shadow_tint[3] = p.exposure;
  g.light_tint[3] = p.sat;
  g.lift[3] = p.vignette;
  g.params[0] = p.contrast;
  g.params[2] = 1.0f - std::clamp(p.exposure * 1.6f - 0.6f, 0.0f, 1.0f);  // night amount (lights)

  // Shadows lean right and down (the art is lit from the top left); they lengthen toward dawn and dusk.
  const f32 strength = p.shadow * (1.0f - overcast);
  const f32 base[3] = {0.58f, 0.60f, 0.76f};
  for (int k = 0; k < 3; ++k) s.color[k] = lerp(1.0f, base[k], strength);
  s.shear[0] = p.shadow_len;
  s.shear[1] = 0.10f;
  s.shear[2] = 2.0f;
  s.shear[3] = 1.0f;
  s.enabled = strength > 0.02f;

  const f32 k = (!primed || dt <= 0) ? 1.0f : 1.0f - std::exp(-dt * 1.5f);
  auto blend = [&](f32* dst, const f32* src, int n) {
    for (int i = 0; i < n; ++i) dst[i] = lerp(dst[i], src[i], k);
  };
  blend(grade.shadow_tint, g.shadow_tint, 4);
  blend(grade.light_tint, g.light_tint, 4);
  blend(grade.lift, g.lift, 4);
  blend(grade.params, g.params, 4);
  blend(shadows.color, s.color, 4);
  blend(shadows.shear, s.shear, 4);
  shadows.enabled = s.enabled || shadows.color[0] < 0.98f;

  // Ground: dryness, dry patches and browning follow the year (keyed mid-season, wrapping round),
  // settled snow follows the weather in sim time.
  static constexpr f32 kSeasonGround[4][3] = {
      {0.00f, 0.25f, 0.00f},  // spring
      {0.10f, 0.45f, 0.00f},  // summer
      {0.06f, 0.35f, 0.18f},  // autumn
      {0.00f, 0.20f, 0.40f},  // winter
  };
  const f32 year = (static_cast<f32>(sim::day_of_year(w.now)) +
                    static_cast<f32>(sim::tick_of_day(w.now)) / static_cast<f32>(kTicksPerDay)) /
                   static_cast<f32>(kDaysPerSeason);  // 0..4, season i is centred on i + 0.5
  const f32 sp = std::fmod(year - 0.5f + 4.0f, 4.0f);
  const int s0 = static_cast<int>(sp) % 4, s1 = (s0 + 1) % 4;
  const f32 st = sp - std::floor(sp);
  for (int i = 0; i < 3; ++i) ground[i] = lerp(kSeasonGround[s0][i], kSeasonGround[s1][i], st);
  if (weather == sim::Weather::Heat) ground[0] = std::min(1.0f, ground[0] + 0.15f);
  const bool snowing = weather == sim::Weather::Snow;
  if (!primed) {
    snow = snowing ? 0.8f : 0.0f;
  } else if (w.now > last_tick) {
    const f32 dt_ticks = static_cast<f32>(std::min<Tick>(w.now - last_tick, kTicksPerDay));
    if (snowing) snow = std::min(1.0f, snow + dt_ticks / 400.0f);
    else snow = std::max(0.0f, snow - dt_ticks / (sim::season(w.now) == sim::Season::Winter ? 1200.0f : 300.0f));
  }
  last_tick = w.now;
  ground[3] = snow;
  primed = true;
}

}  // namespace urbs::game
