#ifndef FIB_RENDER_H
#define FIB_RENDER_H

#include <stdio.h>

#include "fib_image.h"

#define FIB_DEFAULT_OUTPUT_WIDTH 80
#define FIB_DEFAULT_OUTPUT_HEIGHT 40

typedef enum {
    FIB_PALETTE_CLASSIC = 0,
    FIB_PALETTE_SMOOTH,
    FIB_PALETTE_BLOCKS
} FibPalette;

typedef enum {
    FIB_COLOR_AUTO = 0,
    FIB_COLOR_ALWAYS,
    FIB_COLOR_NEVER
} FibColorMode;

typedef struct {
    int output_width;
    int output_height;
    FibColorMode color_mode;
    int enable_color;
    FibPalette palette;
} FibRenderConfig;

void fib_render_ascii(const FibImage *image, const FibRenderConfig *config, FILE *output);
const char *fib_palette_name(FibPalette palette);
int fib_palette_from_string(const char *value, FibPalette *palette_out);

#endif
