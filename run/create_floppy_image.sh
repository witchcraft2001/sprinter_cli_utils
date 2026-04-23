#!/usr/bin/env bash
set -euo pipefail

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command not found: $1" >&2
    exit 1
  fi
}

need_cmd mformat
need_cmd mcopy
need_cmd mmd
need_cmd make
need_cmd zip

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

image_path="${1:-$repo_root/build/utils.img}"
dist_zip_path="${2:-$repo_root/dist/utils.zip}"
dist_root="$repo_root/build/zip_root"

mkdir -p "$repo_root/build"
mkdir -p "$repo_root/dist"
rm -f "$image_path"
rm -f "$dist_zip_path"
rm -rf "$dist_root"

utilities=(
  make
  diff
  deltree
)

MAKE_BUILD_LOG="${MAKE_BUILD_LOG:-0}"
MAKE_BUILD_VERSION="${MAKE_BUILD_VERSION:-}"

build_utility() {
  local utility="$1"

  case "$utility" in
    make)
      make -C "$repo_root/make" clean
      if [ -n "$MAKE_BUILD_VERSION" ]; then
        make -C "$repo_root/make" LOG="$MAKE_BUILD_LOG" VERSION="$MAKE_BUILD_VERSION"
      else
        make -C "$repo_root/make" LOG="$MAKE_BUILD_LOG"
      fi
      make -C "$repo_root/make/examples/tools/mkstamp"
      make -C "$repo_root/make/examples/tools/mkfail"
      make -C "$repo_root/make/examples/tools/mkdel"
      ;;
    diff)
      make -C "$repo_root/diff" clean
      if [ -n "$MAKE_BUILD_VERSION" ]; then
        make -C "$repo_root/diff" LOG="$MAKE_BUILD_LOG" VERSION="$MAKE_BUILD_VERSION"
      else
        make -C "$repo_root/diff" LOG="$MAKE_BUILD_LOG"
      fi
      ;;
    deltree)
      make -C "$repo_root/deltree" clean
      if [ -n "$MAKE_BUILD_VERSION" ]; then
        make -C "$repo_root/deltree" LOG="$MAKE_BUILD_LOG" VERSION="$MAKE_BUILD_VERSION"
      else
        make -C "$repo_root/deltree" LOG="$MAKE_BUILD_LOG"
      fi
      ;;
    *)
      echo "Error: unknown utility: $utility" >&2
      exit 1
      ;;
  esac
}

mkdir_img_dir() {
  local path="$1"
  mmd -i "$image_path" "$path" 2>/dev/null || true
}

