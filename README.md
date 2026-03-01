# PNGTOASCII (`fib`)

`fib` converts PNG/JPG/JPEG images into high-quality, terminal-friendly ASCII art.

## Highlights

- Supports PNG (grayscale, RGB, RGBA, palette) and JPEG inputs
- Adaptive tone expansion and edge-aware glyph selection for improved structure
- Error-diffusion rendering for stronger tonal separation
- Summed-area downsampling for stable detail at smaller output sizes
- Production terminal color policy: `--color auto|always|never`, plus `--ansi` / `--no-ansi` aliases
- Multiple shading profiles: `classic`, `smooth`, `blocks`
- Deterministic fixture-based tests and sanitizer test path

## Project Structure

- `main.c`: CLI entrypoint and argument parsing
- `fib.c`: application orchestration and runtime color policy
- `fib_image.c` / `fib_image.h`: image loading and decoding (`libpng`, `libjpeg`)
- `fib_render.c` / `fib_render.h`: ASCII rendering, palette logic, and ANSI output
- `tests/`: deterministic fixture generation and regression tests

## Build

```bash
make build
```

## Usage

```bash
./fib [--ansi|--no-ansi|--color auto|always|never] [--palette classic|smooth|blocks] <input.(png|jpg|jpeg)> [output_width] [output_height] [output.txt]
```

### Options

- `--color auto|always|never`: terminal color policy
- `--ansi`: alias for `--color always`
- `--no-ansi`: alias for `--color never`
- `--palette classic|smooth|blocks`: shading profile
- `-h, --help`: print usage
- `-V, --version`: print version

### Examples

```bash
./fib tests/fixtures/gradient.png 96 40
./fib tests/fixtures/stripes.png 48 12 demo/stripes_ascii.txt
./fib --ansi --palette smooth tests/fixtures/checker.png 96 32
./fib --color always --palette blocks tests/fixtures/radial.png 90 40
```

## Testing

```bash
make test
```

Runs fixture generation and regression checks for PNG/JPEG parity, depth/edge quality, and terminal CLI behavior.

```bash
make memcheck
```

Runs the same suite with AddressSanitizer/UndefinedBehaviorSanitizer enabled.
