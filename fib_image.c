#include "fib_image.h"

#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>
#include <png.h>

#define FIB_MAX_IMAGE_DIMENSION 16384

typedef struct {
    struct jpeg_error_mgr jpeg_error;
    jmp_buf jump_buffer;
} FibJpegError;

static int safe_multiply_size(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > SIZE_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

void fib_image_free(FibImage *image) {
    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

static int fib_image_allocate(FibImage *image, int width, int height) {
    size_t pixel_count = 0;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "error: invalid image dimensions\n");
        return 0;
    }
    if (width > FIB_MAX_IMAGE_DIMENSION || height > FIB_MAX_IMAGE_DIMENSION) {
        fprintf(stderr, "error: image too large (max %dx%d)\n", FIB_MAX_IMAGE_DIMENSION, FIB_MAX_IMAGE_DIMENSION);
        return 0;
    }
    if (!safe_multiply_size((size_t)width, (size_t)height, &pixel_count)) {
        fprintf(stderr, "error: image size overflow\n");
        return 0;
    }

    image->pixels = (unsigned char *)malloc(pixel_count);
    if (!image->pixels) {
        fprintf(stderr, "error: not enough memory for image (%dx%d)\n", width, height);
        return 0;
    }

    image->width = width;
    image->height = height;
    return 1;
}

static unsigned char alpha_to_white(unsigned char channel, unsigned char alpha) {
    return (unsigned char)(((unsigned int)channel * alpha + 255U * (255U - alpha)) / 255U);
}

