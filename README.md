# PNGTOASCII

`fib` converts png/jpg/jpeg images into terminal-friendly ascii art.

## Features

- png input support (grayscale, rgb, rgba, palette)
- jpeg input support (jpg/jpeg)
- adaptive tone expansion for low-contrast scenes
- edge-aware glyph placement (`/`, `\`, `|`, `-`) for clearer structure
- floyd-steinberg-style error diffusion for more visible tonal separation
- fast summed-area downsampling for cleaner output
- Deterministic test fixtures and automated test Makefile
- Demo generation targets for recording/sharing

## Build

```bash
make build
```

## Run

```bash
./fib <input.(png|jpg|jpeg)> [output_width] [output_height] [output.txt]
```

Examples:

```bash
./fib tests/fixtures/gradient.png 96 40
./fib tests/fixtures/stripes.png 48 12 demo/stripes_ascii.txt
```

## Tests

```bash
make test
```

this runs deterministic fixture generation and validates expected ascii output for png and jpg paths.

```bash
make memcheck
```

this runs the same tests with address/undefined sanitizers enabled.

## optional: download extra png test images

```bash
make fetch-images
```

Downloads go to `tests/fixtures/downloaded/`.

## Demo outputs (recording-ready)

```bash
make demo
```

Generated files:

- `demo/gradient_ascii.txt`
- `demo/stripes_ascii.txt`
- `demo/white_jpg_ascii.txt`
- `demo/checker_ascii.txt`
- `demo/radial_ascii.txt`
