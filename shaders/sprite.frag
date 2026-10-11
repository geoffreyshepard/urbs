#version 450
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) flat in float v_layer;
layout(set = 2, binding = 0) uniform sampler2DArray tex;
layout(location = 0) out vec4 o_color;

void main() {
  vec4 c = texture(tex, vec3(v_uv, v_layer)) * v_color;
  if (c.a < 0.01) discard;
  o_color = c;
}
