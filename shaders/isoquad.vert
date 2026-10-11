#version 450
// One map-sized texture (one texel per tile) drawn as the iso diamond of the whole map: the far
// zoom terrain.  No vertex buffer; 6 vertices from gl_VertexIndex.
layout(set = 1, binding = 0) uniform Quad {
  vec2 origin;  // iso px at screen top-left
  vec2 scale;   // screen px per iso px
  vec2 screen;  // target size in px
  vec2 map;     // map size in tiles
  vec4 iso;     // x: half tile width, y: half tile height (iso px), z: depth
} q;

layout(location = 0) out vec2 v_uv;

void main() {
  int vid = gl_VertexIndex;
  vec2 corner = vec2((vid == 1 || vid == 2 || vid == 4) ? 1.0 : 0.0, (vid == 2 || vid == 4 || vid == 5) ? 1.0 : 0.0);
  vec2 tile = corner * q.map;
  vec2 world = vec2((tile.x - tile.y) * q.iso.x, (tile.x + tile.y) * q.iso.y);
  vec2 px = (world - q.origin) * q.scale;
  vec2 ndc = px / q.screen * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, q.iso.z, 1.0);
  v_uv = corner;
}
