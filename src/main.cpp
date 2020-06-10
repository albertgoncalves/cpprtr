#include <atomic>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "types.h"

#define IMAGE_WIDTH  512u
#define IMAGE_HEIGHT 512u
#define N_PIXELS     262144u

#define FLOAT_HEIGHT 512.0f
#define FLOAT_WIDTH  512.0f

#define RGB_COLOR_SCALE 255.999f

#define BLOCK_WIDTH  128u
#define BLOCK_HEIGHT 128u
#define X_BLOCKS     4u
#define Y_BLOCKS     4u
#define N_BLOCKS     16u

#define N_THREADS 3u

#define FILEPATH "out/main.bmp"

#include "bmp.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

struct XY {
    u32 x;
    u32 y;
};

struct Block {
    XY start;
    XY end;
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

static Vec3 at(Ray* ray, f32 t) {
    return ray->origin + (ray->direction * t);
}

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

static const Vec3 VIEWPORT_DEPTH = {
    0.0f,
    0.0f,
    1.0f,
};

static const Vec3 LOWER_LEFT_CORNER = ORIGIN - (VIEWPORT_WIDTH / 2.0f) -
                                      (VIEWPORT_HEIGHT / 2.0f) -
                                      VIEWPORT_DEPTH;

static f32 hit_sphere(Vec3 center, f32 radius, Ray* ray) {
    Vec3 oc = ray->origin - center;
    f32  a = dot(ray->direction, ray->direction);
    f32  b = 2.0f * dot(oc, ray->direction);
    f32  c = dot(oc, oc) - (radius * radius);
    f32  discriminant = (b * b) - (4.0f * a * c);
    if (discriminant < 0.0f) {
        return -1.0f;
    }
    return (-b - sqrt(discriminant)) / (2.0f * a);
}

static RgbColor get_color(Ray* ray) {
    f32 t = hit_sphere({0.0f, 0.0f, -1.0f}, 0.5f, ray);
    if (0.0f < t) {
        Vec3 v = {
            0.0f,
            0.0f,
            -1.0f,
        };
        Vec3 normal = unit(at(ray, t) - v);
        return {
            0.5f * (normal.x + 1.0f),
            0.5f * (normal.y + 1.0f),
            0.5f * (normal.z + 1.0f),
        };
    }
    t = 0.5f * (unit(ray->direction).y + 1.0f);
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
                (LOWER_LEFT_CORNER + (x * VIEWPORT_WIDTH) + y_vertical) -
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
            XY start = {
                x * BLOCK_WIDTH,
                y * BLOCK_HEIGHT,
            };
            XY end = {
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
           "sizeof(XY)      : %zu\n"
           "sizeof(Block)   : %zu\n"
           "sizeof(Payload) : %zu\n"
           "sizeof(Memory)  : %zu\n"
           "\n",
           sizeof(Vec3),
           sizeof(Ray),
           sizeof(XY),
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
