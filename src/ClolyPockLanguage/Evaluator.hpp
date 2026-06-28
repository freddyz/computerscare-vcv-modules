#pragma once

#include <vector>

#include "Ast.hpp"

namespace cloly {
namespace language {

struct ClockSpec {
  double bpm = 0.0;
  double hz = 0.0;
  double periodSeconds = 0.0;
};

struct EvaluationResult {
  ClockSpec spec;
  std::vector<Diagnostic> diagnostics;

  bool ok() const { return diagnostics.empty(); }
};

EvaluationResult evaluateClockLiteral(const ClockLiteralAst& ast);

}  // namespace language
}  // namespace cloly
