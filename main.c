#include "fib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fib_render.h"

#define FIB_MAX_OUTPUT_DIMENSION 1000

typedef struct {
    int has_value;
    int value;
} ParsedInt;

static int is_help_arg(const char *arg) {
    return strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0;
}

static int is_version_arg(const char *arg) {
    return strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0;
}

static int parse_positive_int(const char *value, ParsedInt *parsed) {
    char *end_ptr = NULL;
    long parsed_value = strtol(value, &end_ptr, 10);
    if (value[0] == '\0' || end_ptr == value || *end_ptr != '\0') {
        return 0;
    }
    if (parsed_value <= 0 || parsed_value > FIB_MAX_OUTPUT_DIMENSION) {
        return 0;
    }
    parsed->has_value = 1;
    parsed->value = (int)parsed_value;
    return 1;
}

static int parse_color_mode(const char *value, FibColorMode *mode_out) {
    if (strcmp(value, "auto") == 0) {
        *mode_out = FIB_COLOR_AUTO;
        return 1;
    }
    if (strcmp(value, "always") == 0) {
        *mode_out = FIB_COLOR_ALWAYS;
        return 1;
    }
    if (strcmp(value, "never") == 0) {
        *mode_out = FIB_COLOR_NEVER;
        return 1;
    }
    return 0;
}

static int parse_cli_args(int argc,
                          char *argv[],
                          FibRenderConfig *config,
                          const char **input_path,
                          const char **output_path) {
    int index = 1;
    int positional_count = 0;
    ParsedInt width = {0};
    ParsedInt height = {0};

    config->output_width = FIB_DEFAULT_OUTPUT_WIDTH;
    config->output_height = FIB_DEFAULT_OUTPUT_HEIGHT;
    config->color_mode = FIB_COLOR_AUTO;
    config->enable_color = 0;
    config->palette = FIB_PALETTE_CLASSIC;
    *input_path = NULL;
    *output_path = NULL;

    if (argc < 2) {
        return 0;
    }

    while (index < argc) {
        const char *arg = argv[index];

        if (strcmp(arg, "--ansi") == 0) {
            config->color_mode = FIB_COLOR_ALWAYS;
            index++;
            continue;
        }
        if (strcmp(arg, "--no-ansi") == 0) {
            config->color_mode = FIB_COLOR_NEVER;
            index++;
            continue;
        }
        if (strcmp(arg, "--color") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "error: --color requires a value (auto|always|never)\n");
                return 0;
            }
            if (!parse_color_mode(argv[index + 1], &config->color_mode)) {
                fprintf(stderr, "error: invalid --color value '%s' (use auto|always|never)\n", argv[index + 1]);
                return 0;
            }
            index += 2;
            continue;
        }
        if (strcmp(arg, "--palette") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "error: --palette requires a value\n");
                return 0;
            }
            if (!fib_palette_from_string(argv[index + 1], &config->palette)) {
                fprintf(stderr, "error: invalid palette '%s' (use classic|smooth|blocks)\n", argv[index + 1]);
                return 0;
            }
            index += 2;
            continue;
        }

        if (strncmp(arg, "--", 2) == 0) {
            fprintf(stderr, "error: unknown option %s\n", arg);
            return 0;
        }

        if (positional_count == 0) {
            *input_path = arg;
        } else if (positional_count == 1) {
            if (!parse_positive_int(arg, &width)) {
                fprintf(stderr, "error: invalid output_width '%s' (1..%d)\n", arg, FIB_MAX_OUTPUT_DIMENSION);
                return 0;
            }
        } else if (positional_count == 2) {
            if (!parse_positive_int(arg, &height)) {
                fprintf(stderr, "error: invalid output_height '%s' (1..%d)\n", arg, FIB_MAX_OUTPUT_DIMENSION);
                return 0;
            }
        } else if (positional_count == 3) {
            *output_path = arg;
        } else {
            fprintf(stderr, "error: too many positional arguments\n");
            return 0;
        }

        positional_count++;
        index++;
    }

    if (!*input_path) {
        return 0;
    }

    if (width.has_value) {
        config->output_width = width.value;
    }
    if (height.has_value) {
        config->output_height = height.value;
    }

    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || is_help_arg(argv[1])) {
        fib_print_usage(argv[0]);
        return (argc < 2) ? 1 : 0;
    }
    if (is_version_arg(argv[1])) {
        puts("fib 1.0.0");
        return 0;
    }

    FibRenderConfig config;
    const char *input_path = NULL;
    const char *output_path = NULL;

    if (!parse_cli_args(argc, argv, &config, &input_path, &output_path)) {
        fib_print_usage(argv[0]);
        return 1;
    }

    return fib_run(input_path, &config, output_path);
}
