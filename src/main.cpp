#include <atomic>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

#include "types.h"

#define IMAGE_WIDTH  512u
#define IMAGE_HEIGHT 512u
#define N_PIXELS     262144u

#define FLOAT_HEIGHT 512.0f
#define FLOAT_WIDTH  512.0f

#define N_SPHERES 4u

#define N_BOUNCES         8u
#define SAMPLES_PER_PIXEL 64u
#define EPSILON           0.001f

#define BLOCK_WIDTH  64u
#define BLOCK_HEIGHT 64u
#define X_BLOCKS     8u
#define Y_BLOCKS     8u
#define N_BLOCKS     64u

#define N_THREADS 4u

#define FILEPATH "out/main.bmp"

#include "bmp.h"
#include "color.h"
#include "math.h"
#include "random.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

enum Material {
    LAMBERTIAN,
    METAL,
};

struct HitRecord {
    Vec3     point;
    Vec3     normal;
    Material material;
    RgbColor albedo;
    f32      fuzz;
    f32      t;
    bool     front_face;
};

struct Sphere {
    Vec3     center;
    RgbColor albedo;
    f32      radius;
    f32      fuzz;
    Material material;
};

struct Point {
    u32 x;
    u32 y;
};

struct Block {
    Point start;
    Point end;
};

struct Payload {
    Pixel* buffer;
    Block* blocks;
};

struct Memory {
    BmpImage image;
    Thread   threads[N_THREADS];
    Block    blocks[N_BLOCKS];
};

static u16Atomic INDEX;

static const Vec3 ORIGIN = {
    0.0f,
    0.0f,
    0.0f,
};

static const Vec3 VIEWPORT_WIDTH = {
    2.0f,
    0.0f,
    0.0f,
};

static const Vec3 VIEWPORT_HEIGHT = {
    0.0f,
    2.0f,
    0.0f,
};

static const Vec3 FOCAL_LENGTH = {
    0.0f,
    0.0f,
    1.0f,
};

static const Sphere SPHERES[N_SPHERES] = {
    {{0.0f, 0.0f, -1.0f}, {0.7f, 0.3f, 0.3f}, 0.5f, 0.0f, LAMBERTIAN},
    {{0.0f, -100.5f, -1.0f}, {0.8f, 0.8f, 0.0f}, 100.0f, 0.0f, LAMBERTIAN},
    {{1.0f, 0.0f, -1.0f}, {0.8f, 0.6f, 0.2f}, 0.5f, 0.1f, METAL},
    {{-1.0f, 0.0f, -1.0f}, {0.8f, 0.8f, 0.8f}, 0.5f, 0.05f, METAL},
};

static const Vec3 VIEWPORT_BOTTOM_LEFT =
    ORIGIN - (VIEWPORT_WIDTH / 2.0f) - (VIEWPORT_HEIGHT / 2.0f) - FOCAL_LENGTH;

static Vec3 at(const Ray* ray, f32 t) {
    return ray->origin + (ray->direction * t);
}

static void set_record(HitRecord*    record,
                       const Sphere* sphere,
                       const Ray*    ray,
                       f32           t) {
    record->t = t;
    Vec3 point = at(ray, t);
    record->point = point;
    Vec3 outward_normal = (point - sphere->center) / sphere->radius;
    bool front_face = dot(ray->direction, outward_normal) < 0;
    record->front_face = front_face;
    record->normal = front_face ? outward_normal : -outward_normal;
    record->material = sphere->material;
    record->albedo = sphere->albedo;
    record->fuzz = sphere->fuzz;
}

