#version 450
// Static chunk layer (terrain, roads, cliffs, decorations): sprite.vert plus each vertex's position
// on the top-down sim grid, which ground.frag needs to sample the per-tile ground texture.  The
// instance colour's alpha byte carries a code from build_chunk (render/terrain.hpp):
// alpha = 255 - (1 + kind * 8 + level); kind 0 terrain, 1 road, 2 cliff, 3 decoration.
layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec2 i_size;
layout(location = 2) in uvec4 i_rect;
layout(location = 3) in vec4 i_color;

layout(set = 1, binding = 0) uniform Camera {
  vec2 origin;
  vec2 scale;
  vec2 screen;
  vec2 tex_inv;
  vec4 page;  // x: rows per atlas page
} cam;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out vec2 v_world;
layout(location = 3) flat out float v_depth;
layout(location = 4) flat out float v_layer;
layout(location = 5) flat out float v_kind;

const float kTileW = 128.0, kTileH = 64.0, kLevelPx = 16.0, kTilePx = 16.0;  // render/iso.hpp

void main() {
  int vid = gl_VertexIndex;
  vec2 corner = vec2((vid == 1 || vid == 2 || vid == 4) ? 1.0 : 0.0, (vid == 2 || vid == 4 || vid == 5) ? 1.0 : 0.0);
  vec2 iso = i_pos.xy + corner * i_size;
  vec2 px = (iso - cam.origin) * cam.scale;
  vec2 ndc = px / cam.screen * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, i_pos.z, 1.0);
  uint rows = uint(cam.page.x);
  v_layer = float(i_rect.y / rows);
  v_uv = (vec2(float(i_rect.x), float(i_rect.y % rows)) + corner * vec2(i_rect.zw)) * cam.tex_inv;

  int code = int(round((1.0 - i_color.a) * 255.0));
  float kind = code > 0 ? float((code - 1) / 8) : 3.0;
  float level = code > 0 ? float((code - 1) % 8) : 0.0;
  // iso::to_tile on the instance's level, then sim world px.
  float a = iso.x / (kTileW * 0.5);
  float b = (iso.y + level * kLevelPx) / (kTileH * 0.5);
  v_world = vec2(a + b, b - a) * 0.5 * kTilePx;
  v_color = vec4(i_color.rgb, 1.0);
  v_kind = kind;
  v_depth = i_pos.z;
}
