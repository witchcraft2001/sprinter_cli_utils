# diff for Sprinter DSS

Compact `diff` implementation for Estex DSS, written in C with SDCC Sprinter SDK.

Current status: Level 1 MVP (initial).

## Implemented in MVP

- Compare two text files: `diff file1 file2`
- Default output format: normal diff (`a`, `d`, `c` hunks)
- Options:
  - `-q` brief output (report only if files differ)
  - `-s` report when files are identical
  - `-a` treat all files as text
  - `-i` ignore case differences
  - `-b` ignore changes in amount of spaces/tabs
  - `-w` ignore all spaces/tabs
  - `-u` unified diff output (default context 3)
  - `-U N` unified diff output with `N` context lines
  - `-o FILE` write diff output to file (DSS-friendly replacement for shell redirection)
  - `-h`, `-H`, `/?` help
- Exit codes:
  - `0` identical
  - `1` different
  - `2` error
- Automatic binary detection:
  - when binary content is detected: `Binary files A and B differ`
  - use `-a` to force text mode
  - for identical binary files, output is silent unless `-s` is used

## Current limits

MVP uses streaming comparison and supports large files (including `64KB+` text files) without loading full files into RAM.

- max line length: `255` characters
- lookahead window for resync: `8` lines

If a line is longer than the limit, the tool exits with code `2` and a clear error message.

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
diff -a a.txt b.txt
diff -i a.txt b.txt
diff -b a.txt b.txt
diff -w a.txt b.txt
diff -u a.txt b.txt
diff -U 5 a.txt b.txt
diff -o result.dif a.txt b.txt
```

## Roadmap

See `specs.md` for staged plan (`-u`, whitespace/case ignore, directory mode).
