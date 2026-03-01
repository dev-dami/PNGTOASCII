#include "fib_render.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char k_palette_classic[] =
    "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft()1{}[]?+~<>i!lI;:,\"^`'. ";

static const char k_palette_smooth[] =
    "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?+~<>i!lI;:,\"^`'. ";

static const char k_palette_blocks[] =
    "@%#*+=-:. ";

static int safe_multiply_size(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > SIZE_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static const char *palette_chars(FibPalette palette) {
    switch (palette) {
        case FIB_PALETTE_SMOOTH:
            return k_palette_smooth;
        case FIB_PALETTE_BLOCKS:
            return k_palette_blocks;
        case FIB_PALETTE_CLASSIC:
        default:
            return k_palette_classic;
    }
}

const char *fib_palette_name(FibPalette palette) {
    switch (palette) {
        case FIB_PALETTE_SMOOTH:
            return "smooth";
        case FIB_PALETTE_BLOCKS:
            return "blocks";
        case FIB_PALETTE_CLASSIC:
        default:
            return "classic";
    }
}

int fib_palette_from_string(const char *value, FibPalette *palette_out) {
    if (strcmp(value, "classic") == 0) {
        *palette_out = FIB_PALETTE_CLASSIC;
        return 1;
    }
    if (strcmp(value, "smooth") == 0) {
        *palette_out = FIB_PALETTE_SMOOTH;
        return 1;
    }
    if (strcmp(value, "blocks") == 0) {
        *palette_out = FIB_PALETTE_BLOCKS;
        return 1;
    }
    return 0;
}

static unsigned char quantized_value_to_u8(int quantized_index, int quantized_count) {
    if (quantized_index < 0) {
        quantized_index = 0;
    }
    if (quantized_index >= quantized_count) {
        quantized_index = quantized_count - 1;
    }
    return (unsigned char)((quantized_index * 255) / (quantized_count - 1));
}

static unsigned char apply_palette_tone_curve(unsigned char value, FibPalette palette) {
    int adjusted = (int)value;

    if (palette == FIB_PALETTE_SMOOTH) {
        adjusted = (adjusted * 220 + ((adjusted * adjusted) / 255) * 35) / 255;
    } else if (palette == FIB_PALETTE_BLOCKS) {
        adjusted = (adjusted * 165 + ((adjusted * adjusted) / 255) * 90) / 255;
    } else {
        adjusted = (adjusted * 180 + ((adjusted * adjusted) / 255) * 75) / 255;
    }

    if (adjusted < 0) {
        adjusted = 0;
    }
    if (adjusted > 255) {
        adjusted = 255;
    }
    return (unsigned char)adjusted;
}

static void build_tone_lookup_table(const FibImage *image, FibPalette palette, unsigned char tone_lookup[256]) {
    uint32_t histogram[256] = {0};
    size_t pixel_count = (size_t)image->width * (size_t)image->height;

    for (size_t i = 0; i < pixel_count; i++) {
        histogram[image->pixels[i]]++;
    }

    uint64_t low_threshold = (uint64_t)pixel_count / 100U;
    uint64_t high_threshold = ((uint64_t)pixel_count * 99U) / 100U;
    uint64_t accumulator = 0;
    int low = 0;
    int high = 255;

    for (int i = 0; i < 256; i++) {
        accumulator += histogram[i];
        if (accumulator >= low_threshold) {
            low = i;
            break;
        }
    }

    accumulator = 0;
    for (int i = 0; i < 256; i++) {
        accumulator += histogram[i];
        if (accumulator >= high_threshold) {
            high = i;
            break;
        }
    }

    if (high <= low) {
        low = 0;
        high = 255;
    }

    int range = high - low;
    for (int i = 0; i < 256; i++) {
        int normalized = 0;
        if (i <= low) {
            normalized = 0;
        } else if (i >= high) {
            normalized = 255;
        } else {
            normalized = ((i - low) * 255) / range;
        }
        tone_lookup[i] = apply_palette_tone_curve((unsigned char)normalized, palette);
    }
}

static int build_summed_area_tables(const FibImage *image, uint64_t **sum_area_out, uint64_t **sum_square_out, size_t *stride_out) {
    size_t stride = (size_t)image->width + 1U;
    size_t row_count = (size_t)image->height + 1U;
    size_t cell_count = 0;
    uint64_t *sum_area = NULL;
    uint64_t *sum_square = NULL;

    if (!safe_multiply_size(stride, row_count, &cell_count) || cell_count > SIZE_MAX / sizeof(uint64_t)) {
        return 0;
    }

    sum_area = (uint64_t *)calloc(cell_count, sizeof(uint64_t));
    sum_square = (uint64_t *)calloc(cell_count, sizeof(uint64_t));
    if (!sum_area || !sum_square) {
        free(sum_area);
        free(sum_square);
        return 0;
    }

    for (int y = 1; y <= image->height; y++) {
        uint64_t row_sum = 0;
        uint64_t row_square_sum = 0;
        const unsigned char *source = image->pixels + (size_t)(y - 1) * (size_t)image->width;
        uint64_t *current_sum = sum_area + (size_t)y * stride;
        uint64_t *previous_sum = sum_area + (size_t)(y - 1) * stride;
        uint64_t *current_square = sum_square + (size_t)y * stride;
        uint64_t *previous_square = sum_square + (size_t)(y - 1) * stride;

        for (int x = 1; x <= image->width; x++) {
            uint64_t value = source[x - 1];
            row_sum += value;
            row_square_sum += value * value;
            current_sum[x] = previous_sum[x] + row_sum;
            current_square[x] = previous_square[x] + row_square_sum;
        }
    }

    *sum_area_out = sum_area;
    *sum_square_out = sum_square;
    *stride_out = stride;
    return 1;
}

static uint64_t summed_area_block(const uint64_t *table, size_t stride, int x0, int y0, int x1, int y1) {
    size_t sx0 = (size_t)x0;
    size_t sx1 = (size_t)x1;
    size_t sy0 = (size_t)y0;
    size_t sy1 = (size_t)y1;

    return table[sy1 * stride + sx1] - table[sy0 * stride + sx1] - table[sy1 * stride + sx0] + table[sy0 * stride + sx0];
}

static unsigned char clamp_pixel(const FibImage *image, int x, int y) {
    if (x < 0) {
        x = 0;
    }
    if (x >= image->width) {
        x = image->width - 1;
    }
    if (y < 0) {
        y = 0;
    }
    if (y >= image->height) {
        y = image->height - 1;
    }
    return image->pixels[(size_t)y * (size_t)image->width + (size_t)x];
}

static char edge_character(int gradient_x, int gradient_y) {
    int abs_x = gradient_x < 0 ? -gradient_x : gradient_x;
    int abs_y = gradient_y < 0 ? -gradient_y : gradient_y;

    if (abs_x > abs_y * 2) {
        return '|';
    }
    if (abs_y > abs_x * 2) {
        return '-';
    }
    return (gradient_x ^ gradient_y) < 0 ? '/' : '\\';
}

static void write_line(FILE *output, const char *line_chars, const unsigned char *line_shades, int width, int enable_color) {
    if (!enable_color) {
        fwrite(line_chars, 1, (size_t)width, output);
        fputc('\n', output);
        return;
    }

    for (int x = 0; x < width; x++) {
        unsigned int shade = (unsigned int)line_shades[x];
        fprintf(output, "\x1b[38;2;%u;%u;%um%c", shade, shade, shade, line_chars[x]);
    }
    fputs("\x1b[0m\n", output);
}

void fib_render_ascii(const FibImage *image, const FibRenderConfig *config, FILE *output) {
    float scale_x = (float)image->width / (float)config->output_width;
    float scale_y = (float)image->height / (float)config->output_height;
    const char *glyph_palette = palette_chars(config->palette);
    size_t glyph_count = strlen(glyph_palette);
    unsigned char tone_lookup[256];
    uint64_t *sum_area = NULL;
    uint64_t *sum_square = NULL;
    size_t sat_stride = 0;
    int has_summed_area = build_summed_area_tables(image, &sum_area, &sum_square, &sat_stride);

    char *line_chars = (char *)malloc((size_t)config->output_width);
    unsigned char *line_shades = (unsigned char *)malloc((size_t)config->output_width);
    size_t error_buffer_size = 0;
    float *error_line_current = NULL;
    float *error_line_next = NULL;

    build_tone_lookup_table(image, config->palette, tone_lookup);

    if (safe_multiply_size((size_t)(config->output_width + 2), sizeof(float), &error_buffer_size)) {
        error_line_current = (float *)calloc((size_t)(config->output_width + 2), sizeof(float));
        error_line_next = (float *)calloc((size_t)(config->output_width + 2), sizeof(float));
    }

    int has_error_diffusion = (error_line_current != NULL && error_line_next != NULL);
    int quantized_count = (int)glyph_count;
    if (quantized_count < 2) {
        quantized_count = 2;
    }

    for (int y = 0; y < config->output_height; y++) {
        int y0 = (int)(y * scale_y);
        int y1 = (int)((y + 1) * scale_y);

        if (y0 < 0) {
            y0 = 0;
        }
        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > image->height) {
            y1 = image->height;
        }
        if (y0 >= image->height) {
            y0 = image->height - 1;
            y1 = image->height;
        }

        if (has_error_diffusion) {
            memset(error_line_next, 0, error_buffer_size);
        }

        int left_to_right = ((y & 1) == 0);
        int x_start = left_to_right ? 0 : config->output_width - 1;
        int x_end = left_to_right ? config->output_width : -1;
        int x_step = left_to_right ? 1 : -1;

        for (int x = x_start; x != x_end; x += x_step) {
            int x0 = (int)(x * scale_x);
            int x1 = (int)((x + 1) * scale_x);

            if (x0 < 0) {
                x0 = 0;
            }
            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (x1 > image->width) {
                x1 = image->width;
            }
            if (x0 >= image->width) {
                x0 = image->width - 1;
                x1 = image->width;
            }

            uint64_t sample_count = (uint64_t)(x1 - x0) * (uint64_t)(y1 - y0);
            uint64_t sample_sum = 0;

            if (has_summed_area) {
                sample_sum = summed_area_block(sum_area, sat_stride, x0, y0, x1, y1);
            } else {
                for (int yy = y0; yy < y1; yy++) {
                    for (int xx = x0; xx < x1; xx++) {
                        sample_sum += image->pixels[(size_t)yy * (size_t)image->width + (size_t)xx];
                    }
                }
            }

            unsigned char average_value = sample_count ? (unsigned char)(sample_sum / sample_count) : 0;
            int cx = (x0 + x1) >> 1;
            int cy = (y0 + y1) >> 1;

            int gradient_x =
                -(int)clamp_pixel(image, cx - 1, cy - 1) - 2 * (int)clamp_pixel(image, cx - 1, cy) -
                (int)clamp_pixel(image, cx - 1, cy + 1) + (int)clamp_pixel(image, cx + 1, cy - 1) +
                2 * (int)clamp_pixel(image, cx + 1, cy) + (int)clamp_pixel(image, cx + 1, cy + 1);

            int gradient_y =
                -(int)clamp_pixel(image, cx - 1, cy - 1) - 2 * (int)clamp_pixel(image, cx, cy - 1) -
                (int)clamp_pixel(image, cx + 1, cy - 1) + (int)clamp_pixel(image, cx - 1, cy + 1) +
                2 * (int)clamp_pixel(image, cx, cy + 1) + (int)clamp_pixel(image, cx + 1, cy + 1);

            int gradient_magnitude = (gradient_x < 0 ? -gradient_x : gradient_x) +
                                     (gradient_y < 0 ? -gradient_y : gradient_y);

            int radius_x = (x1 - x0) * 2;
            int radius_y = (y1 - y0) * 2;
            if (radius_x < 1) {
                radius_x = 1;
            }
            if (radius_y < 1) {
                radius_y = 1;
            }

            int nx0 = cx - radius_x;
            int nx1 = cx + radius_x + 1;
            int ny0 = cy - radius_y;
            int ny1 = cy + radius_y + 1;

            if (nx0 < 0) {
                nx0 = 0;
            }
            if (ny0 < 0) {
                ny0 = 0;
            }
            if (nx1 > image->width) {
                nx1 = image->width;
            }
            if (ny1 > image->height) {
                ny1 = image->height;
            }

            uint64_t neighborhood_count = (uint64_t)(nx1 - nx0) * (uint64_t)(ny1 - ny0);
            uint64_t neighborhood_sum = 0;
            uint64_t neighborhood_square_sum = 0;

            if (has_summed_area && neighborhood_count > 0) {
                neighborhood_sum = summed_area_block(sum_area, sat_stride, nx0, ny0, nx1, ny1);
                neighborhood_square_sum = summed_area_block(sum_square, sat_stride, nx0, ny0, nx1, ny1);
            } else {
                for (int yy = ny0; yy < ny1; yy++) {
                    for (int xx = nx0; xx < nx1; xx++) {
                        uint64_t pixel_value = image->pixels[(size_t)yy * (size_t)image->width + (size_t)xx];
                        neighborhood_sum += pixel_value;
                        neighborhood_square_sum += pixel_value * pixel_value;
                    }
                }
            }

            int neighborhood_average = average_value;
            long double variance = 0.0L;
            if (neighborhood_count > 0) {
                long double mean = (long double)neighborhood_sum / (long double)neighborhood_count;
                neighborhood_average = (int)(mean + 0.5L);
                variance = (long double)neighborhood_square_sum / (long double)neighborhood_count - mean * mean;
                if (variance < 0.0L) {
                    variance = 0.0L;
                }
            }

            long double gain = 1.55L;
            if (variance < 180.0L) {
                gain = 2.35L;
            } else if (variance < 800.0L) {
                gain = 1.95L;
            }

            int local_value = (int)((long double)average_value + gain * ((long double)average_value - (long double)neighborhood_average));
            if (local_value < 0) {
                local_value = 0;
            }
            if (local_value > 255) {
                local_value = 255;
            }

            int error_index = x + 1;
            float tone_value = (float)tone_lookup[local_value];
            if (has_error_diffusion) {
                tone_value += error_line_current[error_index];
                if (tone_value < 0.0f) {
                    tone_value = 0.0f;
                }
                if (tone_value > 255.0f) {
                    tone_value = 255.0f;
                }
            }

            int edge_threshold = (variance < 220.0L) ? 76 : 116;
            char chosen_char;
            unsigned char shade_value = (unsigned char)local_value;

            if (gradient_magnitude > edge_threshold) {
                chosen_char = edge_character(gradient_x, gradient_y);
                if (has_error_diffusion) {
                    error_line_current[error_index] = 0.0f;
                }
            } else {
                int quantized_index = (int)((tone_value * (float)(quantized_count - 1)) / 255.0f + 0.5f);
                if (quantized_index < 0) {
                    quantized_index = 0;
                }
                if (quantized_index >= quantized_count) {
                    quantized_index = quantized_count - 1;
                }

                chosen_char = glyph_palette[quantized_index];
                shade_value = quantized_value_to_u8(quantized_index, quantized_count);

                if (has_error_diffusion) {
                    float quantization_error = tone_value - (float)shade_value;
                    if (left_to_right) {
                        error_line_current[error_index + 1] += quantization_error * (7.0f / 16.0f);
                        error_line_next[error_index - 1] += quantization_error * (3.0f / 16.0f);
                        error_line_next[error_index] += quantization_error * (5.0f / 16.0f);
                        error_line_next[error_index + 1] += quantization_error * (1.0f / 16.0f);
                    } else {
                        error_line_current[error_index - 1] += quantization_error * (7.0f / 16.0f);
                        error_line_next[error_index + 1] += quantization_error * (3.0f / 16.0f);
                        error_line_next[error_index] += quantization_error * (5.0f / 16.0f);
                        error_line_next[error_index - 1] += quantization_error * (1.0f / 16.0f);
                    }
                }
            }

            line_chars[x] = chosen_char;
            line_shades[x] = shade_value;
        }

        write_line(output, line_chars, line_shades, config->output_width, config->enable_color);

        if (has_error_diffusion) {
            float *tmp = error_line_current;
            error_line_current = error_line_next;
            error_line_next = tmp;
        }
    }

    free(sum_area);
    free(sum_square);
    free(error_line_current);
    free(error_line_next);
    free(line_chars);
    free(line_shades);
}
