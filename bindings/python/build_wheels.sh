#!/usr/bin/env bash
# Stage Release natives and build a wheel for this OS.
set -euo pipefail
TAG="${1:-v2.0.0}"
REPO="${2:-GlebVoronkov03/PLER}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PKG="$ROOT/bindings/python"
NATIVE="$PKG/pler_metric/_native"
TMP="$ROOT/dist/_py_wheel_tmp"
OUT="$ROOT/dist/wheels"

detect_platform() {
  if [[ -n "${PLER_PLATFORM:-}" ]]; then echo "$PLER_PLATFORM"; return; fi
  case "$(uname -s)" in
    Linux) echo "linux-x64" ;;
    Darwin)
      if [[ "$(uname -m)" == "arm64" ]]; then echo "macos-arm64"; else echo "macos-x64"; fi
      ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows-x64" ;;
    *) echo "unsupported"; exit 1 ;;
  esac
}

PLATFORM="$(detect_platform)"
rm -rf "$TMP"
mkdir -p "$TMP" "$OUT" "$NATIVE/$PLATFORM"

pick_asset() {
  case "$PLATFORM" in
    windows-x64) echo "pler-2.0-win64.zip" ;;
    linux-x64)
      if gh api "repos/$REPO/releases/tags/$TAG" --jq '.assets[].name' 2>/dev/null | grep -qx 'pler-2.0-linux-x64-cuda.tar.gz'; then
        echo "pler-2.0-linux-x64-cuda.tar.gz"
      else
        echo "pler-2.0-linux-x64.tar.gz"
      fi
      ;;
    macos-arm64) echo "pler-2.0-macos-arm64.zip" ;;
    macos-x64) echo "pler-2.0-macos-x64.zip" ;;
  esac
}

ASSET="$(pick_asset)"
echo "Platform=$PLATFORM Asset=$ASSET"
gh release download "$TAG" -R "$REPO" -D "$TMP" --clobber -p "$ASSET"
EXTRACT="$TMP/extract"
mkdir -p "$EXTRACT"
if [[ "$ASSET" == *.tar.gz ]]; then
  tar -xzf "$TMP/$ASSET" -C "$EXTRACT"
else
  unzip -q -o "$TMP/$ASSET" -d "$EXTRACT"
fi

rm -rf "${NATIVE:?}/$PLATFORM"
mkdir -p "$NATIVE/$PLATFORM"
CLI=pler
LIB=libpler.so
[[ "$PLATFORM" == windows-x64 ]] && CLI=pler.exe && LIB=pler.dll
[[ "$PLATFORM" == macos-* ]] && LIB=libpler.dylib

FOUND_CLI="$(find "$EXTRACT" -type f -name "$CLI" | head -n1)"
FOUND_LIB="$(find "$EXTRACT" -type f -name "$LIB" | head -n1)"
[[ -n "$FOUND_CLI" && -n "$FOUND_LIB" ]] || { echo "missing binaries"; exit 1; }
cp -f "$FOUND_CLI" "$NATIVE/$PLATFORM/$CLI"
cp -f "$FOUND_LIB" "$NATIVE/$PLATFORM/$LIB"
chmod +x "$NATIVE/$PLATFORM/$CLI" || true
BINDIR="$(dirname "$FOUND_CLI")"
cp -f "$BINDIR"/libcudart.so* "$NATIVE/$PLATFORM/" 2>/dev/null || true
cp -f "$BINDIR"/cudart64_*.dll "$NATIVE/$PLATFORM/" 2>/dev/null || true

cd "$PKG"
python -m pip install -q build wheel
python -m build --wheel -o "$OUT"
echo "Wheels in $OUT"
ls -la "$OUT"
