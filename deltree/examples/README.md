# DELTREE examples for Sprinter DSS

Current package includes manual scenarios for recursive removal and wildcard masks.
Both scenarios use `MKSTAMP.EXE` to create files (no shell redirection required).

## 01_BASIC

Creates a nested directory tree with files, marks one file as read-only,
and verifies that `DELTREE` removes the full tree after confirmation.

## 02_WILDCARDS

Creates files and subdirectories under `WTEST`, then checks:
- `DELTREE /Y WTEST\\*.BAK` removes only matching files
- `DELTREE /Y WTEST\\???.TMP` removes 3-char `.TMP` names
- `DELTREE /Y WTEST\\*` removes all content inside `WTEST`
