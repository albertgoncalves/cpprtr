#ifndef __BMP_H__
#define __BMP_H__

#include "types.hpp"

#pragma pack(push, 1)

struct BmpHeader {
    u16 id;
    u32 file_size;
    u32 _;
    u32 header_offset;
};

struct DibHeader {
    u32 header_size;
    i32 pixel_width;
    i32 pixel_height;
    u16 color_planes;
    u16 bits_per_pixel;
    u8  _[24];
};

struct Pixel {
    u8 blue;
    u8 green;
    u8 red;
};

#pragma pack(pop)

#define BMP_HEADER_SIZE sizeof(BmpHeader) + sizeof(DibHeader)
#define BMP_FILE_SIZE   BMP_HEADER_SIZE + sizeof(Pixel[N_PIXELS])

struct BmpImage {
    Pixel     pixels[N_PIXELS];
    DibHeader dib_header;
    BmpHeader bmp_header;
};

static void set_bmp_header(BmpHeader* header) {
    header->id = 0x4d42;
    header->file_size = BMP_FILE_SIZE;
    header->header_offset = BMP_HEADER_SIZE;
}

static void set_dib_header(DibHeader* header) {
    header->header_size = sizeof(DibHeader);
    header->pixel_width = (i32)IMAGE_WIDTH;
    header->pixel_height = (i32)IMAGE_HEIGHT;
    header->color_planes = 1;
    header->bits_per_pixel = sizeof(Pixel) * 8;
}

static void write_bmp(FileHandle* file, BmpImage* image) {
    if (fwrite(&image->bmp_header, 1, sizeof(BmpHeader), file) !=
        sizeof(BmpHeader))
    {
        exit(EXIT_FAILURE);
    }
    if (fwrite(&image->dib_header, 1, sizeof(DibHeader), file) !=
        sizeof(DibHeader))
    {
        exit(EXIT_FAILURE);
    }
    if (fwrite(&image->pixels, 1, sizeof(Pixel[N_PIXELS]), file) !=
        sizeof(Pixel[N_PIXELS]))
    {
        exit(EXIT_FAILURE);
    }
}

#endif
