#version 450
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform Light {
  vec4 tint;  // global light (day/night, season); rgb multiplies, a unused
} light;
layout(location = 0) out vec4 o_color;

void main() {
  vec4 c = texture(tex, v_uv) * v_color;
  if (c.a < 0.01) discard;
  o_color = vec4(c.rgb * light.tint.rgb, c.a);
}
