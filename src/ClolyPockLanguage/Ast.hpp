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

enum class ClockUnit {
  Unknown,
  Bpm,
  Hertz,
  Millihertz,
  Kilohertz,
  Milliseconds,
  Seconds,
  Minutes
};

enum class ClockLiteralKind { Empty, Numeric, Colon, RandomRange };

struct RandomChoiceAst {
  double minValue = 0.0;
  double maxValue = 0.0;
  std::string minValueLexeme;
  std::string maxValueLexeme;
  SourceRange range;
};

struct ClockLiteralAst {
  ClockLiteralKind kind = ClockLiteralKind::Empty;
  ClockUnit unit = ClockUnit::Unknown;
  SourceRange range;
  SourceRange unitRange;

  double value = 0.0;
  std::string valueLexeme;
  double minValue = 0.0;
  double maxValue = 0.0;
  std::string minValueLexeme;
  std::string maxValueLexeme;
  std::vector<RandomChoiceAst> randomChoices;

  int minutes = 0;
  int seconds = 0;
  std::string minutesLexeme;
  std::string secondsLexeme;
};

struct ClockBlockAst {
  ClockLiteralAst literal;
  int repeat = 1;
  bool repeatIsDuration = false;
  ClockLiteralAst repeatDuration;
  int probability = 100;
  SourceRange range;
  SourceRange repeatRange;
  SourceRange repeatValueRange;
  bool repeatValueIsOwn = false;
  SourceRange probabilityRange;
};

struct ClockProgramAst {
  std::vector<ClockBlockAst> blocks;
  SourceRange range;
};

struct ParseResult {
  ClockLiteralAst ast;
  ClockProgramAst program;
  std::vector<Diagnostic> diagnostics;

  bool ok() const { return diagnostics.empty(); }
};

std::string clockUnitName(ClockUnit unit);

}  // namespace language
}  // namespace cloly
