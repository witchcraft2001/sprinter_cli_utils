# make for Sprinter DSS

Compact `make` implementation for Estex DSS, written in C with SDCC Sprinter SDK.

Current stable status: Level 1 MVP.

## Implemented (Level 1)

- Variables: `=` and `:=`
- Variable references: `$(NAME)`
- Explicit rules: `target: dep dep`
- Recipe prefixes: `@` (silent) and `-` (ignore error)
- `.PHONY`
- Timestamp rebuild checks using FAT file date/time
- CLI options: `-f <file>`, `-n`, `target`
- Help: `/?`, `-h`, `-H`
- GNU-style up-to-date message:
  - `make: Nothing to be done for \`all'.`

## Not implemented yet

- Pattern rules (`%.o: %.c`)
- Automatic variables (`$@`, `$<`, `$^`)
- GNU functions (`wildcard`, `patsubst`)
- include/conditionals and advanced GNU make semantics

## Build

From `utils/make`:

```bash
make
```

Output: `make.exe`.

Useful build flags:

```bash
make VERSION=0.1.20260418
make LOG=1
make SDK_DIR=/absolute/path/to/sdcc-sprinter-sdk/
```

## Run on DSS

```text
make [-n] [-f FILE] [target]
```

Default makefile lookup order:

1. `Makefile`
2. `makefile`
3. `MAKEFILE`

## PATH and utility placement on DSS

`make.exe` runs recipe commands through DSS `EXEC`, so command lookup depends on current directory and `PATH`.

For stable usage on Sprinter DSS, use one of these setups:

1. Place `MAKE.EXE` and required runtime helpers in a system directory already available to your command environment.
2. Or add the `MAKE` tools directory to `PATH` and keep helper executables there.

Runtime note:

- `MKDEL.EXE` is used by provided `clean` examples and can be useful in real projects.
- `MKSTAMP.EXE` and `MKFAIL.EXE` are test/training helpers used only by `examples/*` scenarios; they are not required for production makefiles.

If a helper tool is missing from the current directory and not reachable via `PATH`, recipes will fail with `cannot exec` errors.

## Examples

See `examples/README.md`.

## Packaging

From `utils`:

```bash
./run/create_floppy_image.sh
```

This script now builds:

- FAT12 image: `build/utils.img`
- ZIP package (store mode): `dist/utils.zip`

Both packages use the same runtime folder layout for DSS testing.
