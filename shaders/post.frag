#version 450
// Colour grade for the world layer (the UI is drawn afterwards, ungraded).
// Split toning: dark tones lean toward shadow_tint, bright tones toward light_tint, so a single
// grade gives cool shadows and warm light instead of a flat multiply. Time of day, season and
// weather only change these parameters (see game/lighting.cpp).
layout(location = 0) in vec2 v_uv;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 3, binding = 0) uniform Grade {
  vec4 shadow_tint;  // rgb; a = exposure
  vec4 light_tint;   // rgb; a = saturation
  vec4 lift;         // rgb added to blacks; a = vignette strength
  vec4 params;       // x = contrast, y = time (s), z = night (0..1), w = unused
} g;
layout(location = 0) out vec4 o_color;

const vec3 kLuma = vec3(0.299, 0.587, 0.114);

void main() {
  vec3 c = texture(scene, v_uv).rgb;
  float l = dot(c, kLuma);
  vec3 tint = mix(g.shadow_tint.rgb, g.light_tint.rgb, smoothstep(0.08, 0.85, l));
  c *= tint * g.shadow_tint.a;
  float l2 = dot(c, kLuma);
  c = mix(vec3(l2), c, g.light_tint.a);
  c = (c - 0.5) * g.params.x + 0.5;
  c = g.lift.rgb + c * (1.0 - g.lift.rgb);
  vec2 d = v_uv - 0.5;
  c *= 1.0 - g.lift.a * dot(d, d) * 2.0;
  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
