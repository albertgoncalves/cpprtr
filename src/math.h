#ifndef __MATH_H__
#define __MATH_H__

#include "types.h"

struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
};

static u8 clamp_add(u8 a, u8 b) {
    return (U8_MAX - b) < a ? (u8)U8_MAX : (u8)(a + b);
}

static u8 mul(u8 a, f32 b) {
    f32 result = ((f32)a) * b;
    return (u8)(255.0f < result ? 255.0f : result);
}

static Vec3 operator+(Vec3 a, Vec3 b) {
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
    };
}

static Vec3 operator-(Vec3 a, Vec3 b) {
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
    };
}

static f32 dot(Vec3 a, Vec3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

static f32 len(Vec3 a) {
    return sqrtf((a.x * a.x) + (a.y * a.y) + (a.z * a.z));
}

#endif
