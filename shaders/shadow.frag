#version 450
layout(location = 0) in vec2 v_uv;
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform ShadowColor {
  vec4 color;  // rgb multiplies the ground (blend: dst * src)
} sc;
layout(location = 0) out vec4 o_color;

void main() {
  // Alpha 254 marks ground-level pixels (yards, paving plates): they cast nothing.
  if (texture(tex, v_uv).a < 0.999) discard;
  o_color = sc.color;
}
