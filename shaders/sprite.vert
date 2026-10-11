#version 450
// Instanced sprite quad. One instance = one sprite; 6 vertices generated from gl_VertexIndex.
layout(location = 0) in vec3 i_pos;    // iso px of top-left, depth
layout(location = 1) in vec2 i_size;   // iso px size
layout(location = 2) in uvec4 i_rect;  // atlas u, v, w, h (texels; v runs through the stacked pages)
layout(location = 3) in vec4 i_color;  // tint

layout(set = 1, binding = 0) uniform Camera {
  vec2 origin;     // iso px at screen top-left
  vec2 scale;      // screen px per iso px
  vec2 screen;     // target size in px
  vec2 tex_inv;    // 1 / page size
  vec4 page;       // x: rows per atlas page
} cam;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) flat out float v_layer;

void main() {
  int vid = gl_VertexIndex;
  vec2 corner = vec2((vid == 1 || vid == 2 || vid == 4) ? 1.0 : 0.0, (vid == 2 || vid == 4 || vid == 5) ? 1.0 : 0.0);
  vec2 world = i_pos.xy + corner * i_size;
  vec2 px = (world - cam.origin) * cam.scale;
  vec2 ndc = px / cam.screen * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, i_pos.z, 1.0);
  uint rows = uint(cam.page.x);
  v_layer = float(i_rect.y / rows);
  vec2 texel = vec2(float(i_rect.x), float(i_rect.y % rows));
  v_uv = (texel + corner * vec2(i_rect.zw)) * cam.tex_inv;
  v_color = i_color;
}
