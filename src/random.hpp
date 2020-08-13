#ifndef __RANDOM_H__
#define __RANDOM_H__

#include "types.hpp"

#define PCG_CONSTANT 9600629759793949339ull

struct PcgRng {
    u64 state;
    u64 increment;
};

static u32 get_microseconds() {
    TimeValue time;
    gettimeofday(&time, NULL);
    return (u32)time.tv_usec;
}

static void init_random(PcgRng* rng) {
    rng->state = PCG_CONSTANT * get_microseconds();
    rng->increment = PCG_CONSTANT * get_microseconds();
}

static u32 get_random_u32(PcgRng* rng) {
    u64 state = rng->state;
    rng->state = (state * 6364136223846793005ull) + (rng->increment | 1);
    u32 xor_shift = (u32)(((state >> 18u) ^ state) >> 27u);
    u32 rotate = (u32)(state >> 59u);
    return (xor_shift >> rotate) | (xor_shift << ((-rotate) & 31));
}

static u32 get_random_u32_below(PcgRng* rng, u32 bound) {
    u32 threshold = (-bound) % bound;
    for (;;) {
        u32 value = get_random_u32(rng);
        if (threshold <= value) {
            return value % bound;
        }
    }
}

static f32 get_random_f32(PcgRng* rng) {
    return (f32)get_random_u32_below(rng, U32_MAX) / (f32)U32_MAX;
}

#endif
