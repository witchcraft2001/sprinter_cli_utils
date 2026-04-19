# diff examples

## 01_identical

Two equal files for exit code `0` and `-s` checks.

- quick run: `CHECK.BAT`

## 02_insert_delete

Files with added and deleted lines.

- quick run: `CHECK.BAT`

## 03_change_blocks

Files with changed line blocks.

- quick run: `CHECK.BAT`

## 04_ignore_case

Case-only differences.

- `diff A.TXT B.TXT` -> differences
- `diff -i A.TXT B.TXT` -> identical

- quick run: `CHECK.BAT`

## 05_ignore_space_change

Different amount of spaces/tabs between tokens.

- `diff A.TXT B.TXT` -> differences
- `diff -b A.TXT B.TXT` -> identical

- quick run: `CHECK.BAT`

## 06_ignore_all_space

Whitespace removed/inserted completely.

- `diff A.TXT B.TXT` -> differences
- `diff -w A.TXT B.TXT` -> identical

- quick run: `CHECK.BAT`
