#ifndef __COLOR_H__
#define __COLOR_H__

#include "types.h"

#define RGB_COLOR_SCALE 255.0f

struct RgbColor {
    f32 red;
    f32 green;
    f32 blue;
};

static RgbColor& operator+=(RgbColor& a, RgbColor b) {
    a.red += b.red;
    a.green += b.green;
    a.blue += b.blue;
    return a;
}

static RgbColor& operator+=(RgbColor& a, f32 b) {
    a.red += b;
    a.green += b;
    a.blue += b;
    return a;
}

static RgbColor operator*(RgbColor a, RgbColor b) {
    return {
        a.red * b.red,
        a.green * b.green,
        a.blue * b.blue,
    };
}

static RgbColor& operator*=(RgbColor& a, RgbColor b) {
    a.red *= b.red;
    a.green *= b.green;
    a.blue *= b.blue;
    return a;
}

static RgbColor& operator/=(RgbColor& a, f32 b) {
    a.red /= b;
    a.green /= b;
    a.blue /= b;
    return a;
}

static f32 clamp(f32 x, f32 min, f32 max) {
    if (x < min) {
        return min;
    } else if (max < x) {
        return max;
    } else {
        return x;
    }
}

static void clamp(RgbColor* color, f32 min, f32 max) {
    color->red = clamp(color->red, min, max);
    color->green = clamp(color->green, min, max);
    color->blue = clamp(color->blue, min, max);
}

#endif
