#!/usr/bin/env bash
# Build pler-blender-2.0.zip with bundled Release CLI binaries.
set -euo pipefail
TAG="${1:-v2.0.0}"
REPO="${2:-GlebVoronkov03/PLER}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ADDON_SRC="$ROOT/addons/blender/pler_metric"
STAGE="$ROOT/dist/pler_blender_stage"
BIN_ROOT="$STAGE/pler_metric/bin"
OUT_ZIP="$ROOT/dist/pler-blender-2.0.zip"
TMP="$ROOT/dist/_blender_pkg_tmp"

rm -rf "$STAGE" "$TMP"
mkdir -p "$STAGE/pler_metric" "$BIN_ROOT" "$TMP" "$ROOT/dist"
rsync -a --exclude bin --exclude __pycache__ "$ADDON_SRC/" "$STAGE/pler_metric/"

gh release download "$TAG" -R "$REPO" -D "$TMP" --clobber \
  -p "pler-2.0-win64.zip" \
  -p "pler-2.0-linux-x64.tar.gz" \
  -p "pler-2.0-macos-arm64.zip" \
  -p "pler-2.0-macos-x64.zip"

copy_cli() {
  local archive="$1" key="$2" name="$3"
  local extract="$TMP/extract_$key"
  mkdir -p "$extract" "$BIN_ROOT/$key"
  if [[ "$archive" == *.tar.gz ]]; then
    tar -xzf "$archive" -C "$extract"
  else
    unzip -q -o "$archive" -d "$extract"
  fi
  local found
  found="$(find "$extract" -type f -name "$name" | head -n1)"
  [[ -n "$found" ]] || { echo "missing $name in $archive" >&2; exit 1; }
  cp -f "$found" "$BIN_ROOT/$key/$name"
  chmod +x "$BIN_ROOT/$key/$name" || true
  local bdir
  bdir="$(dirname "$found")"
  for pat in pler.dll libpler.so libpler.dylib; do
    [[ -f "$bdir/$pat" ]] && cp -f "$bdir/$pat" "$BIN_ROOT/$key/" || true
  done
  echo "staged $key/$name"
}

copy_cli "$TMP/pler-2.0-win64.zip" windows-x64 pler.exe
copy_cli "$TMP/pler-2.0-linux-x64.tar.gz" linux-x64 pler
copy_cli "$TMP/pler-2.0-macos-arm64.zip" macos-arm64 pler
copy_cli "$TMP/pler-2.0-macos-x64.zip" macos-x64 pler

rm -f "$OUT_ZIP"
( cd "$STAGE" && zip -r "$OUT_ZIP" pler_metric )
echo "Wrote $OUT_ZIP"
ls -la "$OUT_ZIP"
