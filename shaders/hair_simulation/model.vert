#version 320 es

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 texCoord;

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
    vec4 view_dir;
} global_uniform;

layout(set = 1, binding = 0) uniform LocalUniform {
    mat4 model;
} local_uniform;

layout (location = 0) out vec4 o_pos;
layout (location = 1) out vec4 o_normal;
layout (location = 2) out vec4 o_color;
layout (location = 3) out vec2 o_uv;

void main(void)
{
    o_uv = texCoord;

    o_normal = global_uniform.view * local_uniform.model * vec4(normal, 1.0);

    o_pos = vec4(position, 1.0);

    o_color = vec4(color, 1.0);

    gl_Position = global_uniform.projection * global_uniform.view * local_uniform.model * vec4(position, 1.0);
}
