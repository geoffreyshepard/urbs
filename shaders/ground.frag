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

// Value noise over the sim grid (tiles), for the meadow variation below.
float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash12(i), b = hash12(i + vec2(1, 0)), c = hash12(i + vec2(0, 1)), d = hash12(i + vec2(1, 1));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
  vec4 c = texture(tex, vec3(v_uv, v_layer)) * v_color;
  int kind = int(v_kind + 0.5);
  if (kind == 0 || kind == 2) {  // terrain bases and cliff faces: alpha-tested, see render/terrain.hpp
    if (c.a < 0.5) discard;
    c.a = 1.0;
  }
  if (c.a < 0.01) discard;
  vec4 g = texture(ground, v_world * u.map.xy);
  bool terrain = kind == 0 || kind == 4;
  bool decor = kind == 3;

  // Luminance: hills and broad light/dark patches (softened: the art carries its own shading).
  float lum = mix(1.0, g.g * (255.0 / 128.0), decor ? 0.5 : 0.8);
  c.rgb *= lum;

  float veg = smoothstep(0.30, 0.50, g.b);
  float lush = clamp((g.b - 0.5) * 2.0, 0.0, 1.0);
  float l = dot(c.rgb, kLuma);
  bool water = c.b > c.r + 0.06 && c.b > c.g;
  if (terrain && !water && veg > 0.0) {
    // Meadow variation in world space (the grass art repeats every two tiles): broad lush and pale
    // swathes a few tiles across, sun-dried patches, and darker clover clumps.
    vec2 t = v_world * (1.0 / 16.0);
    float n1 = vnoise(t * 0.17 + vec2(3.1, 7.7));
    float n2 = vnoise(t * 0.55 + vec2(17.0, 5.0));
    float n3 = vnoise(t * 2.1 + vec2(41.0, 13.0));
    float lv = dot(c.rgb, kLuma);
    c.rgb *= mix(0.84, 1.13, n1 * 0.75 + n2 * 0.25) * veg + (1.0 - veg);
    c.rgb = mix(c.rgb, c.rgb * vec3(1.10, 1.05, 0.80), smoothstep(0.55, 0.9, n1) * 0.55 * veg);
    c.rgb = mix(c.rgb, c.rgb * vec3(0.90, 1.00, 1.02), smoothstep(0.45, 0.1, n1) * 0.5 * veg);
    float dryp = smoothstep(0.64, 0.80, n2 * 0.65 + n3 * 0.35);
    c.rgb = mix(c.rgb, vec3(lv) * vec3(1.32, 1.14, 0.72), dryp * 0.45 * veg);
    c.rgb *= 1.0 - smoothstep(0.72, 0.92, n3) * (1.0 - dryp) * 0.10 * veg;
  }
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
  float snow = u.season.w * (terrain || kind == 1 ? 1.0 : (decor ? 0.6 : 0.3));
  if (snow > 0.0 && !water) {
    float s = clamp(snow * (0.7 + 0.3 * l), 0.0, 1.0);
    c.rgb = mix(c.rgb, vec3(0.90, 0.93, 0.98) * (0.85 + 0.2 * l), s * 0.85);
  }
  o_color = c;
}
