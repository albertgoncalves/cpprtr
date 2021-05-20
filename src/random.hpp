#ifndef __RANDOM_H__
#define __RANDOM_H__

struct PcgRng {
    u64 state;
    u64 increment;
};

static u32 rotr(u32 x, u32 n) {
    __asm__("ror %1,%0" : "+r"(x) : "c"(n));
    return x;
}

static u32 get_random_u32(PcgRng* rng) {
    const u64 state = rng->state;
    rng->state = (state * 6364136223846793005llu) + (rng->increment | 1u);
    const u32 xor_shift = (u32)(((state >> 18u) ^ state) >> 27u);
    const u32 rotate = (u32)(state >> 59u);
    return rotr(xor_shift, rotate);
}

static void set_seed(PcgRng* rng, u64 state, u64 increment) {
    rng->state = 0u;
    rng->increment = (increment << 1u) | 1u;
    get_random_u32(rng);
    rng->state += state;
    get_random_u32(rng);
}

// NOTE: See `https://github.com/hfinkel/sleef-bgq/blob/master/purec/sleefsp.c#L117-L130`.
static f32 ldexpf_(f32 x, i32 q) {
    i32 m = q >> 31;
    m = (((m + q) >> 6) - m) << 4;
    q = q - (m << 2);
    m += 127;
    m = m < 0 ? 0 : m;
    m = m > 255 ? 255 : m;
    union {
        f32 as_f32;
        i32 as_i32;
    } u;
    u.as_i32 = m << 23;
    x = x * u.as_f32 * u.as_f32 * u.as_f32 * u.as_f32;
    u.as_i32 = (q + 0x7F) << 23;
    return x * u.as_f32;
}

static f32 get_random_f32(PcgRng* rng) {
    return ldexpf_((f32)get_random_u32(rng), -32);
}

#endif
