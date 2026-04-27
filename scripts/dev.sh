#!/usr/bin/env bash
# Dev helper. Most tasks need RACK_DIR pointing at a VCV Rack SDK; the
# `sandbox` task does not, and is the one to use in agent loops or sandboxed
# CI environments where the SDK isn't installed.

set -e

cd "$(dirname "$0")/.."
SRC_FILES=$(find src -name "*.cpp" -o -name "*.hpp")
CMD=${1:-help}

require_sdk() {
  if [[ -z "${RACK_DIR:-}" || ! -f "${RACK_DIR}/include/rack.hpp" ]]; then
    echo "ERROR: RACK_DIR not set or doesn't contain include/rack.hpp." >&2
    echo "  Run: scripts/fetch-rack-sdk.sh   (sets up \$HOME/Rack-SDK)" >&2
    echo "  Then: export RACK_DIR=\$HOME/Rack-SDK" >&2
    exit 1
  fi
}

fmt() {
  echo "==> Formatting..."
  clang-format -i $SRC_FILES
}

fmt_check() {
  echo "==> Checking format..."
  clang-format --dry-run --Werror $SRC_FILES
}

lint() {
  require_sdk
  echo "==> Linting..."
  clang-tidy $SRC_FILES -- -std=c++17 -I./src -I"$RACK_DIR/include" -I"$RACK_DIR/dep/include"
}

cppcheck_run() {
  echo "==> cppcheck..."
  cppcheck --enable=unusedFunction --suppress=missingIncludeSystem \
    -I./src ${RACK_DIR:+-I"$RACK_DIR/include"} \
    --max-configs=1 --error-exitcode=1 src/
}

build() {
  require_sdk
  echo "==> Building..."
  make -j"$(nproc 2>/dev/null || echo 4)"
}

run_tests() {
  echo "==> Tests..."
  g++ -std=c++17 -o /tmp/cs_test src/test.cpp src/dtpulse.cpp && /tmp/cs_test
}

sandbox() {
  exec scripts/sandbox-check.sh
}

usage() {
  cat <<EOF
Usage: $0 <command>

Sandbox-friendly (no SDK):
  sandbox    Run scripts/sandbox-check.sh  (use this in agent loops)
  fmt        Format src/ in place with clang-format
  fmt-check  Verify formatting without modifying

Need RACK_DIR pointing at a VCV Rack SDK:
  lint       clang-tidy
  cppcheck   cppcheck unused-function analysis
  build      make
  test       Build then run /tmp/cs_test
  check      fmt-check + lint + build
  all        fmt + lint + build + test
EOF
}

case "$CMD" in
  fmt)        fmt ;;
  fmt-check)  fmt_check ;;
  lint)       lint ;;
  cppcheck)   cppcheck_run ;;
  build)      build ;;
  test)       build && run_tests ;;
  sandbox)    sandbox ;;
  check)      fmt_check && lint && build ;;
  all)        fmt && lint && build && run_tests ;;
  help|*)     usage; [[ "$CMD" == "help" ]] && exit 0 || exit 1 ;;
esac

echo "Done."
