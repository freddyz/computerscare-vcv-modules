#!/usr/bin/env bash
set -e

SRC_FILES=$(find src -name "*.cpp" -o -name "*.hpp")
CPP_FILES=$(find src -name "*.cpp")
CMD=${1:-fast}
RACK_DIR=${RACK_DIR:-$HOME/dev/VCV-Rack/Rack}
RACK_INCLUDE_DIR=${RACK_INCLUDE_DIR:-$RACK_DIR/include}
RACK_DEP_INCLUDE_DIR=${RACK_DEP_INCLUDE_DIR:-$RACK_DIR/dep/include}
CLANG_TIDY_CHECKS=${CLANG_TIDY_CHECKS:-clang-analyzer-*}
CPPCHECK_ISSUE_ENABLE=${CPPCHECK_ISSUE_ENABLE:-all}
ANALYSIS_JOBS=${ANALYSIS_JOBS:-}

analysis_jobs() {
  if [ -n "$ANALYSIS_JOBS" ]; then
    echo "$ANALYSIS_JOBS"
    return
  fi

  if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc 2>/dev/null || true)
    if [ -n "$JOBS" ]; then
      echo "$JOBS"
      return
    fi
  fi

  if command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu 2>/dev/null || true)
    if [ -n "$JOBS" ]; then
      echo "$JOBS"
      return
    fi
  fi

  echo 4
}

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

clang_tidy_project() {
  echo "==> clang-tidy project analysis..."
  CLANG_TIDY=$(find_clang_tidy)
  if [ -z "$CLANG_TIDY" ]; then
    echo "clang-tidy not found. Install it with: brew install llvm" >&2
    exit 1
  fi

  "$CLANG_TIDY" --checks="$CLANG_TIDY_CHECKS" --system-headers=false \
    --header-filter="^$(pwd)/src/.*" $CPP_FILES -- \
    -std=c++17 -I./src -I"$RACK_INCLUDE_DIR" -I"$RACK_DEP_INCLUDE_DIR"
  echo "    OK."
}

clang_tidy_vcv_library() {
  echo "==> clang-tidy VCV Library-style analysis..."
  CLANG_TIDY=$(find_clang_tidy)
  if [ -z "$CLANG_TIDY" ]; then
    echo "clang-tidy not found. Install it with: brew install llvm" >&2
    exit 1
  fi

  JOBS=$(analysis_jobs)
  CLANG_TIDY_VCV_CHECKS="$CLANG_TIDY_CHECKS,-clang-analyzer-security.insecureAPI.rand"
  CLANG_TIDY_HEADER_FILTER="^$(pwd)/src/.*"
  printf "%s\n" $CPP_FILES | xargs -n 1 -P "$JOBS" sh -c '
    "$1" --checks="$2" --system-headers=false --header-filter="$3" "$6" -- \
      -std=c++17 -I./src -I"$4" -I"$5"
  ' sh "$CLANG_TIDY" "$CLANG_TIDY_VCV_CHECKS" "$CLANG_TIDY_HEADER_FILTER" \
    "$RACK_INCLUDE_DIR" "$RACK_DEP_INCLUDE_DIR" 2>&1 |
    awk '
      /^.*\/src\/.* warning: / {
        sub(/^.*\/src\//, "src/")
        print
        found = 1
        next
      }
      /^src\/.* warning: / {
        print
        found = 1
      }
      END { exit found ? 1 : 0 }
    '
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

cppcheck_vcv_library() {
  echo "==> cppcheck VCV Library-style analysis..."
  cppcheck --enable="$CPPCHECK_ISSUE_ENABLE" --suppress=missingIncludeSystem \
    -j "$(analysis_jobs)" --max-configs=1 --template="{file}:{line}: warning: {message} [{id}]" \
    $SRC_FILES 2>&1 |
    awk '
      /\[(uninitMemberVar|uninitMemberVarNoCtor|uninitDerivedMemberVar|duplInheritedMember|nullPointerRedundantCheck|invalidPrintfArgType_sint)\]$/ {
        print
        found = 1
      }
      END { exit found ? 1 : 0 }
    '
  echo "    OK."
}

static_analysis() {
  set +e
  cppcheck_vcv_library
  CPPCHECK_STATUS=$?
  clang_tidy_vcv_library
  CLANG_TIDY_STATUS=$?
  set -e

  if [ "$CPPCHECK_STATUS" -ne 0 ] || [ "$CLANG_TIDY_STATUS" -ne 0 ]; then
    exit 1
  fi
}

build() {
  echo "==> Building..."
  make
}

run_tests() {
  echo "==> Tests..."
  g++ -std=c++17 -o /tmp/cs_test src/test.cpp src/dtpulse.cpp && /tmp/cs_test
  g++ -std=c++11 -I src tests/polyphonic_mapping_test.cpp -o /tmp/cs_polyphonic_mapping_test && /tmp/cs_polyphonic_mapping_test
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
  echo "Usage: $0 [fmt|lint|clang-tidy-project|clang-tidy-vcv-library|cppcheck|cppcheck-vcv-library|static|build|fast|check|full-check|test|all|help]"
  echo ""
  echo "  fmt                 Auto-format all src files in place"
  echo "  lint                Run clang-tidy static analysis"
  echo "  clang-tidy-project  Run clang-tidy with project-focused header filtering"
  echo "  clang-tidy-vcv-library Run clang-tidy with settings close to VCV Library output"
  echo "  cppcheck            Run cppcheck unused-function analysis"
  echo "  cppcheck-vcv-library Run cppcheck with settings close to VCV Library output"
  echo "  static              Run cppcheck-vcv-library + clang-tidy-vcv-library"
  echo "  build               Build the Rack plugin"
  echo "  fast                Auto-format + run tests for local iteration"
  echo "  check               Alias for fast local validation"
  echo "  full-check          Prettier format check + tests"
  echo "  test                Build + run test binary"
  echo "  all                 fmt + lint + cppcheck + build + test"
  echo "  help                Show this help"
}

case $CMD in
  fmt)                fmt ;;
  lint)               lint ;;
  clang-tidy-project) clang_tidy_project ;;
  clang-tidy-vcv-library) clang_tidy_vcv_library ;;
  cppcheck)           cppcheck_run ;;
  cppcheck-vcv-library) cppcheck_vcv_library ;;
  static)             static_analysis ;;
  build)              build ;;
  fast)               fast ;;
  check)              check ;;
  full-check)         full_check ;;
  test)               test_all ;;
  all)                fmt && lint && cppcheck_run && test_all ;;
  help)               usage ;;
  *)
    usage
    exit 1
    ;;
esac

echo ""
echo "Done."
