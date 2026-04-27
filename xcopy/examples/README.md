# xcopy examples

## 01_verfy

Verification scenario for copying a prepared tree from `B:` to `C:`.

- source tree contains nested directories, an empty directory and a file that gets `+H +S +R`
- `CHECK.BAT` runs `XCOPY B:\... C:\XCOPYCHK /Y /E /H /K`
- copied files are verified with `DIFF -s`

Quick run on DSS from the example directory:

- `CHECK.BAT`
