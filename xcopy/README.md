# xcopy for Sprinter DSS

Compact `xcopy` implementation for Estex DSS, written in C with SDCC Sprinter SDK.

Implemented switches:

- `/Y` overwrite without prompt
- `/-Y` force prompt before overwrite
- `/E` copy empty directories
- `/H` include hidden/system entries
- `/K` preserve file attributes
- `/V` show verbose progress while scanning directories

## Memory model

The copy engine uses paged Sprinter RAM through SDK `dss_getmem()` and
`dss_setwin()`.

- page size: `16 KB`
- window used for I/O buffer: `WIN3` (`0xC000-0xFFFF`)
- maximum paged buffer: `4` pages by default (`64 KB`)

Copy loop:

1. allocate up to 4 DSS pages
2. read source file page-by-page until the paged buffer is full or EOF is reached
3. write the accumulated buffer to destination
4. repeat until the file is copied completely

This reduces the number of DSS read/write calls compared to a small conventional buffer.

During long copies the utility polls the keyboard and aborts on `Esc`, `Ctrl+X`
or `Ctrl+Z`. Allocated DSS pages are always returned before process exit.

## Build

```sh
make -C xcopy
```

## Run on DSS

```text
XCOPY SOURCE DEST /Y
XCOPY SOURCE DEST /Y /E /H /K
```

When destination for a single source file is ambiguous and does not exist, `xcopy` asks whether it should be treated as a file or a directory.

At startup the utility prints only the program banner. The end-of-run statistics
include copied bytes; elapsed time and speed are reported as `n/a` until the DSS
time wrapper is made safe for this target.

## Example

See `examples/README.md`.
