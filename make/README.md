# make for Sprinter DSS (Iteration 1)

Small `make` implementation for Estex DSS, written in C with SDCC Sprinter SDK.

Current status: Level 1 MVP.

## Implemented

- Variables: `=` and `:=`
- Variable references: `$(NAME)`
- Explicit rules: `target: dep dep`
- Recipe prefixes: `@` and `-`
- `.PHONY`
- Timestamp-based rebuild using FAT date/time
- CLI flags: `-f <file>`, `-n`, goal target argument

## Not implemented yet

- Pattern rules (`%.o: %.c`)
- Automatic variables (`$@`, `$<`, `$^`)
- `$(wildcard ...)`, `$(patsubst ...)`

## Build

From this directory:

```bash
make
```

This produces `make.exe`.

If SDK is not at default relative path, pass:

```bash
make SDK_DIR=/absolute/path/to/sdcc-sprinter-sdk/
```

## Run on DSS

```text
make [-n] [-f FILE] [target]
```

Default makefile lookup: `Makefile`, then `makefile`.

## Examples

See `examples/README.md`.

## Floppy image for emulator

Create a boot/test floppy image with project utilities (currently only `make.exe`):

```bash
cd ..
./run/create_floppy_image.sh
```

Optional output image path:

```bash
./run/create_floppy_image.sh /absolute/path/to/utils.img
```
