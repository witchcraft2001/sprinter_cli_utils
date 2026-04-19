# diff for Sprinter DSS

Compact `diff` implementation for Estex DSS, written in C with SDCC Sprinter SDK.

Current status: Level 1 MVP (initial).

## Implemented in MVP

- Compare two text files: `diff file1 file2`
- Default output format: normal diff (`a`, `d`, `c` hunks)
- Options:
  - `-q` brief output (report only if files differ)
  - `-s` report when files are identical
  - `-h`, `-H`, `/?` help
- Exit codes:
  - `0` identical
  - `1` different
  - `2` error

## Current limits

MVP uses fixed buffers for predictable memory usage on DSS:

- up to `64` lines per file
- up to `160` characters per line
- shared internal text pool: `4096` bytes

When limits are exceeded, the tool exits with code `2` and a clear error message.

## Build

From `utils/diff`:

```bash
make
```

Output: `diff.exe`.

Useful flags:

```bash
make VERSION=0.1.20260419
make LOG=1
make SDK_DIR=/absolute/path/to/sdcc-sprinter-sdk/
```

## Run on DSS

```text
diff [OPTION]... FILE1 FILE2
```

Examples:

```text
diff a.txt b.txt
diff -q a.txt b.txt
diff -s a.txt b.txt
```

## Roadmap

See `specs.md` for staged plan (`-u`, whitespace/case ignore, directory mode).
