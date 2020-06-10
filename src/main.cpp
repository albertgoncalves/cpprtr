#include <atomic>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "types.h"

#define BUFFER_WIDTH  512
#define BUFFER_HEIGHT 512
constexpr u32 BUFFER_SIZE = BUFFER_WIDTH * BUFFER_WIDTH;

constexpr f32 FLOAT_HEIGHT = (f32)BUFFER_WIDTH;
constexpr f32 FLOAT_WIDTH = (f32)BUFFER_HEIGHT;
constexpr f32 FLOAT_HALF_PERIMETER = FLOAT_HEIGHT + FLOAT_WIDTH;

#define RGB_COLOR_SCALE 256.0f

#define FILEPATH "out/main.bmp"

#include "bmp.h"

static void set_pixels(Pixel* pixels) {
    for (f32 y = 0.0f; y < FLOAT_HEIGHT; ++y) {
        f32 red = (y / FLOAT_HEIGHT) * RGB_COLOR_SCALE;
        for (f32 x = 0.0f; x < FLOAT_WIDTH; ++x) {
            f32 green = (x / FLOAT_WIDTH) * RGB_COLOR_SCALE;
            f32 blue = ((x + y) / FLOAT_HALF_PERIMETER) * RGB_COLOR_SCALE;
            pixels->blue = (u8)blue;
            pixels->green = (u8)green;
            pixels->red = (u8)red;
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
