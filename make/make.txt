make for Sprinter DSS
=====================

Compact make implementation for Estex DSS, written in C with SDCC Sprinter SDK.

Current stable status: Level 1 MVP.

Implemented
---------------------

- Variables: = and :=
- Variable references: $(NAME)
- Explicit rules: target: dep dep
- Recipe prefixes: @ (silent) and - (ignore error)
- .PHONY
- Timestamp rebuild checks via FAT date/time
- CLI: -f <file>, -n, target
- Help: /?, -h, -H
- Up-to-date message:
  make: Nothing to be done for `all'.

Runtime binary
--------------

Main program file on DSS/Sprinter:

  MAKE.EXE

Run on DSS
----------

  make [-n] [-f FILE] [target]

Default makefile lookup order:
  1) Makefile
  2) makefile
  3) MAKEFILE

PATH and utility placement on DSS
-------------------------------

MAKE.EXE runs recipe commands through DSS EXEC, so command lookup depends on
current directory and PATH.

Recommended setup on Sprinter DSS:

1) Put MAKE.EXE and required runtime helpers into a system directory
   available from your command environment.
2) Or add the MAKE tools directory to PATH.

Runtime note:

- MKDEL.EXE is used by provided clean examples and may be useful in real
  projects.
- MKSTAMP.EXE and MKFAIL.EXE are test/training helpers for examples/* only.
  They are not required for production makefiles.

If helper tools are not in current directory and not reachable through PATH,
recipes fail with "cannot exec".

Examples
--------

See examples/README.md
