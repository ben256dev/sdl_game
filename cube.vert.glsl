// cube.vert.glsl
#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_color;
layout(location = 1) out vec2 v_uv;

layout(set = 1, binding = 0) uniform UBO
{
    mat4 mvp;
} ubo;

void main()
{
    gl_Position = ubo.mvp * vec4(in_pos, 1.0);
    v_color = in_color;
    v_uv = in_uv;
}

