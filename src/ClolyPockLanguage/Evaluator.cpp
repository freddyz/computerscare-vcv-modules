#include "Evaluator.hpp"

namespace cloly {
namespace language {

namespace {
void addDiagnostic(EvaluationResult& result, const std::string& message,
                   SourceRange range) {
  Diagnostic diagnostic;
  diagnostic.message = message;
  diagnostic.range = range;
  result.diagnostics.push_back(diagnostic);
}

void setFromPeriodSeconds(EvaluationResult& result, double periodSeconds) {
  result.spec.periodSeconds = periodSeconds;
  result.spec.hz = 1.0 / periodSeconds;
  result.spec.bpm = result.spec.hz * 60.0;
}
}  // namespace

EvaluationResult evaluateClockLiteral(const ClockLiteralAst& ast) {
  EvaluationResult result;

  if (ast.kind == ClockLiteralKind::Empty || ast.unit == ClockUnit::Unknown) {
    addDiagnostic(result, "Cannot evaluate invalid clock literal", ast.range);
    return result;
  }

  double value = ast.value;
  if (ast.kind == ClockLiteralKind::Colon) {
    value = ast.minutes * 60.0 + ast.seconds;
  }

  if (value <= 0.0) {
    addDiagnostic(result, "Clock value must be greater than zero", ast.range);
    return result;
  }

  switch (ast.unit) {
    case ClockUnit::Bpm:
      result.spec.bpm = value;
      result.spec.hz = value / 60.0;
      result.spec.periodSeconds = 60.0 / value;
      break;
    case ClockUnit::Hertz:
      result.spec.hz = value;
      result.spec.bpm = value * 60.0;
      result.spec.periodSeconds = 1.0 / value;
      break;
    case ClockUnit::Millihertz:
      result.spec.hz = value / 1000.0;
      result.spec.bpm = result.spec.hz * 60.0;
      result.spec.periodSeconds = 1.0 / result.spec.hz;
      break;
    case ClockUnit::Kilohertz:
      result.spec.hz = value * 1000.0;
      result.spec.bpm = result.spec.hz * 60.0;
      result.spec.periodSeconds = 1.0 / result.spec.hz;
      break;
    case ClockUnit::Milliseconds:
      setFromPeriodSeconds(result, value / 1000.0);
      break;
    case ClockUnit::Seconds:
      setFromPeriodSeconds(result, value);
      break;
    case ClockUnit::Minutes:
      if (ast.kind == ClockLiteralKind::Colon) {
        setFromPeriodSeconds(result, value);
      } else {
        setFromPeriodSeconds(result, value * 60.0);
      }
      break;
    case ClockUnit::Unknown:
      addDiagnostic(result, "Cannot evaluate unknown clock unit",
                    ast.unitRange);
      break;
  }

  return result;
}

}  // namespace language
}  // namespace cloly
