#pragma once

#include <string>
#include <vector>

namespace cloly {
namespace language {

struct SourceRange {
  int begin = 0;
  int end = 0;
};

struct Diagnostic {
  std::string message;
  SourceRange range;
};

enum class ClockUnit { Unknown, Bpm, Hertz, Milliseconds, Seconds, Minutes };

enum class ClockLiteralKind { Empty, Numeric, Colon };

struct ClockLiteralAst {
  ClockLiteralKind kind = ClockLiteralKind::Empty;
  ClockUnit unit = ClockUnit::Unknown;
  SourceRange range;
  SourceRange unitRange;

  double value = 0.0;
  std::string valueLexeme;

  int minutes = 0;
  int seconds = 0;
  std::string minutesLexeme;
  std::string secondsLexeme;
};

struct ParseResult {
  ClockLiteralAst ast;
  std::vector<Diagnostic> diagnostics;

  bool ok() const { return diagnostics.empty(); }
};

std::string clockUnitName(ClockUnit unit);

}  // namespace language
}  // namespace cloly
