#version 450
// Ground shading for the static chunk layer.  The ground texture has one texel per tile, sampled
// with linear filtering at each fragment's position on the sim grid (ground.vert):
//   r: wear (bare earth around buildings and along roads)
//   g: luminance (128 = 1.0): hill height plus low-frequency variation
//   b: vegetation (0 = bare ground; 128 = dry grass .. 255 = lush)
//   a: land mask (255 land, 0 water); filtered, it rises to 0.5 at the coastline
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec2 v_world;
layout(location = 3) flat in float v_depth;
layout(location = 4) flat in float v_layer;
layout(location = 5) flat in float v_kind;
layout(set = 2, binding = 0) uniform sampler2DArray tex;
layout(set = 2, binding = 1) uniform sampler2D ground;
layout(set = 3, binding = 0) uniform Ground {
  vec4 season;  // x: overall dryness, y: dry-patch strength, z: winter browning, w: snow cover
  vec4 map;     // xy: 1 / map size in world px, z: time (s), w: unused
} u;
layout(location = 0) out vec4 o_color;

const vec3 kLuma = vec3(0.299, 0.587, 0.114);

void main() {
  vec4 c = texture(tex, vec3(v_uv, v_layer)) * v_color;
  if (c.a < 0.01) discard;
  vec4 g = texture(ground, v_world * u.map.xy);
  bool terrain = v_kind < 0.5;
  bool decor = v_kind > 2.5;

  // Luminance: hills and broad light/dark patches (softened: the art carries its own shading).
  float lum = mix(1.0, g.g * (255.0 / 128.0), decor ? 0.5 : 0.8);
  c.rgb *= lum;

  float veg = smoothstep(0.30, 0.50, g.b);
  float lush = clamp((g.b - 0.5) * 2.0, 0.0, 1.0);
  float l = dot(c.rgb, kLuma);
  bool water = c.b > c.r + 0.06 && c.b > c.g;
  if (terrain && !water) {
    // Dry, straw-coloured patches (stronger in summer) and winter browning, on vegetation only.
    float dry = veg * clamp(u.season.x + (1.0 - lush) * u.season.y, 0.0, 1.0) * 0.45;
    c.rgb = mix(c.rgb, vec3(l) * vec3(1.20, 1.04, 0.62), dry);
    float brown = veg * u.season.z * 0.5;
    c.rgb = mix(c.rgb, vec3(l) * vec3(1.08, 0.98, 0.80), brown);
    // Worn earth around buildings and along roads.
    float wear = smoothstep(0.1, 0.9, g.r * (0.75 + 0.5 * veg));
    c.rgb = mix(c.rgb, vec3(0.55, 0.45, 0.32) * (0.8 + 0.5 * l), wear * 0.38);
  }
  if (terrain && water && g.a > 0.02) {
    // Lighter, greener shallows by the shore and a broken foam line that drifts.
    float near = g.a;  // 0.5 on the coastline
    c.rgb = mix(c.rgb, c.rgb * vec3(1.04, 1.16, 1.10), smoothstep(0.1, 0.5, near) * 0.45);
    float wave = sin(near * 38.0 - u.map.z * 1.6 + v_world.x * 0.13 + v_world.y * 0.11);
    float foam = smoothstep(0.36, 0.42, near) * smoothstep(-0.3, 0.1, wave);
    foam = max(foam, smoothstep(0.18, 0.2, near) * (1.0 - smoothstep(0.24, 0.26, near)) * smoothstep(0.7, 0.8, wave));
    c.rgb = mix(c.rgb, vec3(0.86, 0.92, 0.91), foam * 0.6);
  }
  // Snow settles on everything in this layer except water and cliff faces.
  float snow = u.season.w * (terrain || v_kind < 1.5 ? 1.0 : (decor ? 0.6 : 0.3));
  if (snow > 0.0 && !water) {
    float s = clamp(snow * (0.7 + 0.3 * l), 0.0, 1.0);
    c.rgb = mix(c.rgb, vec3(0.90, 0.93, 0.98) * (0.85 + 0.2 * l), s * 0.85);
  }
  o_color = c;
}
