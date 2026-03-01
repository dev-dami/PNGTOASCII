# Terminal Color Policy

`fib` supports ANSI colorized output with deterministic policy rules.

## Modes

- `always`: always emit ANSI color sequences
- `never`: never emit ANSI color sequences
- `auto`: detect whether color is appropriate

## Auto Mode Rules

`auto` disables color when any of the following is true:

- `NO_COLOR` is present
- output is redirected to a file
- `TERM=dumb`
- output stream is not a TTY
- `CLICOLOR=0`

`auto` enables color when `CLICOLOR_FORCE` is truthy.
