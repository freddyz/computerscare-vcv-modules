#!/usr/bin/env bash
set -e

SRC_FILES=$(find src -name "*.cpp" -o -name "*.hpp")
CMD=${1:-all}

fmt() {
  echo "==> Formatting..."
  clang-format -i $SRC_FILES
  echo "    Done."
}

fmt_check() {
  echo "==> Checking format..."
  clang-format --dry-run --Werror $SRC_FILES
  echo "    OK."
}

lint() {
  echo "==> Linting..."
  clang-tidy $SRC_FILES -- -std=c++17 -I./src -I${RACK_DIR:-../../Rack}/include
  echo "    OK."
}

cppcheck_run() {
  echo "==> cppcheck..."
  cppcheck --enable=unusedFunction --suppress=missingIncludeSystem \
    -I./src -I${RACK_DIR:-../../Rack}/include \
    --suppress="*:${RACK_DIR:-../../Rack}/include/*" \
    --max-configs=1 --error-exitcode=1 src/
  echo "    OK."
}

build() {
  echo "==> Building..."
  (cd /Users/adammalone/dev/VCV-Rack/Rack/plugins/computerscare-vcv-modules && make)
}

run_tests() {
  echo "==> Tests..."
  g++ -std=c++17 -o /tmp/cs_test src/test.cpp src/dtpulse.cpp && /tmp/cs_test
  echo "    OK."
}

case $CMD in
  fmt)      fmt ;;
  lint)     lint ;;
  cppcheck) cppcheck_run ;;
  build)    build ;;
  check)    fmt_check && lint && build ;;
  test)     build && run_tests ;;
  all)      fmt && lint && build && run_tests ;;
  *)
    echo "Usage: $0 [fmt|lint|check|test|all]"
    echo ""
    echo "  fmt      Auto-format all src files in place"
    echo "  lint     Run clang-tidy static analysis"
    echo "  cppcheck Run cppcheck unused-function analysis"
    echo "  check    Dry-run format check + lint + cppcheck + build"
    echo "  test     Build + run test binary"
    echo "  all      fmt + lint + cppcheck + build + test (default)"
    exit 1
    ;;
esac

echo ""
echo "Done."
