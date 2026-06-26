#!/usr/bin/env bash
# Fetch the VCV Rack SDK (or its source) so that the plugin can be linted/built.
#
# Output: $RACK_DIR is populated with at least include/ and plugin.mk.
#
# Tries, in order:
#   1. Existing $RACK_DIR (if it already has include/rack.hpp)
#   2. Official SDK zip from vcvrack.com  (best: includes built dep/include)
#   3. Source tarball of VCVRack/Rack@v2 from github.com (no built deps)
#
# Many sandboxed/agent environments block vcvrack.com but allow github.com,
# so step 3 is usually what wins there. Step 3 only gives you headers, not
# pre-built libraries, so a real plugin link will still fail; -fsyntax-only
# checks are still possible after manually populating dep/include.

set -euo pipefail

RACK_DIR="${RACK_DIR:-$HOME/Rack-SDK}"
SDK_VERSION="${SDK_VERSION:-latest}"

case "$(uname -s)" in
  Linux*)  PLATFORM="lin-x64" ;;
  Darwin*) PLATFORM="mac-x64+arm64" ;;
  MINGW*|MSYS*|CYGWIN*) PLATFORM="win-x64" ;;
  *)       PLATFORM="lin-x64" ;;
esac

if [[ -f "$RACK_DIR/include/rack.hpp" && -f "$RACK_DIR/plugin.mk" ]]; then
  echo "Rack SDK already at $RACK_DIR"
  exit 0
fi

mkdir -p "$RACK_DIR"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "==> Trying official SDK from vcvrack.com (platform=$PLATFORM, version=$SDK_VERSION)..."
SDK_URL="https://vcvrack.com/downloads/Rack-SDK-${SDK_VERSION}-${PLATFORM}.zip"
if curl -fsSL --max-time 30 "$SDK_URL" -o "$TMP/sdk.zip" 2>/dev/null; then
  unzip -q "$TMP/sdk.zip" -d "$TMP"
  EXTRACTED="$(find "$TMP" -maxdepth 2 -name plugin.mk -printf '%h\n' | head -1)"
  if [[ -n "$EXTRACTED" ]]; then
    cp -r "$EXTRACTED"/. "$RACK_DIR"/
    echo "==> Installed official SDK to $RACK_DIR"
    exit 0
  fi
fi
echo "    (vcvrack.com unreachable or denied; falling back to GitHub source)"

echo "==> Cloning VCVRack/Rack@v2 source from GitHub..."
GH_URL="https://codeload.github.com/VCVRack/Rack/tar.gz/refs/heads/v2"
if ! curl -fsSL --max-time 60 "$GH_URL" -o "$TMP/rack.tgz"; then
  echo "ERROR: could not fetch Rack source from GitHub either." >&2
  exit 1
fi
tar xzf "$TMP/rack.tgz" -C "$TMP"
SRC="$(find "$TMP" -maxdepth 2 -name plugin.mk -printf '%h\n' | head -1)"
cp -r "$SRC"/. "$RACK_DIR"/

# The dep/ submodules are empty in a tarball download, so plugin.mk's
# `-I$(RACK_DIR)/dep/include` is non-functional. Create the dir so make
# doesn't complain and warn the user.
mkdir -p "$RACK_DIR/dep/include"
cat > "$RACK_DIR/dep/include/.SANDBOX-README" <<'EOF'
This is a partial SDK installed from VCVRack/Rack source on GitHub.
dep/include is empty because the dep/* submodules (nanovg, glfw, etc.)
are not bundled. You can still run preprocessor/syntax checks that don't
require those headers, but full builds will fail. To build the plugin,
download the official SDK from vcvrack.com.
EOF

echo "==> Installed partial SDK (headers only) to $RACK_DIR"
echo "    For full build, fetch the official SDK from vcvrack.com instead."
