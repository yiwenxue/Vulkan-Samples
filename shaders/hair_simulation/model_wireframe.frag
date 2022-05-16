#version 320 es

precision highp float;

layout (location = 0) in vec4 o_pos;
layout (location = 1) in vec4 o_normal;
layout (location = 2) in vec4 o_color;
layout (location = 3) in vec2 o_uv;

layout(location = 0) out vec4 o_color;

void main(void)
{
    o_color = vec4(1.0, 1.0, 1.0, 1.0);
}
