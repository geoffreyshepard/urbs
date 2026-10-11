#version 450
// Ground shading for the static chunk layer. The ground texture has one texel per tile and is
// sampled with linear filtering at the centre of each art pixel, then quantised with a 4x4 Bayer
// pattern in world-pixel space, so the smooth fields below stay crisp, pixel-sized steps.
//   r: wear (bare earth around buildings and along roads)
//   g: luminance (128 = 1.0): hill height plus low-frequency variation
//   b: vegetation (0 = bare ground; 128 = dry grass .. 255 = lush)
//   a: land mask (255 land, 0 water); filtered, it rises to 0.5 at the coastline
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec2 v_world;
layout(location = 3) flat in float v_depth;
layout(location = 4) flat in float v_layer;
layout(set = 2, binding = 0) uniform sampler2DArray tex;
layout(set = 2, binding = 1) uniform sampler2D ground;
layout(set = 3, binding = 0) uniform Ground {
  vec4 season;  // x: overall dryness, y: dry-patch strength, z: winter browning, w: snow cover
  vec4 map;     // xy: 1 / map size in world px, z: time (s), w: terrain depth threshold
} u;
layout(location = 0) out vec4 o_color;

const vec3 kLuma = vec3(0.299, 0.587, 0.114);

float bayer4(vec2 p) {
  ivec2 i = ivec2(mod(p, 4.0));
  int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
  return (float(m[i.y * 4 + i.x]) + 0.5) / 16.0;
}

// Quantise v (0..1) into n steps, dithered.
float steps(float v, float n, float d) { return clamp(floor(v * n + d) / n, 0.0, 1.0); }

void main() {
  vec4 c = texture(tex, vec3(v_uv, v_layer)) * v_color;
  if (c.a < 0.01) discard;
  vec2 cell = floor(v_world);
  vec4 g = texture(ground, (cell + 0.5) * u.map.xy);
  float d = bayer4(cell);
  bool terrain = v_depth > u.map.w;

  // Luminance: hills and broad light/dark patches, in 3% steps.
  float lum = g.g * (255.0 / 128.0);
  lum = floor(lum / 0.03 + d) * 0.03;
  c.rgb *= lum;

  float veg = smoothstep(0.30, 0.50, g.b);
  float lush = clamp((g.b - 0.5) * 2.0, 0.0, 1.0);
  float l = dot(c.rgb, kLuma);
  if (terrain) {
    // Dry, straw-coloured patches (stronger in summer) and winter browning, on vegetation only.
    float dry = veg * clamp(u.season.x + (1.0 - lush) * u.season.y, 0.0, 1.0);
    dry = steps(dry, 4.0, d) * 0.5;
    c.rgb = mix(c.rgb, vec3(l) * vec3(1.20, 1.04, 0.62), dry);
    float brown = steps(veg * u.season.z, 3.0, d) * 0.5;
    c.rgb = mix(c.rgb, vec3(l) * vec3(1.08, 0.98, 0.80), brown);

    // Worn earth around buildings and along roads.
    float wear = steps(g.r * (0.75 + 0.5 * veg), 3.0, d);
    c.rgb = mix(c.rgb, vec3(0.50, 0.40, 0.28) * (0.75 + 0.6 * l), wear * 0.62);

    // Water: lighter, greener shallows by the shore and a broken foam line that drifts.
    bool water = c.b > c.r + 0.06 && c.b > c.g;
    if (water && g.a > 0.02) {
      float near = g.a;  // 0.5 on the coastline
      c.rgb = mix(c.rgb, c.rgb * vec3(1.04, 1.16, 1.10), steps(near * 2.0, 3.0, d) * 0.45);
      float wave = sin(near * 38.0 - u.map.z * 1.6 + cell.x * 0.13 + cell.y * 0.11);
      float foam = step(0.38, near) * step(-0.2, wave + d * 0.6);  // ~2 px against the shore
      foam = max(foam, step(0.20, near) * step(near, 0.25) * step(0.75, wave));  // breaking swell
      c.rgb = mix(c.rgb, vec3(0.84, 0.91, 0.90), foam * 0.7);
    }
  }
  // Snow settles on everything in this layer except water.
  float snow = u.season.w * (terrain ? 1.0 : 0.6);
  bool wet = c.b > c.r + 0.06 && c.b > c.g;
  if (snow > 0.0 && !wet) {
    float s = steps(snow * (0.7 + 0.3 * l), 4.0, d);
    c.rgb = mix(c.rgb, vec3(0.90, 0.93, 0.98) * (0.85 + 0.2 * l), s * 0.85);
  }
  o_color = c;
}
