# Utils Notes

- After every iteration of code changes, rebuild the affected artifacts before finishing the task.
- For utility changes, rebuild the modified utility with its local `Makefile`.
- When changes affect packaged outputs or release contents, regenerate `build/utils.img` and `dist/utils.zip` via `run/create_floppy_image.sh`.
- If a rebuild fails, report the failure clearly and do not present the iteration as complete.

## External reference sources
- You may consult the following local sibling repositories/directories for answers, platform details, and implementation ideas:
  - `/Users/dmitry/dev/zx/sprinter/sdcc-sprinter-sdk`
  - `/Users/dmitry/dev/zx/sprinter/sprinter_bios`
  - `/Users/dmitry/dev/zx/sprinter/Estex-DSS/`
  - `/Users/dmitry/dev/zx/sprinter/sprinter_ai_doc/manual`
  - `/Users/dmitry/dev/zx/sprinter/sources/tasm_071/TASM`
  - `/Users/dmitry/dev/zx/sprinter/sources/fformat/src/fformat_v113`
  - `/Users/dmitry/dev/zx/sprinter/sources/fm/FM-SRC/FM`
- Treat them as reference material only; this repository remains the source of truth for changes you make here.

## Commit Message Notes
- When preparing a commit comment/message, use the format `type(scope): summary`.
- Example: `feat(diff): handle Ctrl+X/Ctrl+Z abort via DSS scan codes`
