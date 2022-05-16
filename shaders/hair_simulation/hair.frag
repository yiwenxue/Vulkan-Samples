#version 320 es

precision highp float;

layout (location = 0) in vec4 pos;

layout(location = 0) out vec4 o_color;

void main(void)
{
    o_color = vec4(vec3(1.0)  * (1.0 - pos.w), 1.0 );
}
