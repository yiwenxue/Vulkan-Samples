#version 320 es

precision highp float;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;

layout (set=1, binding=1) uniform sampler2D u_texture;

layout(location = 0) out vec4 o_color;

void main(void)
{
    o_color = texture(u_texture, uv);
}
