# DSS Runtime Examples

This directory contains practical examples for `make.exe` on Sprinter DSS.

## Layout

- `tools/mkstamp` - helper tool that creates/updates a file (used in recipes)
- `tools/mkfail` - helper tool that exits with a non-zero code (used to test `-` prefix)
- `tools/mkdel` - helper tool that deletes a file (used in clean recipes)
- `01_basic/Makefile` - dependencies + variables + `.PHONY`
- `02_prefixes/Makefile` - `@`, `-`, and `-n` dry-run behavior
- `03_multifile/Makefile` - multi-file dependency tracking and selective rebuilds
- `04_solidc/Makefile` - SoliD-C toolchain build flow (`cc1` -> `cc2` -> `as` -> `ld`)

## Build helper tools

Build on host (GNU make + SDCC SDK):

```bash
make -C tools/mkstamp
make -C tools/mkfail
```

Copy resulting `mkstamp.exe` and `mkfail.exe` to target DSS disk together with the example Makefiles and `make.exe`.

## Run on DSS

From the example directory:

- default target:
  - `make`
- dry run:
  - `make -n`
- explicit file:
  - `make -f Makefile`
- explicit goal:
  - `make clean`

## Suggested checks for 03_multifile

From `03_multifile` directory on DSS:

1. First build:
   - `make`
2. No-op second build (everything up to date):
   - `make`
3. Update only one source and verify partial rebuild:
   - `make touch_a`
   - `make`
4. Update another source and verify partial rebuild:
   - `make touch_b`
   - `make`

## SoliD-C build example (04_solidc)

The source file name is controlled by variable `NAME`:

- `NAME = hello`
- `SRC = $(NAME).c`

To build:

- `make`

To build another source, change only `NAME` in `Makefile`.
