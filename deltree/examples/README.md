# DELTREE examples for Sprinter DSS

Current package includes manual scenarios for recursive removal and wildcard masks.
Both scenarios use `MKSTAMP.EXE` to create files (no shell redirection required).
Options `/Y` and `-Y` are equivalent, same for `/Q` and `-Q`.

## 01_BASIC

Creates a nested directory tree with files, marks one file as read-only,
and verifies that `DELTREE` removes the full tree after confirmation.
You can also press `Esc`, `Ctrl+X` or `Ctrl+Z` during processing and expect:
`deltree: Aborted`

## 02_WILDCARDS

Creates files and subdirectories under `WTEST`, then checks:
- `DELTREE /Y WTEST\\*.BAK` (or `-Y`) removes only matching files
- `DELTREE /Y WTEST\\???.TMP` removes 3-char `.TMP` names
- `DELTREE /Y WTEST\\*` removes all content inside `WTEST`

## Quiet mode

Use `/Q` (or `-Q`) to disable per-directory progress lines:
- default mode prints `removing directory: <path>`
- quiet mode keeps only prompts/errors