static bool hit(const Sphere* sphere,
                const Ray*    ray,
                HitRecord*    record,
                f32           t_min,
                f32           t_max) {
    Vec3 origin_center = ray->origin - sphere->center;
    f32  a = dot(ray->direction, ray->direction);
    f32  half_b = dot(origin_center, ray->direction);
    f32  c =
        dot(origin_center, origin_center) - (sphere->radius * sphere->radius);
    f32 discriminant = (half_b * half_b) - (a * c);
    if (0.0f < discriminant) {
        f32 root = sqrtf(discriminant);
        f32 t = (-half_b - root) / a;
        if ((t_min < t) && (t < t_max)) {
            set_record(record, sphere, ray, t);
            return true;
        }
        t = (-half_b + root) / a;
        if ((t_min < t) && (t < t_max)) {
            set_record(record, sphere, ray, t);
            return true;
        }
    }
    return false;
}

static Vec3 get_random_vec3(PcgRng* rng) {
    return {
        get_random_f32(rng),
        get_random_f32(rng),
        get_random_f32(rng),
    };
}

static Vec3 get_random_in_unit_sphere(PcgRng* rng) {
    for (;;) {
        Vec3 point = (get_random_vec3(rng) * 2.0f) - 1.0f;
        if (dot(point, point) < 1.0f) {
            return point;
        }
    }
}

static Vec3 get_random_unit_vector(PcgRng* rng) {
    f32 a = get_random_f32(rng) * 2.0f * PI;
    f32 z = (get_random_f32(rng) * 2.0f) - 1.0f;
    f32 r = sqrtf(1.0f - (z * z));
    return {
        r * cosf(a),
        r * sinf(a),
        z,
    };
}

static Vec3 get_random_in_hemisphere(PcgRng* rng, Vec3 normal) {
    Vec3 point = get_random_in_unit_sphere(rng);
    if (0.0f < dot(point, normal)) {
        return point;
    }
    return -point;
}

static Vec3 reflect(Vec3 v, Vec3 n) {
    return v - (2.0f * dot(v, n) * n);
}

RgbColor get_color(const Ray*, PcgRng*, u16);
RgbColor get_color(const Ray* ray, PcgRng* rng, u16 depth) {
    if (depth < 1u) {
        return {};
    }
    HitRecord last_record = {};
    HitRecord nearest_record = {};
    bool      hit_anything = false;
    f32       t_nearest = F32_MAX;
    for (u8 i = 0; i < N_SPHERES; ++i) {
        if (hit(&SPHERES[i], ray, &last_record, EPSILON, t_nearest)) {
            hit_anything = true;
            t_nearest = last_record.t;
            nearest_record = last_record;
        }
    }
    if (hit_anything) {
        switch (nearest_record.material) {
        case LAMBERTIAN: {
            Vec3 direction =
                nearest_record.normal + get_random_unit_vector(rng);
            Ray scattered = {
                nearest_record.point,
                direction,
            };
            RgbColor attenuation = nearest_record.albedo;
            return attenuation * get_color(&scattered, rng, (u16)(depth - 1u));
        }
        case METAL: {
            Vec3 reflected =
                reflect(unit(ray->direction), nearest_record.normal);
            Ray scattered = {
                nearest_record.point,
                reflected +
                    (nearest_record.fuzz * get_random_in_unit_sphere(rng)),
            };
            RgbColor attenuation = nearest_record.albedo;
            if (0.0f < dot(scattered.direction, nearest_record.normal)) {
                return attenuation *
                       get_color(&scattered, rng, (u16(depth - 1u)));
            }
            return {};
        }
        }
    }
    f32      t = 0.5f * (unit(ray->direction).y + 1.0f);
    RgbColor color = {
        t * 0.5f,
        t * 0.7f,
        t,
    };
    color += 1.0f - t;
    return color;
}

static void set_color(Pixel* pixel, RgbColor* color) {
    *color /= (f32)SAMPLES_PER_PIXEL;
    clamp(color, 0.0f, 1.0f);
    pixel->red = (u8)(RGB_COLOR_SCALE * sqrtf(color->red));
    pixel->green = (u8)(RGB_COLOR_SCALE * sqrtf(color->green));
    pixel->blue = (u8)(RGB_COLOR_SCALE * sqrtf(color->blue));
}