copy_make_payload() {
  local utility_root="::/MAKE"
  local zip_root="$dist_root/MAKE"

  mkdir_img_dir "$utility_root"
  mkdir_img_dir "$utility_root/EXAMPLES"
  mkdir_img_dir "$utility_root/EXAMPLES/01_BASIC"
  mkdir_img_dir "$utility_root/EXAMPLES/02_PREFX"
  mkdir_img_dir "$utility_root/EXAMPLES/03_MULTI"
  mkdir_img_dir "$utility_root/EXAMPLES/04_SOLID"
  mkdir_img_dir "$utility_root/TOOLS"

  mkdir -p "$zip_root/EXAMPLES/01_BASIC"
  mkdir -p "$zip_root/EXAMPLES/02_PREFX"
  mkdir -p "$zip_root/EXAMPLES/03_MULTI"
  mkdir -p "$zip_root/EXAMPLES/04_SOLID"
  mkdir -p "$zip_root/TOOLS"

  mcopy -i "$image_path" -o "$repo_root/make/make.exe" "$utility_root/MAKE.EXE"
  cp "$repo_root/make/make.exe" "$zip_root/MAKE.EXE"
  cp "$repo_root/make/make.txt" "$zip_root/MAKE.TXT"

  mcopy -i "$image_path" -o "$repo_root/make/examples/README.md" "$utility_root/EXAMPLES/README.MD"
  mcopy -i "$image_path" -o "$repo_root/make/make.txt" "$utility_root/MAKE.TXT"
  mcopy -i "$image_path" -o "$repo_root/make/examples/01_basic/Makefile" "$utility_root/EXAMPLES/01_BASIC/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/02_prefixes/Makefile" "$utility_root/EXAMPLES/02_PREFX/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/03_multifile/Makefile" "$utility_root/EXAMPLES/03_MULTI/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/04_solidc/Makefile" "$utility_root/EXAMPLES/04_SOLID/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/04_solidc/hello.c" "$utility_root/EXAMPLES/04_SOLID/HELLO.C"
  cp "$repo_root/make/examples/README.md" "$zip_root/EXAMPLES/README.MD"
  cp "$repo_root/make/examples/01_basic/Makefile" "$zip_root/EXAMPLES/01_BASIC/MAKEFILE"
  cp "$repo_root/make/examples/02_prefixes/Makefile" "$zip_root/EXAMPLES/02_PREFX/MAKEFILE"
  cp "$repo_root/make/examples/03_multifile/Makefile" "$zip_root/EXAMPLES/03_MULTI/MAKEFILE"
  cp "$repo_root/make/examples/04_solidc/Makefile" "$zip_root/EXAMPLES/04_SOLID/MAKEFILE"
  cp "$repo_root/make/examples/04_solidc/hello.c" "$zip_root/EXAMPLES/04_SOLID/HELLO.C"

  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/TOOLS/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkfail/mkfail.exe" "$utility_root/TOOLS/MKFAIL.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkdel/mkdel.exe" "$utility_root/TOOLS/MKDEL.EXE"
  cp "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$zip_root/TOOLS/MKSTAMP.EXE"
  cp "$repo_root/make/examples/tools/mkfail/mkfail.exe" "$zip_root/TOOLS/MKFAIL.EXE"
  cp "$repo_root/make/examples/tools/mkdel/mkdel.exe" "$zip_root/TOOLS/MKDEL.EXE"

  # Duplicate helper tools into each example directory so examples can run in place.
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/EXAMPLES/01_BASIC/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/EXAMPLES/02_PREFX/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/EXAMPLES/03_MULTI/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkfail/mkfail.exe" "$utility_root/EXAMPLES/02_PREFX/MKFAIL.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkdel/mkdel.exe" "$utility_root/EXAMPLES/04_SOLID/MKDEL.EXE"
  cp "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$zip_root/EXAMPLES/01_BASIC/MKSTAMP.EXE"
  cp "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$zip_root/EXAMPLES/02_PREFX/MKSTAMP.EXE"
  cp "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$zip_root/EXAMPLES/03_MULTI/MKSTAMP.EXE"
  cp "$repo_root/make/examples/tools/mkfail/mkfail.exe" "$zip_root/EXAMPLES/02_PREFX/MKFAIL.EXE"
  cp "$repo_root/make/examples/tools/mkdel/mkdel.exe" "$zip_root/EXAMPLES/04_SOLID/MKDEL.EXE"
}

