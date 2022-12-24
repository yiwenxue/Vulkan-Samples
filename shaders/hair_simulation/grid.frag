#version 450

precision highp float;

layout (location = 0) in vec4 pos;

layout (location = 1) in vec4 i_color;

layout (location = 0) out vec4 o_color;

void main(void)
{
    o_color = i_color;
}
