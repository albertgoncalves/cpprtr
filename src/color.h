#ifndef __COLOR_H__
#define __COLOR_H__

#include "math.h"

struct RgbColor {
    u8 blue;
    u8 red;
    u8 green;
};

static RgbColor add_color(RgbColor a, RgbColor b) {
    RgbColor result = {
        clamp_add(a.red, b.red),
        clamp_add(a.green, b.green),
        clamp_add(a.blue, b.blue),
    };
    return result;
}

#endif