copy_diff_payload() {
  local utility_root="::/DIFF"
  local zip_root="$dist_root/DIFF"

  mkdir_img_dir "$utility_root"
  mkdir_img_dir "$utility_root/EXAMPLES"
  mkdir_img_dir "$utility_root/EXAMPLES/01_IDENT"
  mkdir_img_dir "$utility_root/EXAMPLES/02_INDEL"
  mkdir_img_dir "$utility_root/EXAMPLES/03_CHANG"
  mkdir_img_dir "$utility_root/EXAMPLES/04_IGNCS"
  mkdir_img_dir "$utility_root/EXAMPLES/05_IGNBC"
  mkdir_img_dir "$utility_root/EXAMPLES/06_IGNWS"

  mkdir -p "$zip_root/EXAMPLES/01_IDENT"
  mkdir -p "$zip_root/EXAMPLES/02_INDEL"
  mkdir -p "$zip_root/EXAMPLES/03_CHANG"
  mkdir -p "$zip_root/EXAMPLES/04_IGNCS"
  mkdir -p "$zip_root/EXAMPLES/05_IGNBC"
  mkdir -p "$zip_root/EXAMPLES/06_IGNWS"

  mcopy -i "$image_path" -o "$repo_root/diff/diff.exe" "$utility_root/DIFF.EXE"
  mcopy -i "$image_path" -o "$repo_root/diff/diff.txt" "$utility_root/DIFF.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/README.md" "$utility_root/EXAMPLES/README.MD"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/01_identical/A.TXT" "$utility_root/EXAMPLES/01_IDENT/A.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/01_identical/B.TXT" "$utility_root/EXAMPLES/01_IDENT/B.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/01_identical/CHECK.BAT" "$utility_root/EXAMPLES/01_IDENT/CHECK.BAT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/02_insert_delete/A.TXT" "$utility_root/EXAMPLES/02_INDEL/A.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/02_insert_delete/B.TXT" "$utility_root/EXAMPLES/02_INDEL/B.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/02_insert_delete/CHECK.BAT" "$utility_root/EXAMPLES/02_INDEL/CHECK.BAT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/03_change_blocks/A.TXT" "$utility_root/EXAMPLES/03_CHANG/A.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/03_change_blocks/B.TXT" "$utility_root/EXAMPLES/03_CHANG/B.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/03_change_blocks/CHECK.BAT" "$utility_root/EXAMPLES/03_CHANG/CHECK.BAT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/04_ignore_case/A.TXT" "$utility_root/EXAMPLES/04_IGNCS/A.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/04_ignore_case/B.TXT" "$utility_root/EXAMPLES/04_IGNCS/B.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/04_ignore_case/CHECK.BAT" "$utility_root/EXAMPLES/04_IGNCS/CHECK.BAT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/05_ignore_space_change/A.TXT" "$utility_root/EXAMPLES/05_IGNBC/A.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/05_ignore_space_change/B.TXT" "$utility_root/EXAMPLES/05_IGNBC/B.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/05_ignore_space_change/CHECK.BAT" "$utility_root/EXAMPLES/05_IGNBC/CHECK.BAT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/06_ignore_all_space/A.TXT" "$utility_root/EXAMPLES/06_IGNWS/A.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/06_ignore_all_space/B.TXT" "$utility_root/EXAMPLES/06_IGNWS/B.TXT"
  mcopy -i "$image_path" -o "$repo_root/diff/examples/06_ignore_all_space/CHECK.BAT" "$utility_root/EXAMPLES/06_IGNWS/CHECK.BAT"

  cp "$repo_root/diff/diff.exe" "$zip_root/DIFF.EXE"
  cp "$repo_root/diff/diff.txt" "$zip_root/DIFF.TXT"
  cp "$repo_root/diff/examples/README.md" "$zip_root/EXAMPLES/README.MD"
  cp "$repo_root/diff/examples/01_identical/A.TXT" "$zip_root/EXAMPLES/01_IDENT/A.TXT"
  cp "$repo_root/diff/examples/01_identical/B.TXT" "$zip_root/EXAMPLES/01_IDENT/B.TXT"
  cp "$repo_root/diff/examples/01_identical/CHECK.BAT" "$zip_root/EXAMPLES/01_IDENT/CHECK.BAT"
  cp "$repo_root/diff/examples/02_insert_delete/A.TXT" "$zip_root/EXAMPLES/02_INDEL/A.TXT"
  cp "$repo_root/diff/examples/02_insert_delete/B.TXT" "$zip_root/EXAMPLES/02_INDEL/B.TXT"
  cp "$repo_root/diff/examples/02_insert_delete/CHECK.BAT" "$zip_root/EXAMPLES/02_INDEL/CHECK.BAT"
  cp "$repo_root/diff/examples/03_change_blocks/A.TXT" "$zip_root/EXAMPLES/03_CHANG/A.TXT"
  cp "$repo_root/diff/examples/03_change_blocks/B.TXT" "$zip_root/EXAMPLES/03_CHANG/B.TXT"
  cp "$repo_root/diff/examples/03_change_blocks/CHECK.BAT" "$zip_root/EXAMPLES/03_CHANG/CHECK.BAT"
  cp "$repo_root/diff/examples/04_ignore_case/A.TXT" "$zip_root/EXAMPLES/04_IGNCS/A.TXT"
  cp "$repo_root/diff/examples/04_ignore_case/B.TXT" "$zip_root/EXAMPLES/04_IGNCS/B.TXT"
  cp "$repo_root/diff/examples/04_ignore_case/CHECK.BAT" "$zip_root/EXAMPLES/04_IGNCS/CHECK.BAT"
  cp "$repo_root/diff/examples/05_ignore_space_change/A.TXT" "$zip_root/EXAMPLES/05_IGNBC/A.TXT"
  cp "$repo_root/diff/examples/05_ignore_space_change/B.TXT" "$zip_root/EXAMPLES/05_IGNBC/B.TXT"
  cp "$repo_root/diff/examples/05_ignore_space_change/CHECK.BAT" "$zip_root/EXAMPLES/05_IGNBC/CHECK.BAT"
  cp "$repo_root/diff/examples/06_ignore_all_space/A.TXT" "$zip_root/EXAMPLES/06_IGNWS/A.TXT"
  cp "$repo_root/diff/examples/06_ignore_all_space/B.TXT" "$zip_root/EXAMPLES/06_IGNWS/B.TXT"
  cp "$repo_root/diff/examples/06_ignore_all_space/CHECK.BAT" "$zip_root/EXAMPLES/06_IGNWS/CHECK.BAT"
}

