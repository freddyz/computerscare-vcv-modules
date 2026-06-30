#!/usr/bin/env bash
set -e

SRC_FILES=$(find src -name "*.cpp" -o -name "*.hpp")
CPP_FILES=$(find src -name "*.cpp")
CMD=${1:-fast}
RACK_DIR=${RACK_DIR:-$HOME/dev/VCV-Rack/Rack}
RACK_INCLUDE_DIR=${RACK_INCLUDE_DIR:-$RACK_DIR/include}
RACK_DEP_INCLUDE_DIR=${RACK_DEP_INCLUDE_DIR:-$RACK_DIR/dep/include}
CLANG_TIDY_CHECKS=${CLANG_TIDY_CHECKS:-clang-analyzer-*}

find_clang_tidy() {
  if [ -n "${CLANG_TIDY:-}" ]; then
    echo "$CLANG_TIDY"
    return
  fi

  if command -v clang-tidy >/dev/null 2>&1; then
    command -v clang-tidy
    return
  fi

  for candidate in \
    /opt/homebrew/opt/llvm/bin/clang-tidy \
    /usr/local/opt/llvm/bin/clang-tidy; do
    if [ -x "$candidate" ]; then
      echo "$candidate"
      return
    fi
  done

  echo ""
}

fmt() {
  echo "==> Formatting..."
  clang-format -i $SRC_FILES
  npm run format
  echo "    Done."
}

fmt_check() {
  echo "==> Checking format..."
  npm run format:check
  echo "    OK."
}

lint() {
  echo "==> Linting..."
  CLANG_TIDY=$(find_clang_tidy)
  if [ -z "$CLANG_TIDY" ]; then
    echo "clang-tidy not found. Install it with: brew install llvm" >&2
    exit 1
  fi

  "$CLANG_TIDY" --checks="$CLANG_TIDY_CHECKS" $CPP_FILES -- \
    -std=c++17 -I./src -I"$RACK_INCLUDE_DIR" -I"$RACK_DEP_INCLUDE_DIR"
  echo "    OK."
}

cppcheck_run() {
  echo "==> cppcheck..."
  cppcheck --enable=unusedFunction --suppress=missingIncludeSystem \
    -I./src -I"$RACK_INCLUDE_DIR" -I"$RACK_DEP_INCLUDE_DIR" \
    --suppress="*:$RACK_INCLUDE_DIR/*" \
    --suppress="*:$RACK_DEP_INCLUDE_DIR/*" \
    --max-configs=1 --error-exitcode=1 src/
  echo "    OK."
}

build() {
  echo "==> Building..."
  make
}

run_tests() {
  echo "==> Tests..."
  g++ -std=c++17 -o /tmp/cs_test src/test.cpp src/dtpulse.cpp && /tmp/cs_test
  g++ -std=c++11 -I src tests/polyphonic_mapping_test.cpp -o /tmp/cs_polyphonic_mapping_test && /tmp/cs_polyphonic_mapping_test
  g++ -std=c++11 -I src tests/text_editor_logic_test.cpp -o /tmp/cs_text_editor_logic_test && /tmp/cs_text_editor_logic_test
  g++ -std=c++11 -I src tests/cloly_pock_language_test.cpp src/ClolyPockLanguage/Tokenizer.cpp src/ClolyPockLanguage/Parser.cpp src/ClolyPockLanguage/Evaluator.cpp -o /tmp/cs_cloly_pock_language_test && /tmp/cs_cloly_pock_language_test
  echo "    OK."
}

test_all() {
  build
  run_tests
}

fast() {
  fmt
  test_all
}

check() {
  fast
}

full_check() {
  fmt_check
  test_all
}

usage() {
  echo "Usage: $0 [fmt|lint|cppcheck|build|fast|check|full-check|test|all|help]"
  echo ""
  echo "  fmt        Auto-format all src files in place"
  echo "  lint       Run clang-tidy static analysis"
  echo "  cppcheck   Run cppcheck unused-function analysis"
  echo "  build      Build the Rack plugin"
  echo "  fast       Auto-format + run tests for local iteration"
  echo "  check      Alias for fast local validation"
  echo "  full-check Prettier format check + tests"
  echo "  test       Build + run test binary"
  echo "  all        fmt + lint + cppcheck + build + test"
  echo "  help       Show this help"
}

case $CMD in
  fmt)      fmt ;;
  lint)     lint ;;
  cppcheck) cppcheck_run ;;
  build)    build ;;
  fast)     fast ;;
  check)    check ;;
  full-check) full_check ;;
  test)     test_all ;;
  all)      fmt && lint && cppcheck_run && test_all ;;
  help)     usage ;;
  *)
    usage
    exit 1
    ;;
esac

echo ""
echo "Done."
