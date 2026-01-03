// cube.frag.glsl
#version 450

layout(location = 0) in vec3 v_col;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D u_tex;

void main()
{
    vec4 t = texture(u_tex, v_uv);
    out_color = vec4(v_col, 1.0) * t;
}

