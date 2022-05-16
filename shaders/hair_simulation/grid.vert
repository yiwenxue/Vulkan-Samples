#version 320 es

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 texCoord;

// Instanced attributes
layout(location = 4) in vec4 gridPos;
layout(location = 5) in vec4 gridScale;

// layout (location 4) in int idx;

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
    vec4 view_dir;
} global_uniform;

layout(set = 1, binding = 0) uniform LocalUniform {
    mat4 model;
} local_uniform;

layout (location = 0) out vec4 o_pos;

layout (location = 1) out vec4 o_color;

void main(void)
{

    vec4 pos = local_uniform.model * vec4(position * 0.5, 1.0) + gridPos;

    vec4 rgb;

    if (distance(gridScale.xyz, vec3(0.0)) < 1.0) {
        rgb = vec4(0.0);
    } else {
        rgb = vec4(normalize(gridScale.xyz), 0.5);
    }

    o_color = rgb;

    o_pos = pos;

    gl_Position = global_uniform.projection * global_uniform.view * pos;
}
