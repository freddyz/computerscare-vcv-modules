#!/usr/bin/env bash
# Offscreen renders of real Rack module widgets, for local layout development.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
RACK_PREVIEW_DIR=${RACK_SOURCE_DIR:-${RACK_DIR:-$HOME/dev/VCV-Rack/Rack}}
# The build environment often points RACK_DIR at the SDK, which lacks assets.
if [[ ! -d "$RACK_PREVIEW_DIR/res" && -d "$(dirname "$RACK_PREVIEW_DIR")/Rack/res" ]]; then
  RACK_PREVIEW_DIR="$(dirname "$RACK_PREVIEW_DIR")/Rack"
fi
OUTPUT_DIR=""
MODULE=phlooper
SCALE=2
BUILD_PLUGIN=1
PREVIEW_MODE=scenarios
while (($#)); do
  case "$1" in
    groups) MODULE=groups; PREVIEW_MODE=browser; shift ;;
    phlooper) MODULE=phlooper; shift ;;
    blunch) MODULE=blunch; PREVIEW_MODE=browser; shift ;;
    32nd-and-columbus|windchimes) MODULE=32nd-and-columbus; PREVIEW_MODE=browser; shift ;;
    xrouter|mely-porge|voly-pector|laundry-soup) MODULE=$1; PREVIEW_MODE=browser; shift ;;
    --output) OUTPUT_DIR=$2; shift 2 ;;
    --scale) SCALE=$2; shift 2 ;;
    --browser) PREVIEW_MODE=browser; shift ;;
    --controls) PREVIEW_MODE=controls; shift ;;
    --no-build) BUILD_PLUGIN=0; shift ;;
    -h|--help)
      echo "Usage: scripts/preview-module.sh [groups|phlooper|blunch|32nd-and-columbus|mely-porge|voly-pector|laundry-soup|xrouter] [--output DIR] [--scale 2] [--no-build] [--browser] [--controls]"
      echo "RACK_DIR must contain a built Rack library and res/. No audio device or patch is opened."
      echo "Images and the preview executable default to ignored build/previews/."
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done
if [[ ! -f "$RACK_PREVIEW_DIR/libRack.dylib" ]]; then
  echo "A built macOS Rack source tree is required. Set RACK_DIR (missing $RACK_PREVIEW_DIR/libRack.dylib)." >&2
  exit 1
fi
OUTPUT_DIR=${OUTPUT_DIR:-$REPO_DIR/build/previews/$MODULE}
mkdir -p "$OUTPUT_DIR" "$REPO_DIR/build/previews"
OUTPUT_DIR=$(cd "$OUTPUT_DIR" && pwd)
if [[ $BUILD_PLUGIN == 1 ]]; then
  make -C "$REPO_DIR" RACK_DIR="$RACK_PREVIEW_DIR" -j4
fi
if [[ ! -f "$REPO_DIR/plugin.dylib" ]]; then
  echo "Build plugin.dylib first, or omit --no-build." >&2
  exit 1
fi
PREVIEW_BIN="$REPO_DIR/build/previews/phlooper-preview"
RACK_STAMP="$PREVIEW_BIN.rack-dir"
REBUILD_PREVIEW=0
if [[ ! -f "$RACK_STAMP" || "$(cat "$RACK_STAMP" 2>/dev/null)" != "$RACK_PREVIEW_DIR" ||
      ! -f "$PREVIEW_BIN" || "$REPO_DIR/plugin.dylib" -nt "$PREVIEW_BIN" ||
      "$RACK_PREVIEW_DIR/libRack.dylib" -nt "$PREVIEW_BIN" ||
      "$RACK_PREVIEW_DIR/src/window/Window.cpp" -nt "$PREVIEW_BIN" || "$0" -nt "$PREVIEW_BIN" ]]; then
  REBUILD_PREVIEW=1
elif [[ -n "$(find "$REPO_DIR/src" "$REPO_DIR/scripts/preview" \
    "$RACK_PREVIEW_DIR/include" "$RACK_PREVIEW_DIR/dep/include" \
    -type f -newer "$PREVIEW_BIN" -print -quit)" ]]; then
  REBUILD_PREVIEW=1
fi
if [[ $REBUILD_PREVIEW == 1 ]]; then
  echo "Building preview runner..."
  python3 "$REPO_DIR/scripts/preview/prepare-render-window.py" "$RACK_PREVIEW_DIR" "$REPO_DIR/build/previews/render-window.cpp"
c++ -std=c++11 -O2 -DARCH_MAC -Wno-deprecated-declarations -I"$RACK_PREVIEW_DIR/include" \
  -I"$RACK_PREVIEW_DIR/dep/include" -I"$REPO_DIR/src" \
  "$REPO_DIR/scripts/preview/phlooper.cpp" "$REPO_DIR/build/previews/render-window.cpp" "$REPO_DIR/plugin.dylib" \
  -L"$RACK_PREVIEW_DIR" -lRack -framework OpenGL \
  -Wl,-rpath,"$REPO_DIR" -Wl,-rpath,"$RACK_PREVIEW_DIR" \
  -o "$PREVIEW_BIN"
  printf '%s\n' "$RACK_PREVIEW_DIR" > "$RACK_STAMP"
else
  echo "Using cached preview runner."
fi
DYLD_LIBRARY_PATH="$RACK_PREVIEW_DIR:$REPO_DIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" \
  "$REPO_DIR/build/previews/phlooper-preview" "$REPO_DIR" "$RACK_PREVIEW_DIR" "$OUTPUT_DIR" "$SCALE" "$PREVIEW_MODE" "$MODULE"
