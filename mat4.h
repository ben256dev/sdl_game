#include <math.h>

#include "vec3.h"

typedef struct mat4
{
    float m[16];
} mat4;

static mat4 mat4_identity(void)
{
    mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

static mat4 mat4_mul(mat4 a, mat4 b)
{
    mat4 r;
    for (int c = 0; c < 4; c++)
    {
        for (int row = 0; row < 4; row++)
        {
            r.m[c * 4 + row] =
                a.m[0 * 4 + row] * b.m[c * 4 + 0] +
                a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                a.m[2 * 4 + row] * b.m[c * 4 + 2] +
                a.m[3 * 4 + row] * b.m[c * 4 + 3];
        }
    }
    return r;
}

static mat4 mat4_rotate_x(float a)
{
    mat4 r = mat4_identity();
    float c = cosf(a);
    float s = sinf(a);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

static mat4 mat4_rotate_y(float a)
{
    mat4 r = mat4_identity();
    float c = cosf(a);
    float s = sinf(a);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

static mat4 mat4_perspective(float fovy_radians, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fovy_radians * 0.5f);
    mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

static mat4 mat4_lookat(vec3 eye, vec3 center, vec3 up)
{
    vec3 f = vec3_norm(vec3_sub(center, eye));
    vec3 s = vec3_norm(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);

    mat4 r = mat4_identity();
    r.m[0] = s.x;
    r.m[1] = u.x;
    r.m[2] = -f.x;

    r.m[4] = s.y;
    r.m[5] = u.y;
    r.m[6] = -f.y;

    r.m[8] = s.z;
    r.m[9] = u.z;
    r.m[10] = -f.z;

    r.m[12] = -vec3_dot(s, eye);
    r.m[13] = -vec3_dot(u, eye);
    r.m[14] = vec3_dot(f, eye);

    return r;
}