static int read_png_image(const char *path, FibImage *image) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "error: cannot open file %s\n", path);
        return 0;
    }

    unsigned char signature[8];
    if (fread(signature, 1, sizeof(signature), file) != sizeof(signature) || png_sig_cmp(signature, 0, sizeof(signature)) != 0) {
        fprintf(stderr, "error: invalid png signature: %s\n", path);
        fclose(file);
        return 0;
    }

    png_structp png_state = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_state) {
        fprintf(stderr, "error: cannot initialize png reader\n");
        fclose(file);
        return 0;
    }

    png_infop png_info = png_create_info_struct(png_state);
    if (!png_info) {
        fprintf(stderr, "error: cannot create png info\n");
        png_destroy_read_struct(&png_state, NULL, NULL);
        fclose(file);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_state))) {
        fprintf(stderr, "error: png decode failed\n");
        png_destroy_read_struct(&png_state, &png_info, NULL);
        fib_image_free(image);
        fclose(file);
        return 0;
    }

    png_init_io(png_state, file);
    png_set_sig_bytes(png_state, sizeof(signature));
    png_read_info(png_state, png_info);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;

    png_get_IHDR(png_state, png_info, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
    if (width == 0 || height == 0 || width > FIB_MAX_IMAGE_DIMENSION || height > FIB_MAX_IMAGE_DIMENSION) {
        fprintf(stderr, "error: png dimensions out of range\n");
        png_destroy_read_struct(&png_state, &png_info, NULL);
        fclose(file);
        return 0;
    }

    if (bit_depth == 16) {
        png_set_strip_16(png_state);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_state);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_state);
    }
    if (png_get_valid(png_state, png_info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_state);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_state);
    }
    if ((color_type & PNG_COLOR_MASK_ALPHA) == 0 && !png_get_valid(png_state, png_info, PNG_INFO_tRNS)) {
        png_set_filler(png_state, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(png_state, png_info);
    if (png_get_channels(png_state, png_info) != 4) {
        fprintf(stderr, "error: png channel transform failed\n");
        png_destroy_read_struct(&png_state, &png_info, NULL);
        fclose(file);
        return 0;
    }

    png_size_t row_bytes = png_get_rowbytes(png_state, png_info);
    size_t decode_buffer_size = 0;
    size_t row_pointer_size = 0;
    if (!safe_multiply_size((size_t)row_bytes, (size_t)height, &decode_buffer_size) ||
        !safe_multiply_size(sizeof(png_bytep), (size_t)height, &row_pointer_size)) {
        fprintf(stderr, "error: png buffer size overflow\n");
        png_destroy_read_struct(&png_state, &png_info, NULL);
        fclose(file);
        return 0;
    }

    unsigned char *decode_buffer = (unsigned char *)malloc(decode_buffer_size);
    png_bytep *rows = (png_bytep *)malloc(row_pointer_size);
    if (!decode_buffer || !rows) {
        fprintf(stderr, "error: not enough memory for png decode\n");
        free(decode_buffer);
        free(rows);
        png_destroy_read_struct(&png_state, &png_info, NULL);
        fclose(file);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        rows[y] = decode_buffer + (size_t)y * (size_t)row_bytes;
    }

    png_read_image(png_state, rows);
    png_read_end(png_state, NULL);

    if (!fib_image_allocate(image, (int)width, (int)height)) {
        free(rows);
        free(decode_buffer);
        png_destroy_read_struct(&png_state, &png_info, NULL);
        fclose(file);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        unsigned char *row = rows[y];
        for (png_uint_32 x = 0; x < width; x++) {
            unsigned char red = row[x * 4 + 0];
            unsigned char green = row[x * 4 + 1];
            unsigned char blue = row[x * 4 + 2];
            unsigned char alpha = row[x * 4 + 3];

            red = alpha_to_white(red, alpha);
            green = alpha_to_white(green, alpha);
            blue = alpha_to_white(blue, alpha);

            image->pixels[(size_t)y * (size_t)width + (size_t)x] =
                (unsigned char)((299U * red + 587U * green + 114U * blue) / 1000U);
        }
    }

    free(rows);
    free(decode_buffer);
    png_destroy_read_struct(&png_state, &png_info, NULL);
    fclose(file);
    return 1;
}

static void jpeg_fatal_exit(j_common_ptr jpeg_common) {
    FibJpegError *error = (FibJpegError *)jpeg_common->err;
    longjmp(error->jump_buffer, 1);
}

static int read_jpeg_image(const char *path, FibImage *image) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "error: cannot open file %s\n", path);
        return 0;
    }

    struct jpeg_decompress_struct jpeg_decoder;
    FibJpegError error_state;
    jpeg_decoder.err = jpeg_std_error(&error_state.jpeg_error);
    error_state.jpeg_error.error_exit = jpeg_fatal_exit;

    if (setjmp(error_state.jump_buffer)) {
        jpeg_destroy_decompress(&jpeg_decoder);
        fib_image_free(image);
        fclose(file);
        fprintf(stderr, "error: jpeg decode failed\n");
        return 0;
    }

    jpeg_create_decompress(&jpeg_decoder);
    jpeg_stdio_src(&jpeg_decoder, file);
    jpeg_read_header(&jpeg_decoder, TRUE);
    jpeg_start_decompress(&jpeg_decoder);

    if (jpeg_decoder.output_width == 0 || jpeg_decoder.output_height == 0 ||
        jpeg_decoder.output_width > FIB_MAX_IMAGE_DIMENSION || jpeg_decoder.output_height > FIB_MAX_IMAGE_DIMENSION) {
        jpeg_finish_decompress(&jpeg_decoder);
        jpeg_destroy_decompress(&jpeg_decoder);
        fclose(file);
        fprintf(stderr, "error: jpeg dimensions out of range\n");
        return 0;
    }

    if (!fib_image_allocate(image, (int)jpeg_decoder.output_width, (int)jpeg_decoder.output_height)) {
        jpeg_finish_decompress(&jpeg_decoder);
        jpeg_destroy_decompress(&jpeg_decoder);
        fclose(file);
        return 0;
    }

    int channel_count = jpeg_decoder.output_components;
    size_t row_bytes = 0;
    if (channel_count <= 0 || !safe_multiply_size((size_t)jpeg_decoder.output_width, (size_t)channel_count, &row_bytes) ||
        row_bytes > (size_t)UINT_MAX) {
        jpeg_finish_decompress(&jpeg_decoder);
        jpeg_destroy_decompress(&jpeg_decoder);
        fib_image_free(image);
        fclose(file);
        fprintf(stderr, "error: jpeg row size overflow\n");
        return 0;
    }

    JSAMPARRAY row = (*jpeg_decoder.mem->alloc_sarray)((j_common_ptr)&jpeg_decoder, JPOOL_IMAGE, (JDIMENSION)row_bytes, 1);

    size_t y = 0;
    while (jpeg_decoder.output_scanline < jpeg_decoder.output_height) {
        jpeg_read_scanlines(&jpeg_decoder, row, 1);
        unsigned char *source = row[0];

        for (size_t x = 0; x < jpeg_decoder.output_width; x++) {
            unsigned int luminance = 0;
            if (channel_count >= 3) {
                unsigned char red = source[x * (size_t)channel_count + 0];
                unsigned char green = source[x * (size_t)channel_count + 1];
                unsigned char blue = source[x * (size_t)channel_count + 2];
                luminance = (299U * red + 587U * green + 114U * blue) / 1000U;
            } else {
                luminance = source[x * (size_t)channel_count + 0];
            }

            image->pixels[y * jpeg_decoder.output_width + x] = (unsigned char)luminance;
        }
        y++;
    }

    jpeg_finish_decompress(&jpeg_decoder);
    jpeg_destroy_decompress(&jpeg_decoder);
    fclose(file);
    return 1;
}

int fib_image_load(const char *path, FibImage *image) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "error: cannot open file %s\n", path);
        return 0;
    }

    unsigned char header[8] = {0};
    size_t bytes_read = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (bytes_read >= 8 && png_sig_cmp(header, 0, 8) == 0) {
        return read_png_image(path, image);
    }
    if (bytes_read >= 3 && header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
        return read_jpeg_image(path, image);
    }

    fprintf(stderr, "error: unsupported format, use png/jpg/jpeg\n");
    return 0;
}
