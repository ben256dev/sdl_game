#pragma once

#include <math.h>

typedef struct vec2
{
    float x, y;
} vec2;

#define VEC2_ZERO { 0.0f, 0.0f };

static inline vec2 vec2_add(vec2 a, vec2 b)
{
    vec2 r = { a.x + b.x, a.y + b.y };
    return r;
}
static inline vec2 vec2_sub(vec2 a, vec2 b)
{
    vec2 r = { a.x - b.x, a.y - b.y };
    return r;
}

static inline vec2 vec2_mul(vec2 vector, float scalar)
{
    vec2 r = { vector.x * scalar, vector.y * scalar };
    return r;
}

static inline float vec2_cross(vec2 a, vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

static inline float vec2_dot(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

static inline vec2 vec2_norm(vec2 v)
{
    float len = sqrtf(vec2_dot(v, v));
    if (len <= 0.0f)
    {
        vec2 z = { 0.0f, 0.0f };
        return z;
    }
    vec2 r = { v.x / len, v.y / len };
    return r;
}

