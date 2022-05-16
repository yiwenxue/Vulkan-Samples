#version 320 es

layout(location = 0) in vec4 position;

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
    vec4 view_dir;
} global_uniform;

layout(set = 1, binding = 0) uniform LocalUniform {
    mat4 model;
} local_uniform;

layout (location = 0) out vec4 o_pos;

void main(void)
{
    o_pos = position;

    gl_Position = global_uniform.projection * global_uniform.view *vec4(position.xyz, 1.0);
}
