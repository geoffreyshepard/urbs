#version 450
// Cast shadows: every y-sorted sprite (buildings, trees, people) is drawn again as its silhouette,
// sheared away from the light. Rows higher above the sprite's foot move further, so tall things
// cast long shadows and the foot stays put. Ground-level instances (terrain, roads, painted tiles:
// depth >= kGroundZ) and screen overlays (depth <= kOverlayZ) are collapsed to nothing here, so the
// same instance buffers can be reused without a CPU-side caster list.
layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec2 i_size;
layout(location = 2) in uvec4 i_rect;
layout(location = 3) in vec4 i_color;

layout(set = 1, binding = 0) uniform Camera {
  vec2 origin;
  vec2 scale;
  vec2 screen;
  vec2 tex_inv;
} cam;
layout(set = 1, binding = 1) uniform Shadow {
  vec4 shear;  // x, y: offset per px of height above the foot; z, w: constant offset
  vec4 depth;  // x: depth written for shadows, y: ground threshold, z: overlay threshold
} sh;

layout(location = 0) out vec2 v_uv;

void main() {
  int vid = gl_VertexIndex;
  vec2 corner = vec2((vid == 1 || vid == 2 || vid == 4) ? 1.0 : 0.0, (vid == 2 || vid == 4 || vid == 5) ? 1.0 : 0.0);
  v_uv = (vec2(i_rect.xy) + corner * vec2(i_rect.zw)) * cam.tex_inv;
  bool caster = i_pos.z < sh.depth.y && i_pos.z > sh.depth.z && i_color.a > 0.99;
  if (!caster) {
    gl_Position = vec4(0.0, 0.0, -2.0, 1.0);  // degenerate: every vertex at the same point
    return;
  }
  vec2 world = i_pos.xy + corner * i_size;
  float above = (i_pos.y + i_size.y) - world.y;  // 0 on the bottom edge
  world += sh.shear.xy * above + sh.shear.zw;
  vec2 px = (world - cam.origin) * cam.scale;
  vec2 ndc = px / cam.screen * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, sh.depth.x, 1.0);
}
