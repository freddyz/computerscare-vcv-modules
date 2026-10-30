#pragma once

#include <string>
#include <vector>

namespace blunch {
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

enum class RandomMode { FollowSeed, AlwaysSeeded, AlwaysRandom };

enum class ClockLiteralKind {
  Empty,
  Numeric,
  Colon,
  RandomRange,
  ExternalClock
};

struct RandomChoiceAst {
  bool restChoice = false;
  SourceRange restRange;
  bool externalClockChoice = false;
  char externalClock = '\0';
  int probability = 100;
  bool skipOnMiss = false;
  SourceRange probabilityRange;
  double minValue = 0.0;
  double maxValue = 0.0;
  std::string minValueLexeme;
  std::string maxValueLexeme;
  ClockUnit unit = ClockUnit::Unknown;
  SourceRange unitRange;
  SourceRange range;
  bool hasRepeatModifier = false;
  int repeat = 1;
  bool repeatIsDuration = false;
  double repeatDurationSeconds = 0.0;
  bool repeatUsesExternalClock = false;
  char repeatExternalClock = '\0';
  SourceRange repeatRange;
  SourceRange repeatValueRange;
  bool hasTotalDurationModifier = false;
  bool totalDurationIsTickCount = false;
  int totalDurationTicks = 0;
  double totalDurationSeconds = 0.0;
  bool totalDurationUsesExternalClock = false;
  char totalDurationExternalClock = '\0';
  SourceRange totalDurationRange;
  SourceRange totalDurationValueRange;
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
  char externalClock = '\0';

  int minutes = 0;
  int seconds = 0;
  std::string minutesLexeme;
  std::string secondsLexeme;
};

struct ClockBlockAst {
  RandomMode randomMode = RandomMode::FollowSeed;
  std::vector<ClockLiteralAst> repeatSequence;
  ClockLiteralAst literal;
  ClockLiteralAst repeatRandom;
  ClockLiteralAst repeatDuration;
  ClockLiteralAst totalDurationRandom;
  ClockLiteralAst totalDuration;
  // A bracket sequence advances as one lane item inside parentheses.
  int sequenceGroupId = -1;
  int repeat = 1;
  int probability = 100;
  bool skipOnMiss = false;
  int totalDurationTicks = 0;
  int totalDurationGroupId = -1;
  SourceRange range;
  SourceRange restRange;
  SourceRange repeatRange;
  SourceRange repeatValueRange;
  SourceRange totalDurationRange;
  SourceRange totalDurationValueRange;
  SourceRange probabilityRange;
  bool rest = false;
  bool repeatIsDuration = false;
  bool repeatIsRandom = false;
  bool repeatIsSequence = false;
  bool repeatUsesExternalClock = false;
  char repeatExternalClock = '\0';
  bool repeatValueIsOwn = false;
  bool hasTotalDuration = false;
  bool totalDurationIsRandom = false;
  bool totalDurationIsTickCount = false;
  bool totalDurationUsesExternalClock = false;
  char totalDurationExternalClock = '\0';
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
}  // namespace blunch
