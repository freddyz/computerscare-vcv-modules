#!/usr/bin/env bash
# Build a private SDL3 runtime, or copy a locally built runtime for development.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
# Local development cache lives outside branch checkouts.
if [[ ${1:-} == --local ]]; then
  CACHE=${CS_CONTROLLER_RUNTIME_DIR:-"$HOME/dev/computerscare-controller-runtime"}
  case $(uname -s) in
    Darwin) NAME=libSDL3.dylib ;;
    MINGW*|MSYS*) NAME=SDL3.dll ;;
    *) NAME=libSDL3.so.0 ;;
  esac
  [[ -f "$CACHE/$NAME" && -f "$CACHE/SDL3-LICENSE.txt" ]] || {
    echo "Controller runtime or license missing in $CACHE" >&2
    exit 1
  }
  "$0" --library "$CACHE/$NAME"
  cp "$CACHE/SDL3-LICENSE.txt" "$ROOT/lib/SDL3-LICENSE.txt"
  exit 0
fi
mkdir -p "$ROOT/lib"
if [[ ${1:-} == --library ]]; then
  case $(uname -s) in
    Darwin) NAME=libSDL3.dylib ;;
    MINGW*|MSYS*) NAME=SDL3.dll ;;
    *) NAME=libSDL3.so.0 ;;
  esac
  cp -L "$2" "$ROOT/lib/$NAME"
  echo "Installed $ROOT/lib/$NAME"
  exit 0
fi
if [[ $# != 1 || ! -f "$1/CMakeLists.txt" ]]; then
  echo '       scripts/build-controller-runtime.sh --local' >&2
  echo 'Usage: scripts/build-controller-runtime.sh /path/to/SDL3-source' >&2
  echo '       scripts/build-controller-runtime.sh --library /path/to/SDL3-library' >&2
  exit 2
fi
cmake -S "$1" -B "$ROOT/build/controller-runtime" \
  -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=ON -DSDL_STATIC=OFF \
  -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF \
  -DCMAKE_INSTALL_PREFIX="$ROOT/build/controller-install"
cmake --build "$ROOT/build/controller-runtime" --config Release --parallel 4
cmake --install "$ROOT/build/controller-runtime" --config Release
case $(uname -s) in
  Darwin) LIBRARY="$ROOT/build/controller-install/lib/libSDL3.dylib" ;;
  MINGW*|MSYS*) LIBRARY="$ROOT/build/controller-install/bin/SDL3.dll" ;;
  *) LIBRARY="$ROOT/build/controller-install/lib/libSDL3.so.0" ;;
esac
"$0" --library "$LIBRARY"
cp "$1/LICENSE.txt" "$ROOT/lib/SDL3-LICENSE.txt"
