#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/gen.sh "text" [options]

Options:
  -o, --output FILE     Write SVG to FILE. If omitted, SVG is printed to stdout.
  --font FONT           Font family/name to use. Default: Verdana
  --size SIZE           Font size to use. Default: 72
  --rand VALUE          Randomization amount from 0 to 100. Default: 32
  --seed VALUE          Seed for repeatable randomization.
  -h, --help            Show this help.
EOF
}

if [[ $# -eq 0 ]]; then
  usage >&2
  exit 1
fi

TEXT=""
OUTPUT=""
FONT="Verdana"
SIZE="72"
RAND="32"
SEED=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -o|--output)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      OUTPUT="$2"
      shift 2
      ;;
    --output=*)
      OUTPUT="${1#*=}"
      shift
      ;;
    --font)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      FONT="$2"
      shift 2
      ;;
    --font=*)
      FONT="${1#*=}"
      shift
      ;;
    --size)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      SIZE="$2"
      shift 2
      ;;
    --size=*)
      SIZE="${1#*=}"
      shift
      ;;
    --rand)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      RAND="$2"
      shift 2
      ;;
    --rand=*)
      RAND="${1#*=}"
      shift
      ;;
    --seed)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      SEED="$2"
      shift 2
      ;;
    --seed=*)
      SEED="${1#*=}"
      shift
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
    *)
      if [[ -n "$TEXT" ]]; then
        echo "Only one text argument is supported. Quote text with spaces." >&2
        exit 1
      fi
      TEXT="$1"
      shift
      ;;
  esac
done

if [[ -z "$TEXT" ]]; then
  echo "Missing text argument." >&2
  usage >&2
  exit 1
fi

if ! command -v pango-view >/dev/null 2>&1; then
  echo "pango-view is required but was not found on PATH." >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER="$SCRIPT_DIR/svg_text_randomize.py"

if [[ ! -f "$HELPER" ]]; then
  echo "Missing helper: $HELPER" >&2
  exit 1
fi

if command -v fc-match >/dev/null 2>&1; then
  MATCHED="$(fc-match "$FONT" 2>/dev/null | sed 's/:.*//')"
  if [[ -n "$MATCHED" ]]; then
    MATCHED_FAMILY="$(fc-match -f '%{family[0]}\n' "$FONT" 2>/dev/null || true)"
    MATCHED_FAMILY_LC="$(printf '%s' "$MATCHED_FAMILY" | tr '[:upper:]' '[:lower:]')"
    FONT_LC="$(printf '%s' "$FONT" | tr '[:upper:]' '[:lower:]')"
    if [[ -n "$MATCHED_FAMILY" && "$MATCHED_FAMILY_LC" != "$FONT_LC" ]]; then
      echo "Warning: font '$FONT' resolved to '$MATCHED_FAMILY' ($MATCHED)." >&2
    fi
  fi
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/gen-svg.XXXXXX")"
TMP="$WORK_DIR/input.svg"
trap 'rm -rf "$WORK_DIR"' EXIT

pango-view \
  --no-display \
  --background=transparent \
  --foreground=black \
  --font="$FONT $SIZE" \
  --text="$TEXT" \
  --output="$TMP"

ARGS=(--input "$TMP" --rand "$RAND")
if [[ -n "$OUTPUT" ]]; then
  ARGS+=(--output "$OUTPUT")
fi
if [[ -n "$SEED" ]]; then
  ARGS+=(--seed "$SEED")
fi

python3 -B "$HELPER" "${ARGS[@]}"
