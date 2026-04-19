Sprinter diff for Estex DSS

Status
- Level 1 MVP (initial)

Usage
- diff [OPTION]... FILE1 FILE2

Options
- -q        report only whether files differ
- -s        report when files are identical
- -H -h /?  show help

Exit codes
- 0  files are identical
- 1  files are different
- 2  error (bad args, file read error, limits exceeded)

Current MVP limits
- Max lines per file: 64
- Max line length: 160 characters
- Shared text memory pool: 4096 bytes

Notes
- This is a text diff utility.
- Output format by default is normal diff (a/d/c blocks).
- Future levels add unified format (-u), ignore-space/case, and directory mode.
