#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

// Instanced attributes
layout (location = 3) in vec4 instancePos;
layout (location = 4) in vec4 instanceData;

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
    vec4 view_dir;
} global_uniform;

layout (location = 0) out vec4 instance;

void main() {
    vec4 pos = inPos + instancePos;
    gl_Position = global_uniform.view_proj * global_uniform.model * pos
}