static void render_block(Pixel* pixels, Block block, PcgRng* rng) {
    for (u32 j = block.start.y; j < block.end.y; ++j) {
        u32 offset = j * IMAGE_WIDTH;
        for (u32 i = block.start.x; i < block.end.x; ++i) {
            RgbColor color = {};
            for (u8 _ = 0; _ < SAMPLES_PER_PIXEL; ++_) {
                f32 x = ((f32)i + get_random_f32(rng)) / FLOAT_WIDTH;
                f32 y = ((f32)j + get_random_f32(rng)) / FLOAT_HEIGHT;
                Ray ray = {
                    ORIGIN,
                    (VIEWPORT_BOTTOM_LEFT + (x * VIEWPORT_WIDTH) +
                     (y * VIEWPORT_HEIGHT)) -
                        ORIGIN,
                };
                color += get_color(&ray, rng, N_BOUNCES);
            }
            set_color(&pixels[i + offset], &color);
        }
    }
}

static void* thread_render(void* args) {
    Payload* payload = (Payload*)args;
    Pixel*   buffer = payload->buffer;
    PcgRng   rng = {};
    init_random(&rng);
    for (;;) {
        u16 index = INDEX.fetch_add(1, std::memory_order_seq_cst);
        if (N_BLOCKS <= index) {
            return NULL;
        }
        render_block(buffer, payload->blocks[index], &rng);
    }
}

static void set_pixels(Memory* memory) {
    Payload payload;
    payload.buffer = memory->image.pixels;
    payload.blocks = memory->blocks;
    u16 index = 0;
    for (u32 y = 0; y < Y_BLOCKS; ++y) {
        for (u32 x = 0; x < X_BLOCKS; ++x) {
            Point start = {
                x * BLOCK_WIDTH,
                y * BLOCK_HEIGHT,
            };
            Point end = {
                start.x + BLOCK_WIDTH,
                start.y + BLOCK_HEIGHT,
            };
            end.x = end.x < IMAGE_WIDTH ? end.x : IMAGE_WIDTH;
            end.y = end.y < IMAGE_HEIGHT ? end.y : IMAGE_HEIGHT;
            Block block = {
                start,
                end,
            };
            memory->blocks[index++] = block;
        }
    }
    for (u8 i = 0; i < N_THREADS; ++i) {
        pthread_create(&memory->threads[i], NULL, thread_render, &payload);
    }
    for (u8 i = 0; i < N_THREADS; ++i) {
        pthread_join(memory->threads[i], NULL);
    }
}

int main() {
    printf("sizeof(Vec3)      : %zu\n"
           "sizeof(Material)  : %zu\n"
           "sizeof(HitRecord) : %zu\n"
           "sizeof(Sphere)    : %zu\n"
           "sizeof(Ray)       : %zu\n"
           "sizeof(Point)     : %zu\n"
           "sizeof(Block)     : %zu\n"
           "sizeof(Payload)   : %zu\n"
           "sizeof(Memory)    : %zu\n"
           "\n",
           sizeof(Vec3),
           sizeof(Material),
           sizeof(HitRecord),
           sizeof(Sphere),
           sizeof(Ray),
           sizeof(Point),
           sizeof(Block),
           sizeof(Payload),
           sizeof(Memory));
    FileHandle* file = fopen(FILEPATH, "wb");
    if (file == NULL) {
        return EXIT_FAILURE;
    }
    Memory* memory = (Memory*)calloc(1, sizeof(Memory));
    if (memory == NULL) {
        return EXIT_FAILURE;
    }
    set_bmp_header(&memory->image.bmp_header);
    set_dib_header(&memory->image.dib_header);
    set_pixels(memory);
    write_bmp(file, &memory->image);
    fclose(file);
    free(memory);
    printf("Done!\n");
    return EXIT_SUCCESS;
}
