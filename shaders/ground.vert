#version 450
// Static chunk layer (terrain, roads, decorations): sprite.vert plus the world position and depth,
// which ground.frag needs to sample the per-tile ground texture.
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

void main() {
  int vid = gl_VertexIndex;
  vec2 corner = vec2((vid == 1 || vid == 2 || vid == 4) ? 1.0 : 0.0, (vid == 2 || vid == 4 || vid == 5) ? 1.0 : 0.0);
  vec2 world = i_pos.xy + corner * i_size;
  vec2 px = (world - cam.origin) * cam.scale;
  vec2 ndc = px / cam.screen * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, i_pos.z, 1.0);
  uint rows = uint(cam.page.x);
  v_layer = float(i_rect.y / rows);
  v_uv = (vec2(float(i_rect.x), float(i_rect.y % rows)) + corner * vec2(i_rect.zw)) * cam.tex_inv;
  v_color = i_color;
  v_world = world;
  v_depth = i_pos.z;
}
