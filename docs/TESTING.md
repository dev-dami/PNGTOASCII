# Testing

## Commands

```bash
make test
make memcheck
```

## Coverage Areas

- PNG/JPEG parity against fixture output
- Low-contrast depth and edge glyph behavior
- Terminal CLI color mode behavior (`always`, `never`, aliases, and auto/file handling)

`make memcheck` builds with ASAN/UBSAN and re-runs the full test suite.
