#include <atomic>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "types.h"

#define IMAGE_WIDTH  1024u
#define IMAGE_HEIGHT 1024u
#define N_PIXELS     1048576u

#define FLOAT_HEIGHT 1024.0f
#define FLOAT_WIDTH  1024.0f

#define N_SPHERES 1u

#define RGB_COLOR_SCALE 255.0f

#define BLOCK_WIDTH  128u
#define BLOCK_HEIGHT 128u
#define X_BLOCKS     8u
#define Y_BLOCKS     8u
#define N_BLOCKS     64u

#define N_THREADS 3u

#define FILEPATH "out/main.bmp"

#include "bmp.h"
#include "math.h"

struct RgbColor {
    f32 red;
    f32 green;
    f32 blue;
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    f32  t;
    bool front_face;
};

struct Sphere {
    Vec3 center;
    f32  radius;
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
    {{0.0f, 0.0f, -1.0f}, 0.5f},
};

static const Vec3 VIEWPORT_BOTTOM_LEFT =
    ORIGIN - (VIEWPORT_WIDTH / 2.0f) - (VIEWPORT_HEIGHT / 2.0f) - FOCAL_LENGTH;

static Vec3 at(const Ray* ray, f32 t) {
    return ray->origin + (ray->direction * t);
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
            record->t = t;
            Vec3 point = at(ray, t);
            record->point = point;
            Vec3 outward_normal = (point - sphere->center) / sphere->radius;
            bool front_face = dot(ray->direction, outward_normal) < 0;
            record->front_face = front_face;
            record->normal = front_face ? outward_normal : -outward_normal;
            return true;
        }
        t = (-half_b + root) / a;
        if ((t_min < t) && (t < t_max)) {
            record->t = t;
            Vec3 point = at(ray, t);
            record->point = point;
            Vec3 outward_normal = (point - sphere->center) / sphere->radius;
            bool front_face = dot(ray->direction, outward_normal) < 0;
            record->front_face = front_face;
            record->normal = front_face ? outward_normal : -outward_normal;
            return true;
        }
    }
    return false;
}

/* NOTE: Stopped at `6.7 Common Constants and Utility Functions`. */
static RgbColor get_color(const Ray* ray) {
    HitRecord last_record = {};
    HitRecord nearest_record = {};
    bool      hit_anything = false;
    f32       t_nearest = F32_MAX;
    for (u8 i = 0; i < N_SPHERES; ++i) {
        if (hit(&SPHERES[i], ray, &last_record, 0, t_nearest)) {
            hit_anything = true;
            t_nearest = last_record.t;
            nearest_record = last_record;
        }
    }
    if (hit_anything) {
        return {
            0.5f * (nearest_record.normal.x + 1.0f),
            0.5f * (nearest_record.normal.y + 1.0f),
            0.5f * (nearest_record.normal.z + 1.0f),
        };
    }
    f32 t = 0.5f * (unit(ray->direction).y + 1.0f);
    f32 u = 1.0f - t;
    return {
        u + (t * 0.5f),
        u + (t * 0.7f),
        u + t,
    };
}

static void render_block(Pixel* pixels, Block block) {
    for (u32 j = block.start.y; j < block.end.y; ++j) {
        u32  offset = j * IMAGE_WIDTH;
        f32  y = (f32)j / FLOAT_HEIGHT;
        Vec3 y_vertical = y * VIEWPORT_HEIGHT;
        for (u32 i = block.start.x; i < block.end.x; ++i) {
            f32 x = (f32)i / FLOAT_WIDTH;
            Ray ray = {
                ORIGIN,
                (VIEWPORT_BOTTOM_LEFT + (x * VIEWPORT_WIDTH) + y_vertical) -
                    ORIGIN,
            };
            RgbColor color = get_color(&ray);
            Pixel*   pixel = &pixels[i + offset];
            pixel->red = (u8)(RGB_COLOR_SCALE * color.red);
            pixel->green = (u8)(RGB_COLOR_SCALE * color.green);
            pixel->blue = (u8)(RGB_COLOR_SCALE * color.blue);
        }
    }
}

static void* thread_render(void* args) {
    Payload* payload = (Payload*)args;
    Pixel*   buffer = payload->buffer;
    for (;;) {
        u16 index = INDEX.fetch_add(1, std::memory_order_seq_cst);
        if (N_BLOCKS <= index) {
            return NULL;
        }
        render_block(buffer, payload->blocks[index]);
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
    printf("sizeof(Vec3)    : %zu\n"
           "sizeof(Ray)     : %zu\n"
           "sizeof(Point)   : %zu\n"
           "sizeof(Block)   : %zu\n"
           "sizeof(Payload) : %zu\n"
           "sizeof(Memory)  : %zu\n"
           "\n",
           sizeof(Vec3),
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
