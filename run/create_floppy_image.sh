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

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

image_path="${1:-$repo_root/build/utils.img}"

mkdir -p "$repo_root/build"
rm -f "$image_path"

utilities=(
  make
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

  mkdir_img_dir "$utility_root"
  mkdir_img_dir "$utility_root/EXAMPLES"
  mkdir_img_dir "$utility_root/EXAMPLES/01_BASIC"
  mkdir_img_dir "$utility_root/EXAMPLES/02_PREFX"
  mkdir_img_dir "$utility_root/EXAMPLES/03_MULTI"
  mkdir_img_dir "$utility_root/EXAMPLES/04_SOLID"
  mkdir_img_dir "$utility_root/TOOLS"

  mcopy -i "$image_path" -o "$repo_root/make/make.exe" "$utility_root/MAKE.EXE"

  mcopy -i "$image_path" -o "$repo_root/make/examples/README.md" "$utility_root/EXAMPLES/README.MD"
  mcopy -i "$image_path" -o "$repo_root/make/examples/01_basic/Makefile" "$utility_root/EXAMPLES/01_BASIC/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/02_prefixes/Makefile" "$utility_root/EXAMPLES/02_PREFX/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/03_multifile/Makefile" "$utility_root/EXAMPLES/03_MULTI/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/04_solidc/Makefile" "$utility_root/EXAMPLES/04_SOLID/MAKEFILE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/04_solidc/hello.c" "$utility_root/EXAMPLES/04_SOLID/HELLO.C"

  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/TOOLS/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkfail/mkfail.exe" "$utility_root/TOOLS/MKFAIL.EXE"

  # Duplicate helper tools into each example directory so examples can run in place.
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/EXAMPLES/01_BASIC/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/EXAMPLES/02_PREFX/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkstamp/mkstamp.exe" "$utility_root/EXAMPLES/03_MULTI/MKSTAMP.EXE"
  mcopy -i "$image_path" -o "$repo_root/make/examples/tools/mkfail/mkfail.exe" "$utility_root/EXAMPLES/02_PREFX/MKFAIL.EXE"
}

copy_utility_payload() {
  local utility="$1"

  case "$utility" in
    make)
      copy_make_payload
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

echo "Created FAT12 floppy image: $image_path"
echo "Utilities packaged by folders under image root."
