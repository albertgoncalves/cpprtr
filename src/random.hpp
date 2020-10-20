#ifndef __RANDOM_H__
#define __RANDOM_H__

struct PcgRng {
    u64 state;
    u64 increment;
};

static u32 get_random_u32(PcgRng* rng) {
    const u64 state = rng->state;
    rng->state = (state * 6364136223846793005ull) + (rng->increment | 1u);
    const u32 xor_shift = (u32)(((state >> 18u) ^ state) >> 27u);
    const u32 rotate = (u32)(state >> 59u);
    return (xor_shift >> rotate) | (xor_shift << ((-rotate) & 31u));
}

static void set_seed(PcgRng* rng, u64 state, u64 increment) {
    rng->state = 0u;
    rng->increment = (increment << 1u) | 1u;
    get_random_u32(rng);
    rng->state += state;
    get_random_u32(rng);
}

static f32 get_random_f32(PcgRng* rng) {
    return ldexpf((f32)get_random_u32(rng), -32);
}

#endif
