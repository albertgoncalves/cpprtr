#ifndef __MATH_H__
#define __MATH_H__

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

static Vec3 operator-(Vec3 a, f32 b) {
    return {
        a.x - b,
        a.y - b,
        a.z - b,
    };
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

static Vec3 reflect(Vec3 v, Vec3 n) {
    return v - (2.0f * dot(v, n) * n);
}

static Vec3 refract(Vec3 uv, Vec3 n, f32 etai_over_etat) {
    f32  cos_theta = dot(-uv, n);
    Vec3 parallel = etai_over_etat * (uv + (cos_theta * n));
    f32  length_squared = dot(parallel, parallel);
    Vec3 perpendicular;
    if (1.0f <= length_squared) {
        perpendicular = {};
    } else {
        perpendicular = (-sqrtf(1.0f - length_squared)) * n;
    }
    return parallel + perpendicular;
}

static f32 schlick(f32 cosine, f32 refreactive_index) {
    f32 r0 = (1.0f - refreactive_index) / (1.0f + refreactive_index);
    r0 *= r0;
    return r0 + ((1.0f - r0) * powf(1.0f - cosine, 5.0f));
}

#endif
