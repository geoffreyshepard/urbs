#version 450
// Colour grade for the world layer (the UI is drawn afterwards, ungraded).
// Split toning: dark tones lean toward shadow_tint, bright tones toward light_tint, so a single
// grade gives cool shadows and warm light instead of a flat multiply. Then vibrance (saturation that
// spares already-saturated colours), an S-curve contrast that keeps black and white in range, the
// lift, and a soft vignette that leaves the middle of the screen alone. Time of day, season and
// weather only change these parameters (see game/lighting.cpp).
layout(location = 0) in vec2 v_uv;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 3, binding = 0) uniform Grade {
  vec4 shadow_tint;  // rgb; a = exposure
  vec4 light_tint;   // rgb; a = saturation (1 = unchanged; above 1 = vibrance)
  vec4 lift;         // rgb added to blacks; a = vignette strength
  vec4 params;       // x = contrast (S-curve slope at mid grey), y = time (s), z = night (0..1), w = unused
} g;
layout(location = 0) out vec4 o_color;

const vec3 kLuma = vec3(0.299, 0.587, 0.114);

void main() {
  vec3 c = texture(scene, v_uv).rgb;
  float l = dot(c, kLuma);
  vec3 tint = mix(g.shadow_tint.rgb, g.light_tint.rgb, smoothstep(0.08, 0.85, l));
  c *= tint * g.shadow_tint.a;
  // Saturation below 1 desaturates evenly; above 1 it acts as vibrance, so terracotta roofs and
  // awnings that are already strong do not run to neon while grass, plaster and crowds gain colour.
  float l2 = dot(c, kLuma);
  float sat = max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
  float s = g.light_tint.a;
  float k = s <= 1.0 ? s : 1.0 + (s - 1.0) * (1.0 - smoothstep(0.15, 0.6, sat));
  c = mix(vec3(l2), c, k);
  // S-curve contrast: slope params.x at mid grey, 0 and 1 stay fixed (no clipped highlights).
  c = clamp(c, 0.0, 1.0);
  c += (g.params.x - 1.0) * (c - 0.5) * c * (1.0 - c) * 4.0;
  c = g.lift.rgb + c * (1.0 - g.lift.rgb);
  vec2 d = (v_uv - 0.5) * vec2(1.0, 0.8);
  c *= 1.0 - g.lift.a * smoothstep(0.12, 0.42, dot(d, d) * 1.6);
  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
