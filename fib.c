#include "fib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fib_image.h"
#include "fib_render.h"

void fib_print_usage(const char *program_name) {
    printf("usage: %s [--ansi|--no-ansi|--color auto|always|never] [--palette classic|smooth|blocks] <input.(png|jpg|jpeg)> [output_width] [output_height] [output.txt]\n",
           program_name);
    printf("  --ansi         : force ANSI color output (compat alias for --color always)\n");
    printf("  --no-ansi      : disable ANSI color output (compat alias for --color never)\n");
    printf("  --color        : color mode (auto, always, never)\n");
    printf("  --palette      : shading profile (classic, smooth, blocks)\n");
    printf("  input          : input image file (png/jpg/jpeg)\n");
    printf("  output_width   : output width in chars (default: %d)\n", FIB_DEFAULT_OUTPUT_WIDTH);
    printf("  output_height  : output height in lines (default: %d)\n", FIB_DEFAULT_OUTPUT_HEIGHT);
    printf("  output.txt     : output file (default: stdout)\n");
    printf("  --version      : print version and exit\n");
}

static int env_truthy(const char *name) {
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return 0;
    }
    if (strcmp(value, "0") == 0) {
        return 0;
    }
    return 1;
}

static int should_enable_color(FibColorMode mode, int has_output_path, FILE *output) {
    if (mode == FIB_COLOR_ALWAYS) {
        return 1;
    }
    if (mode == FIB_COLOR_NEVER) {
        return 0;
    }

    if (getenv("NO_COLOR")) {
        return 0;
    }
    if (env_truthy("CLICOLOR_FORCE")) {
        return 1;
    }
    if (has_output_path) {
        return 0;
    }

    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0) {
        return 0;
    }
    (void)output;
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }
    if (getenv("CLICOLOR") && strcmp(getenv("CLICOLOR"), "0") == 0) {
        return 0;
    }
    return 1;
}

int fib_run(const char *input_path, const FibRenderConfig *config, const char *output_path) {
    FibRenderConfig runtime_config = *config;
    FibImage image = {0};
    if (!fib_image_load(input_path, &image)) {
        return 1;
    }

    FILE *output = stdout;
    if (output_path) {
        output = fopen(output_path, "w");
        if (!output) {
            fprintf(stderr, "error: cannot create output file %s\n", output_path);
            fib_image_free(&image);
            return 1;
        }
    }

    runtime_config.enable_color = should_enable_color(runtime_config.color_mode, output_path != NULL, output);

    fib_render_ascii(&image, &runtime_config, output);

    if (output_path) {
        fclose(output);
        printf("ascii art saved to: %s\n", output_path);
    }

    fib_image_free(&image);
    return 0;
}
