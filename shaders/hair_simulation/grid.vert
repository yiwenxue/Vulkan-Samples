#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 texCoord;

// Instanced attributes
layout(location = 4) in vec4 gridPos;
layout(location = 5) in vec4 gridData;

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
    pos.w = 1.0;

    vec4 rgb;
    vec3 velocity = vec3(0.0);

    if (gridData.w > 0) {
        velocity = gridData.xyz * (1.0 / gridData.w);
    }

    if (length(velocity) < 0.1) {
        rgb = vec4(0.0);
    } else {
        velocity = normalize(velocity);
        velocity = 0.5 * (velocity + vec3(1.0));
        rgb = vec4(velocity, 0.2);
    }

    // if (gridData.w == 0) {
    //     rgb = vec4(0.0);
    // } else {
    //     rgb = vec4(normalize(gridData.xyz), 0.5);
    // }

    o_color = rgb;

    o_pos = pos;

    gl_Position = global_uniform.projection * global_uniform.view * pos;
}
