#pragma once

#include <math.h>

typedef struct vec3
{
    float x, y, z;
} vec3;

#define VEC3_ZERO { 0.0f, 0.0f, 0.0f };

static vec3 vec3_add(vec3 a, vec3 b)
{
    vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}
static vec3 vec3_sub(vec3 a, vec3 b)
{
    vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static vec3 vec3_mul(vec3 vector, float scalar)
{
    vec3 r = { vector.x * scalar, vector.y * scalar, vector.z * scalar };
    return r;
}

static vec3 vec3_cross(vec3 a, vec3 b)
{
    vec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static float vec3_dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3 vec3_norm(vec3 v)
{
    float len = sqrtf(vec3_dot(v, v));
    if (len <= 0.0f)
    {
        vec3 z = { 0, 0, 0 };
        return z;
    }
    vec3 r = { v.x / len, v.y / len, v.z / len };
    return r;
}
