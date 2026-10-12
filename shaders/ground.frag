#version 450
// Ground shading for the static chunk layer.  The ground texture has one texel per tile corner
// (texel (x, y) = the NW corner of tile (x, y), averaged over the four tiles there), sampled with
// linear filtering at each fragment's position on the sim grid (ground.vert), half a texel up-left
// so it interpolates between corners (ground_at):
//   r: wear (bare earth around buildings and along roads)
//   g: luminance (128 = 1.0): hill height plus low-frequency variation
//   b: vegetation (0 = bare ground; 128 = dry grass .. 255 = lush)
//   a: land at the corner (1 = all four tiles land, else count / 6): about 1/3 on a straight
//      coastline, 0 on open water
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec2 v_world;
layout(location = 3) flat in float v_depth;
layout(location = 4) flat in float v_layer;
layout(location = 5) flat in float v_kind;
layout(location = 6) flat in float v_level;
layout(set = 2, binding = 0) uniform sampler2DArray tex;
layout(set = 2, binding = 1) uniform sampler2D ground;
layout(set = 3, binding = 0) uniform Ground {
  vec4 season;  // x: overall dryness, y: dry-patch strength, z: winter browning, w: snow cover
  vec4 map;     // xy: 1 / map size in world px, z: time (s), w: unused
} u;
layout(location = 0) out vec4 o_color;

const vec3 kLuma = vec3(0.299, 0.587, 0.114);
const vec3 kShallow = vec3(0.38, 0.69, 0.75);  // shallow water over sand (terrain_tex.py "shallow", lighter)
const vec3 kSwash = vec3(0.60, 0.80, 0.80);    // the thin sheet of a spent wave
const vec3 kFoam = vec3(0.88, 0.93, 0.92);

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

// Beach sand at tile coordinates t, shared by the beach pieces and the sand fringe on the land
// beside them so the two meet without a seam: warm sand in broad patches, fine grain, pebbles.
vec3 sand_color(vec2 t) {
  float n1 = vnoise(t * 0.55 + vec2(5.3, 1.7));
  float n2 = vnoise(t * 2.3 + vec2(11.0, 3.0));
  float n3 = vnoise(t * 9.0 + vec2(2.0, 19.0));
  float grain = hash12(floor(t * 40.0));
  vec3 c = mix(vec3(0.85, 0.76, 0.56), vec3(0.77, 0.66, 0.46), smoothstep(0.25, 0.75, n1 * 0.6 + n2 * 0.4));
  float ripple = 0.5 + 0.5 * sin(t.x * 13.0 + t.y * 5.0 + n2 * 6.0);
  c *= 0.96 + 0.04 * ripple + 0.09 * (n3 - 0.5) + 0.06 * (grain - 0.5);
  // A few pebbles of mixed size and colour.
  vec2 q = t * 9.0, cell = floor(q);
  vec2 ctr = vec2(hash12(cell + 7.1), hash12(cell + 3.7)) * 0.6 + 0.2;
  float r = 0.08 + 0.13 * hash12(cell + 5.9);
  float peb = step(0.87, hash12(cell)) * (1.0 - smoothstep(r * 0.6, r, length(fract(q) - ctr)));
  vec3 pc = mix(vec3(0.50, 0.48, 0.44), vec3(0.72, 0.66, 0.57), hash12(cell + 1.3));
  return mix(c, pc, peb * 0.65);
}

// Scree at the foot of rock cliffs: fallen stones in the rock terrain's greys, lit from the upper left.
vec3 scree_color(vec2 t) {
  // Gravel between the stones.
  float g1 = vnoise(t * 2.0 + vec2(3.0, 7.0)), g2 = vnoise(t * 24.0 + vec2(1.0, 5.0));
  vec3 c = mix(vec3(0.60, 0.56, 0.48), vec3(0.50, 0.47, 0.41), g1) * (0.9 + 0.2 * g2);
  // Stones of mixed size, each in its cell of a jittered grid (some cells hold none).
  vec2 q = t * 8.0, cell = floor(q), f = fract(q);
  for (int j = -1; j <= 1; ++j)
    for (int i = -1; i <= 1; ++i) {
      vec2 id = cell + vec2(i, j);
      float hs = hash12(id);
      if (hs < 0.3) continue;
      vec2 off = f - (vec2(i, j) + vec2(hash12(id + 7.1), hash12(id + 3.7)) * 0.6 + 0.2);
      off.x *= 0.8 + 0.4 * hash12(id + 2.2);
      float r = 0.22 + 0.3 * hash12(id + 5.9);
      float d = length(off) / r;
      if (d > 1.15) continue;
      vec3 sc = mix(vec3(0.66, 0.63, 0.56), vec3(0.45, 0.43, 0.39), hash12(id + 1.3));
      sc = mix(sc, vec3(0.55, 0.57, 0.40), step(0.92, hash12(id + 9.2)) * 0.5);  // a mossy one
      sc *= 1.0 + 0.5 * dot(off / r, vec2(-0.55, -0.75)) * (1.0 - d * 0.5);     // rounded, lit from the upper left
      c = mix(c * (1.0 - 0.35 * smoothstep(1.15, 0.95, d)), sc, smoothstep(1.0, 0.9, d));  // contact shadow, stone
    }
  return c;
}

vec4 ground_at(vec2 world) { return texture(ground, (world + 8.0) * u.map.xy); }

