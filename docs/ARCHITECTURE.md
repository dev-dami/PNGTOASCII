# Architecture

`fib` is organized into focused C modules to keep responsibilities explicit.

- `main.c`: CLI parsing and process exit control
- `fib.c`: runtime orchestration and terminal color decision policy
- `fib_image.c` / `fib_image.h`: PNG/JPEG decode path and grayscale conversion
- `fib_render.c` / `fib_render.h`: downsampling, edge-aware glyph selection, dithering, and ANSI line emission

This split makes changes safer: CLI changes do not touch decoders, and render changes do not alter argument parsing.
