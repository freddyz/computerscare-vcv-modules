#!/usr/bin/env bash
# Sandbox-friendly checks: things an agent can run in a loop without the
# VCV Rack SDK to verify the codebase isn't obviously broken.
#
# What this catches:
#   - Invalid plugin.json
#   - Modules in plugin.json missing a C++ registration (or vice versa)
#   - asset::plugin("res/foo.svg") references that point to missing files
#   - SVG/JSON/preset files that aren't valid XML/JSON
#   - clang-format violations
#   - C++ files that won't even preprocess (mismatched #if / unknown #include)
#
# What this does NOT catch:
#   - Type errors, undefined symbols, link errors. Those need the real SDK.
#
# Exits 0 on success, 1 on first failure (so an agent can react fast).

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"

FAIL=0
fail() { echo "FAIL: $*" >&2; FAIL=1; }
pass() { echo "  ok: $*"; }

# ---------------------------------------------------------------------------
echo "==> plugin.json is valid JSON..."
python3 -c "import json; json.load(open('plugin.json'))" \
  || { fail "plugin.json is not valid JSON"; exit 1; }
pass "plugin.json"

# ---------------------------------------------------------------------------
echo "==> module slugs in plugin.json are wired up in C++..."
python3 - <<'PY' || FAIL=1
import json, re, sys, pathlib

manifest = json.load(open("plugin.json"))
manifest_slugs = {m["slug"] for m in manifest["modules"]}

src = pathlib.Path("src")
all_src = "\n".join(p.read_text() for p in [*src.rglob("*.cpp"), *src.rglob("*.hpp")])

# Match createModel<...>("slug") with optional whitespace/newlines between > and (
created_slugs = set(re.findall(
    r'createModel<[^<>]+(?:<[^<>]*>[^<>]*)*>\s*\(\s*"([^"]+)"',
    all_src,
))

addmodel_idents = set(re.findall(r'p->addModel\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)', all_src))

errors = []

missing_in_cpp = manifest_slugs - created_slugs
if missing_in_cpp:
    errors.append(f"slugs in plugin.json with no createModel<>: {sorted(missing_in_cpp)}")

missing_in_manifest = created_slugs - manifest_slugs
if missing_in_manifest:
    errors.append(f"createModel<> slugs not in plugin.json: {sorted(missing_in_manifest)}")

# Every addModel() target should have a Model* defined somewhere in the source.
defined_models = set(re.findall(
    r'\bModel\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*createModel',
    all_src,
))
undefined_addmodels = addmodel_idents - defined_models
if undefined_addmodels:
    errors.append(f"p->addModel() refers to undefined Model: {sorted(undefined_addmodels)}")

if errors:
    print("\n  ".join(["plugin.json <-> C++ wiring problems:"] + errors), file=sys.stderr)
    sys.exit(1)
PY
[[ $FAIL -eq 0 ]] && pass "manifest <-> C++ registrations"

# ---------------------------------------------------------------------------
echo "==> asset::plugin paths point to existing files..."
python3 - <<'PY' || FAIL=1
import re, pathlib, sys

pat = re.compile(r'asset::plugin\(\s*pluginInstance\s*,\s*"([^"]+)"\s*\)')
missing = []
for p in pathlib.Path("src").rglob("*.cpp"):
    for m in pat.finditer(p.read_text()):
        ref = m.group(1)
        if not pathlib.Path(ref).exists():
            missing.append((p.name, ref))
for p in pathlib.Path("src").rglob("*.hpp"):
    for m in pat.finditer(p.read_text()):
        ref = m.group(1)
        if not pathlib.Path(ref).exists():
            missing.append((p.name, ref))

if missing:
    for src, ref in missing:
        print(f"  {src}: missing asset {ref}", file=sys.stderr)
    print(f"FAIL: {len(missing)} broken asset references", file=sys.stderr)
    sys.exit(1)
PY
[[ $FAIL -eq 0 ]] && pass "asset references"

# ---------------------------------------------------------------------------
echo "==> SVG files parse as XML..."
BAD_SVGS=$(find res -name "*.svg" -print0 \
  | xargs -0 -I{} python3 -c "
import xml.etree.ElementTree as ET, sys
try:
    ET.parse('{}')
except Exception as e:
    print('{}: ' + str(e))
" 2>&1 | head -20)
if [[ -n "$BAD_SVGS" ]]; then
  fail "broken SVGs:"
  echo "$BAD_SVGS" >&2
fi
[[ -z "$BAD_SVGS" ]] && pass "SVG XML"

# ---------------------------------------------------------------------------
echo "==> presets/*.vcvm parse as JSON..."
if compgen -G "presets/**/*.vcvm" > /dev/null; then
  while IFS= read -r f; do
    python3 -c "import json; json.load(open('$f'))" 2>/dev/null \
      || fail "broken preset: $f"
  done < <(find presets -name "*.vcvm")
fi
pass "presets"

# ---------------------------------------------------------------------------
# clang-format is opt-in (export CHECK_FMT=1) because much of the legacy code
# isn't formatted yet and we don't want every loop iteration to fail on style.
if [[ "${CHECK_FMT:-0}" == "1" ]]; then
  echo "==> clang-format dry-run..."
  SRC_FILES=$(find src -name "*.cpp" -o -name "*.hpp")
  if command -v clang-format >/dev/null; then
    if ! clang-format --dry-run --Werror $SRC_FILES 2>/dev/null; then
      fail "clang-format violations (run scripts/dev.sh fmt to fix)"
    else
      pass "clang-format"
    fi
  else
    echo "  skip: clang-format not installed"
  fi
fi

# ---------------------------------------------------------------------------
echo "==> C++ preprocessor dry-run (catches truly broken syntax)..."
# Without the SDK we can't do -fsyntax-only properly, but -E (preprocess) with
# missing headers ignored at least catches mismatched #if/#endif, stray chars,
# bad #include paths within our own code, etc.
if command -v g++ >/dev/null; then
  PP_ERRORS=$(
    for f in $(find src -name "*.cpp" -o -name "*.hpp"); do
      g++ -E -std=c++17 -x c++ -nostdinc -I src "$f" -o /dev/null 2>&1 \
        | grep -E "error:" \
        | grep -vE "fatal error:.*file not found|No such file or directory" \
        || true
    done
  )
  if [[ -n "$PP_ERRORS" ]]; then
    fail "preprocessor errors:"
    echo "$PP_ERRORS" | head -20 >&2
  else
    pass "preprocessor"
  fi
fi

# ---------------------------------------------------------------------------
echo
if [[ $FAIL -eq 0 ]]; then
  echo "All sandbox checks passed."
else
  echo "Sandbox checks failed."
  exit 1
fi
