#!/usr/bin/env bash
# Stage install tree and archive for Linux / macOS (Phase 2).
# Usage:
#   ./package.sh --os linux --arch x64
#   ./package.sh --os macos --arch arm64
#   ./package.sh --os linux --arch x64 --cuda
set -euo pipefail

OS=""
ARCH=""
WITH_CUDA=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --os) OS="${2:-}"; shift 2 ;;
    --arch) ARCH="${2:-}"; shift 2 ;;
    --cuda) WITH_CUDA=1; shift ;;
    -h|--help)
      echo "Usage: $0 --os linux|macos --arch x64|arm64 [--cuda]"
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -z "$OS" || -z "$ARCH" ]]; then
  echo "Required: --os linux|macos --arch x64|arm64" >&2
  exit 1
fi
if [[ "$OS" != "linux" && "$OS" != "macos" ]]; then
  echo "--os must be linux or macos" >&2
  exit 1
fi
if [[ "$ARCH" != "x64" && "$ARCH" != "arm64" ]]; then
  echo "--arch must be x64 or arm64" >&2
  exit 1
fi
if [[ "$WITH_CUDA" -eq 1 && "$OS" != "linux" ]]; then
  echo "--cuda is only supported for linux packages" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD="${PLER_BUILD_DIR:-$SCRIPT_DIR/build}"
PREFIX="$ROOT/dist/pler-2.0"

find_bin() {
  local name="$1"
  local candidates=(
    "$BUILD/pler_cli/$name"
    "$BUILD/pler_cli/Release/$name"
    "$BUILD/pler_gui/$name"
    "$BUILD/pler_gui/Release/$name"
    "$BUILD/pler_c/$name"
    "$BUILD/pler_c/Release/$name"
    "$BUILD/bin/$name"
    "$BUILD/$name"
  )
  local c
  for c in "${candidates[@]}"; do
    if [[ -f "$c" ]]; then
      echo "$c"
      return 0
    fi
  done
  return 1
}

PLER_BIN="$(find_bin pler || true)"
GUI_BIN="$(find_bin pler_gui || true)"
LIB_SO="$(find_bin libpler.so || true)"
LIB_DYLIB="$(find_bin libpler.dylib || true)"

if [[ -z "$PLER_BIN" ]]; then
  echo "pler binary not found under $BUILD — configure and build Release first" >&2
  exit 1
fi
if [[ -z "$GUI_BIN" ]]; then
  echo "pler_gui binary not found under $BUILD" >&2
  exit 1
fi

rm -rf "$PREFIX"
mkdir -p "$PREFIX/bin" "$PREFIX/config" "$PREFIX/docs" "$PREFIX/samples" "$PREFIX/include"

cp -f "$PLER_BIN" "$PREFIX/bin/pler"
cp -f "$GUI_BIN" "$PREFIX/bin/pler_gui"
chmod +x "$PREFIX/bin/pler" "$PREFIX/bin/pler_gui"

if [[ -n "$LIB_SO" ]]; then
  cp -f "$LIB_SO" "$PREFIX/bin/"
elif [[ -n "$LIB_DYLIB" ]]; then
  cp -f "$LIB_DYLIB" "$PREFIX/bin/"
else
  # CMake may place libpler next to the shared target without Release/ subdir variants above
  shopt -s nullglob
  for f in "$BUILD"/pler_c/libpler.* "$BUILD"/pler_c/Release/libpler.* "$BUILD"/libpler.*; do
    cp -f "$f" "$PREFIX/bin/"
  done
  shopt -u nullglob
fi

cp -f "$SCRIPT_DIR/pler_c/include/pler.h" "$PREFIX/include/"
cp -f "$ROOT/config/pler.yaml" "$PREFIX/config/"
cp -f "$ROOT"/docs/*.md "$PREFIX/docs/" 2>/dev/null || true
cp -f "$ROOT/README.md" "$PREFIX/docs/"
cp -f "$ROOT/LICENSE" "$PREFIX/docs/"
cp -f "$ROOT/CITATION.cff" "$PREFIX/docs/"
cp -f "$ROOT"/samples/*.obj "$PREFIX/samples/" 2>/dev/null || true

if [[ "$WITH_CUDA" -eq 1 ]]; then
  CUDABIN="${CUDA_PATH:+$CUDA_PATH/lib64}"
  if [[ -z "${CUDABIN:-}" || ! -d "$CUDABIN" ]]; then
    for d in /usr/local/cuda/lib64 /usr/local/cuda-12*/lib64 /usr/local/cuda-11*/lib64; do
      if [[ -d "$d" ]]; then CUDABIN="$d"; break; fi
    done
  fi
  if [[ -n "${CUDABIN:-}" && -d "$CUDABIN" ]]; then
    shopt -s nullglob
    copied=0
    for f in "$CUDABIN"/libcudart.so*; do
      cp -f "$f" "$PREFIX/bin/"
      copied=1
    done
    shopt -u nullglob
    if [[ "$copied" -eq 1 ]]; then
      echo "Bundled CUDA runtime from $CUDABIN"
    else
      echo "Warning: no libcudart.so* in $CUDABIN" >&2
    fi
  else
    echo "Warning: CUDA lib dir not found; packaging without cudart" >&2
  fi
fi

{
  echo "PLER 2.0.0"
  echo
  echo "Run:  ./bin/pler --version"
  echo "      ./bin/pler selftest"
  echo "      ./bin/pler samples/ref_unit_sphere.obj samples/test_unit_sphere_lod.obj"
  echo "      ./bin/pler_gui"
  echo
  if [[ "$WITH_CUDA" -eq 1 ]]; then
    echo "This package may include CUDA runtime libraries. Without an NVIDIA GPU,"
    echo "PLER falls back to the CPU BVH automatically."
  else
    echo "CPU package. Build with CUDA locally for the GPU BVH path."
  fi
  echo
  if [[ "$OS" == "macos" ]]; then
    echo "macOS builds are unsigned. Gatekeeper may require:"
    echo "  xattr -dr com.apple.quarantine ."
  elif [[ "$OS" == "linux" ]]; then
    echo "GUI needs system OpenGL/X11 (typical desktop install)."
  fi
  echo
  echo "License: MIT  (see docs/LICENSE)"
  echo "Author: Gleb Alekseevich Voronkov  glebvoronkov03@gmail.com"
} > "$PREFIX/README.txt"

mkdir -p "$ROOT/dist"
cd "$ROOT/dist"

if [[ "$OS" == "linux" ]]; then
  if [[ "$WITH_CUDA" -eq 1 ]]; then
    OUT="pler-2.0-linux-${ARCH}-cuda.tar.gz"
  else
    OUT="pler-2.0-linux-${ARCH}.tar.gz"
  fi
  rm -f "$OUT"
  tar -czf "$OUT" pler-2.0
  echo "Archive: $ROOT/dist/$OUT"
else
  OUT="pler-2.0-macos-${ARCH}.zip"
  DMG="pler-2.0-macos-${ARCH}.dmg"
  rm -f "$OUT" "$DMG"
  if command -v ditto >/dev/null 2>&1; then
    ditto -c -k --sequesterRsrc --keepParent pler-2.0 "$OUT"
  else
    zip -r "$OUT" pler-2.0
  fi
  echo "Archive: $ROOT/dist/$OUT"

  if command -v hdiutil >/dev/null 2>&1; then
    hdiutil create -volname "PLER-2.0" -srcfolder pler-2.0 -ov -format UDZO "$DMG"
    echo "DMG: $ROOT/dist/$DMG"
  else
    echo "hdiutil not found; skipped DMG" >&2
  fi
fi
