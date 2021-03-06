#ifndef __COLOR_H__
#define __COLOR_H__

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
    return x < min ? min : max < x ? max : x;
}

static void clamp(RgbColor* color, f32 min, f32 max) {
    color->red = clamp(color->red, min, max);
    color->green = clamp(color->green, min, max);
    color->blue = clamp(color->blue, min, max);
}

#endif
