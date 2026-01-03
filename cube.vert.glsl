// cube.vert.glsl
#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_col;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_col;
layout(location = 1) out vec2 v_uv;

layout(set = 1, binding = 0) uniform UBO
{
    mat4 u_mvp;
} ubo;

void main()
{
    v_col = in_col;
    v_uv = in_uv;
    gl_Position = ubo.u_mvp * vec4(in_pos, 1.0);
}

