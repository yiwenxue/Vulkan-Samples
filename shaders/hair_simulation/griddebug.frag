#version 450

layout (location = 1) in vec4 instance;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    float dist = distance(instance.xyz, vec3(0.0, 0.0, 0.0));

    float alpha = 0;
    if (instance.w > 0) { alpha = 0.2; }

	outFragColor = vec4(instance.xyz / dist, alpha);		
}