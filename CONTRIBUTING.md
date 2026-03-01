# Contributing

Thanks for contributing to PNGTOASCII.

## Development Setup

1. Install C toolchain and development headers for `libpng` and `libjpeg`.
2. Build locally:
   ```bash
   make build
   ```
3. Run the full quality suite:
   ```bash
   make test
   make memcheck
   ```

## Pull Request Guidelines

- Keep commits focused and reviewable.
- Preserve or improve test coverage for behavior changes.
- Update documentation when CLI flags or behavior changes.
- Keep output deterministic for fixture-based tests.
