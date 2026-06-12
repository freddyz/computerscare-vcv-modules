#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: svgp.sh [RAND] [options]

Examples:
  svgp.sh
  svgp.sh 40
  svgp.sh --aspect 3 --height 80 --angle 30
  svgp.sh --rand 40 --color 180 --border 1 --border-color black
EOF
}

if [[ $# -eq 1 && ( "$1" == "-h" || "$1" == "--help" ) ]]; then
  usage
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$SCRIPT_DIR/gen/svg_out"
mkdir -p "$OUT_DIR"

SUFFIX="$(od -An -N2 -tx1 /dev/urandom | tr -d ' \n')"
if [[ -z "$SUFFIX" ]]; then
  SUFFIX="$(date +%s | tail -c 5)"
fi

OUTPUT="$OUT_DIR/perspective-rect-$SUFFIX.svg"

ARGS=()
if [[ $# -gt 0 && "$1" != -* ]]; then
  ARGS+=("--rand" "$1")
  shift
fi

if [[ ${#ARGS[@]} -gt 0 ]]; then
  python3 -B "$SCRIPT_DIR/svg_fake_perspective_rect.py" "${ARGS[@]}" "$@" --output "$OUTPUT"
else
  python3 -B "$SCRIPT_DIR/svg_fake_perspective_rect.py" "$@" --output "$OUTPUT"
fi

printf '%s\n' "$OUTPUT"
