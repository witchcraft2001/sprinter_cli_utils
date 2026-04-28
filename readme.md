# Sprinter DSS Utilities

Author: Dmitry Mikhalchenkov, Sprinter Team

This repository contains command-line utilities for Sprinter DSS, implemented in
C for the SDCC Sprinter SDK. The utilities are intended for real Sprinter/DSS
use and for emulator-based testing through packaged disk images.

## Utilities

- `make` - compact `make` implementation with variables, explicit rules,
  recipe prefixes, `.PHONY`, timestamp checks and DSS command execution.
- `diff` - compact text file comparison utility with normal and unified diff
  output, binary detection and common ignore options.
- `deltree` - recursive tree deletion utility with confirmation, wildcard
  support and attribute clearing before removal.
- `xcopy` - file and directory copy utility using Sprinter paged RAM for larger
  I/O buffers.

Each utility has its own source directory, local `Makefile`, user help text and
implementation notes.

## Build

Build an individual utility from its directory:

```sh
make -C make
make -C diff
make -C deltree
make -C xcopy
```

Create the FAT12 image and ZIP distribution from the repository root:

```sh
./run/create_floppy_image.sh
```

The packaging script rebuilds the utilities and creates:

- `build/utils.img`
- `dist/utils.zip`

## Repository Layout

- `make/`, `diff/`, `deltree/`, `xcopy/` - utility sources, docs and examples.
- `run/create_floppy_image.sh` - build and packaging script.
- `build/` - generated disk image and temporary packaging tree.
- `dist/` - generated ZIP distribution.
- `references/` - reference materials used for implementation research.

## License

Copyright (C) Dmitry Mikhalchenkov, Sprinter Team.

This project is licensed under the GNU General Public License version 3 or
later (`GPL-3.0-or-later`). The license requires that redistributed binaries or
derived versions remain under the same copyleft terms, that corresponding source
code is made available, and that the original copyright and authorship notices
are preserved.

See `LICENSE.md` for the repository license notice.
