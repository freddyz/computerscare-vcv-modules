#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: svgt.sh TEXT [RAND]

Examples:
  svgt.sh routing
  svgt.sh routing 20
  svgt.sh "horse friend" 20
EOF
}

if [[ $# -eq 1 && ( "$1" == "-h" || "$1" == "--help" ) ]]; then
  usage
  exit 0
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage >&2
  exit 1
fi

TEXT="$1"
RAND="${2:-}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

NAME="$(printf '%s' "$TEXT" \
  | tr '[:upper:]' '[:lower:]' \
  | sed 's/[^[:alnum:]_.-][^[:alnum:]_.-]*/-/g; s/^[._-]*//; s/[._-]*$//')"
if [[ -z "$NAME" ]]; then
  NAME="text"
fi

OUT_DIR="$SCRIPT_DIR/gen/svg_out"
mkdir -p "$OUT_DIR"

SUFFIX="$(od -An -N2 -tx1 /dev/urandom | tr -d ' \n')"
if [[ -z "$SUFFIX" ]]; then
  SUFFIX="$(date +%s | tail -c 5)"
fi

OUTPUT="$OUT_DIR/$NAME-$SUFFIX.svg"
ARGS=("$TEXT")
if [[ -n "$RAND" ]]; then
  ARGS+=("--rand=$RAND")
fi

"$SCRIPT_DIR/gen.sh" "${ARGS[@]}" > "$OUTPUT"

printf '%s\n' "$OUTPUT"
