#include <atomic>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "types.h"

#define BUFFER_WIDTH  512
#define BUFFER_HEIGHT 512
#define BUFFER_SIZE   262144

#define FLOAT_HEIGHT 512.0f
#define FLOAT_WIDTH  512.0f

#define RGB_COLOR_SCALE 255.999f

#define FILEPATH "out/main.bmp"

#include "bmp.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

static Vec3 at(Ray* ray, f32 t) {
    return ray->origin + (ray->direction * t);
}

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

static void set_pixels(Pixel* pixels) {
    f32  viewport_width = 2.0f;
    f32  viewport_height = (FLOAT_HEIGHT / FLOAT_WIDTH) * viewport_width;
    f32  focal_length = 1.0f;
    Vec3 origin = {
        0.0f,
        0.0f,
        0.0f,
    };
    Vec3 horizontal = {
        viewport_width,
        0.0f,
        0.0f,
    };
    Vec3 vertical = {
        0.0f,
        viewport_height,
        0.0f,
    };
    Vec3 lower_left_corner = origin - (horizontal / 2.0f) - (vertical / 2.0f);
    lower_left_corner.z -= focal_length;
    for (u16 j = 0; j < BUFFER_HEIGHT; ++j) {
        f32  y = (f32)j / FLOAT_HEIGHT;
        Vec3 y_vertical = y * vertical;
        for (u16 i = 0; i < BUFFER_WIDTH; ++i) {
            f32 x = (f32)i / FLOAT_WIDTH;
            Ray ray = {
                origin,
                (lower_left_corner + (x * horizontal) + y_vertical) - origin,
            };
            RgbColor color = get_color(&ray);
            pixels->red = (u8)(RGB_COLOR_SCALE * color.red);
            pixels->green = (u8)(RGB_COLOR_SCALE * color.green);
            pixels->blue = (u8)(RGB_COLOR_SCALE * color.blue);
            ++pixels;
        }
    }
}

int main() {
    FileHandle* file = fopen(FILEPATH, "wb");
    if (file == NULL) {
        return EXIT_FAILURE;
    }
    BmpImage* image = (BmpImage*)calloc(1, sizeof(BmpImage));
    if (image == NULL) {
        return EXIT_FAILURE;
    }
    set_bmp_header(&image->bmp_header);
    set_dib_header(&image->dib_header);
    set_pixels(image->pixels);
    write_bmp(file, image);
    fclose(file);
    free(image);
    printf("Done!\n");
    return EXIT_SUCCESS;
}
