#ifndef __MATH_H__
#define __MATH_H__

#include "types.h"

#define PI 3.1415926535897932385f

static f32 degrees_to_radians(f32 degrees) {
    return (degrees * PI) / 180.0f;
}

struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
};

static Vec3 operator+(Vec3 a, Vec3 b) {
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
    };
}

static Vec3 operator+(Vec3 a, f32 b) {
    return {
        a.x + b,
        a.y + b,
        a.z + b,
    };
}

static Vec3& operator+=(Vec3& a, Vec3 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

static Vec3 operator-(Vec3 a) {
    return {
        -a.x,
        -a.y,
        -a.z,
    };
}

static Vec3 operator-(Vec3 a, Vec3 b) {
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
    };
}

static Vec3& operator-=(Vec3& a, Vec3 b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

static Vec3 operator*(Vec3 a, f32 b) {
    return {
        a.x * b,
        a.y * b,
        a.z * b,
    };
}

static Vec3 operator*(f32 a, Vec3 b) {
    return {
        a * b.x,
        a * b.y,
        a * b.z,
    };
}

static Vec3 operator/(Vec3 a, f32 b) {
    return {
        a.x / b,
        a.y / b,
        a.z / b,
    };
}

static f32 dot(Vec3 a, Vec3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

static f32 len(Vec3 a) {
    return sqrtf((a.x * a.x) + (a.y * a.y) + (a.z * a.z));
}

static Vec3 unit(Vec3 a) {
    return a / len(a);
}

static Vec3 cross(Vec3 a, Vec3 b) {
    return {
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x),
    };
}

#endif
