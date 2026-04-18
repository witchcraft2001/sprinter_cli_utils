# DSS Runtime Examples

This directory contains practical examples for `make.exe` on Sprinter DSS.

## Layout

- `tools/mkstamp` - helper tool that creates/updates a file (used in recipes)
- `tools/mkfail` - helper tool that exits with a non-zero code (used to test `-` prefix)
- `01_basic/Makefile` - dependencies + variables + `.PHONY`
- `02_prefixes/Makefile` - `@`, `-`, and `-n` dry-run behavior

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
