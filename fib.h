#ifndef FIB_H
#define FIB_H

#include "fib_render.h"

int fib_run(const char *input_path, const FibRenderConfig *config, const char *output_path);
void fib_print_usage(const char *program_name);

#endif