copy_deltree_payload() {
  local utility_root="::/DELTREE"
  local zip_root="$dist_root/DELTREE"

  mkdir_img_dir "$utility_root"
  mkdir_img_dir "$utility_root/EXAMPLES"
  mkdir_img_dir "$utility_root/EXAMPLES/01_BASIC"
  mkdir_img_dir "$utility_root/EXAMPLES/02_WILDS"

  mkdir -p "$zip_root/EXAMPLES/01_BASIC"
  mkdir -p "$zip_root/EXAMPLES/02_WILDS"

  mcopy -i "$image_path" -o "$repo_root/deltree/deltree.exe" "$utility_root/DELTREE.EXE"
  mcopy -i "$image_path" -o "$repo_root/deltree/deltree.txt" "$utility_root/DELTREE.TXT"
  mcopy -i "$image_path" -o "$repo_root/deltree/examples/README.md" "$utility_root/EXAMPLES/README.MD"
  mcopy -i "$image_path" -o "$repo_root/deltree/examples/01_basic/CHECK.BAT" "$utility_root/EXAMPLES/01_BASIC/CHECK.BAT"
  mcopy -i "$image_path" -o "$repo_root/deltree/examples/02_wildcards/CHECK.BAT" "$utility_root/EXAMPLES/02_WILDS/CHECK.BAT"

  cp "$repo_root/deltree/deltree.exe" "$zip_root/DELTREE.EXE"
  cp "$repo_root/deltree/deltree.txt" "$zip_root/DELTREE.TXT"
  cp "$repo_root/deltree/examples/README.md" "$zip_root/EXAMPLES/README.MD"
  cp "$repo_root/deltree/examples/01_basic/CHECK.BAT" "$zip_root/EXAMPLES/01_BASIC/CHECK.BAT"
  cp "$repo_root/deltree/examples/02_wildcards/CHECK.BAT" "$zip_root/EXAMPLES/02_WILDS/CHECK.BAT"
}

copy_utility_payload() {
  local utility="$1"

  case "$utility" in
    make)
      copy_make_payload
      ;;
    diff)
      copy_diff_payload
      ;;
    deltree)
      copy_deltree_payload
      ;;
    *)
      echo "Error: unknown utility: $utility" >&2
      exit 1
      ;;
  esac
}

echo "Building project utilities..."
for utility in "${utilities[@]}"; do
  build_utility "$utility"
done

echo "Creating FAT12 floppy image..."
mformat -C -i "$image_path" -f 1440 ::

for utility in "${utilities[@]}"; do
  copy_utility_payload "$utility"
done

echo "Creating ZIP package (store mode)..."
(
  cd "$dist_root"
  zip -r -0 "$dist_zip_path" .
)

echo "Created FAT12 floppy image: $image_path"
echo "Created ZIP package: $dist_zip_path"
echo "Utilities packaged by folders under image root."