void main() {
  int kind = int(v_kind + 0.5);
  if (kind == 5) {
    // Beach piece (terrain_tex.py shores()): r slope shading / 1.5, g height (h + 0.6) / 1.6 in
    // levels above the water, b the marching-squares field, a coverage.  ground.vert unprojects on
    // the water's level; a point h levels up sits h / 4 tiles further south-east.
    vec4 d = texture(tex, vec3(v_uv, v_layer));
    if (d.a < 0.02) discard;
    float h = d.g * 1.6 - 0.6;
    if (h < -0.44) discard;  // deep enough that only the water tile beneath shows
    vec2 wp = v_world + max(h, 0.0) * 4.0;
    vec2 t = wp * (1.0 / 16.0);
    vec4 g = ground_at(wp);
    float lum = mix(1.0, g.g * (255.0 / 128.0), 0.8);
    float time = u.map.z;
    // Corners that touch high ground (instance colour, red byte / 16) turn the sand into scree.
    int rb = int(v_color.r * 255.0 + 0.5) >> 4;
    float rock = 0.0;
    if (rb != 0) {
      vec2 f = fract(t);
      vec4 k = vec4(rb & 1, (rb >> 1) & 1, (rb >> 2) & 1, (rb >> 3) & 1);
      rock = mix(mix(k.x, k.y, f.x), mix(k.z, k.w, f.x), f.y);
      rock = smoothstep(0.3, 0.7, rock + (vnoise(t * 3.7 + vec2(4.0, 8.0)) - 0.5) * 0.35);
    }
    vec3 sand = rock < 1.0 ? sand_color(t) : vec3(0.0);
    if (rock > 0.0) sand = mix(sand, scree_color(t), rock);
    sand *= mix(1.0, d.r * 1.5, 0.75);  // slope light, a little softer than baked
    float a = 1.0;
    vec3 c;
    // The swash: a thin sheet of water runs up the sand and drains back, out of step along the coast.
    float ph = time * 0.75 + 2.0 * (sin(t.x * 0.7) + sin(t.y * 0.9 + 1.3)) + (t.x + t.y) * 0.35;
    float reach = 0.035 + 0.07 * (0.5 + 0.5 * sin(ph));
    if (h > 0.0) {
      float wet = 1.0 - smoothstep(0.0, 0.2, h);
      c = mix(sand, sand * vec3(0.76, 0.75, 0.76), wet);
      float film = 1.0 - smoothstep(reach - 0.012, reach, h);
      c = mix(c, kSwash, film * 0.45);
      float edge = film * smoothstep(reach - 0.04, reach - 0.008, h);
      c = mix(c, kFoam, edge * 0.7);
    } else {
      // Under the water: clear turquoise over the sand, fading into the water tile beneath, and a
      // soft band of foam along the waterline.
      float depth = -h;
      c = mix(sand * vec3(0.80, 0.92, 0.92), kShallow, smoothstep(0.0, 0.3, depth));
      a = 1.0 - smoothstep(0.1, 0.45, depth);
      float foam = (1.0 - smoothstep(0.0, 0.045, depth)) * (0.75 + 0.25 * sin(ph * 1.7 + t.x * 9.0));
      c = mix(c, kFoam, foam * 0.6);
    }
    c *= lum;
    if (u.season.w > 0.0 && h > 0.05) {
      float s = clamp(u.season.w * (0.7 + 0.3 * dot(c, kLuma)), 0.0, 1.0);
      c = mix(c, vec3(0.90, 0.93, 0.98) * (0.85 + 0.2 * dot(c, kLuma)), s * 0.85);
    }
    o_color = vec4(c, a * d.a);
    return;
  }

  vec4 c = texture(tex, vec3(v_uv, v_layer)) * v_color;
  if (kind == 0 || kind == 2) {  // terrain bases and cliff faces: alpha-tested, see render/terrain.hpp
    if (c.a < 0.5) discard;
    c.a = 1.0;
  }
  if (c.a < 0.01) discard;
  vec4 g = ground_at(v_world);
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
    // Sand fringe on low land by the water: it continues the beach pieces' sand, so the beach's top
    // follows a smooth line across the tiles instead of their stepped edges (render/terrain.hpp).
    if (v_level < 1.5) {
      float near = g.a;
      if (near < 0.8) {
        vec2 t = v_world * (1.0 / 16.0);
        float f = 1.0 - smoothstep(0.52, 0.74, near + (vnoise(t * 3.1 + vec2(9.0, 4.0)) - 0.5) * 0.1);
        c.rgb = mix(c.rgb, sand_color(t) * lum, f);
      }
    }
  }
  if (terrain && water) {
    // Lighter, greener shallows by every shore, and surf breaking at the foot of rocky banks (beaches
    // carry their own foam).
    float near = g.a;
    if (near > 0.005) {
      c.rgb = mix(c.rgb, c.rgb * vec3(1.06, 1.12, 1.08), smoothstep(0.0, 0.3, near) * 0.45);
      float wave = sin(near * 60.0 - u.map.z * 1.6 + v_world.x * 0.13 + v_world.y * 0.11);
      float foam = smoothstep(0.25, 0.31, near) * smoothstep(-0.3, 0.1, wave);
      c.rgb = mix(c.rgb, kFoam, foam * 0.6);
    }
  }
  // Snow settles on everything in this layer except water and cliff faces.
  float snow = u.season.w * (terrain || kind == 1 ? 1.0 : (decor ? 0.6 : 0.3));
  if (snow > 0.0 && !water) {
    float s = clamp(snow * (0.7 + 0.3 * l), 0.0, 1.0);
    c.rgb = mix(c.rgb, vec3(0.90, 0.93, 0.98) * (0.85 + 0.2 * l), s * 0.85);
  }
  o_color = c;
}
