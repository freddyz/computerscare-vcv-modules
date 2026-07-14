#include "Blunch/BlunchProgramCompiler.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

void requireNear(double actual, double expected, const char* message) {
  if (std::fabs(actual - expected) > 0.000001) {
    std::fprintf(stderr, "FAIL: %s expected %.9f got %.9f\n", message,
                 expected, actual);
    std::exit(1);
  }
}

int mapExternalClock(char clock) {
  switch (clock) {
    case 'w':
      return 10;
    case 'x':
      return 11;
    case 'y':
      return 12;
    case 'z':
      return 13;
    default:
      return -1;
  }
}

std::vector<BlunchProgramStep> requireCompiles(const std::string& source,
                                               int lineBegin = 0) {
  std::vector<BlunchProgramStep> program;
  require(blunch::program::compileLineProgram(source, lineBegin,
                                              mapExternalClock, program),
          "program should compile");
  require(!program.empty(), "compiled program should have steps");
  return program;
}

}  // namespace

int main() {
  namespace language = blunch::language;
  namespace program = blunch::program;

  std::vector<BlunchProgramStep> compiled = requireCompiles("x@3y", 40);
  require(compiled.size() == 1, "external repeat compiles to one step");
  require(compiled[0].externalClockInput == 11,
          "external literal maps through compiler callback");
  require(compiled[0].repeatExternalClockInput == 12,
          "external repeat maps through compiler callback");
  require(compiled[0].repeat == 3, "external repeat count is preserved");
  require(compiled[0].repeatHighlightBegin == 42,
          "repeat highlight is offset by line begin");
  require(compiled[0].repeatHighlightEnd == 44,
          "repeat highlight end is offset by line begin");

  compiled = requireCompiles("{1.33hz|~2.0hz|4.25hz}");
  require(compiled.size() == 1, "random rest choice compiles to one step");
  require(compiled[0].hasRandomValue, "random rest step is marked random");
  require(compiled[0].literal.randomChoices.size() == 3,
          "random rest choices are preserved");
  require(!compiled[0].literal.randomChoices[0].restChoice,
          "first random choice is not a rest");
  require(compiled[0].literal.randomChoices[1].restChoice,
          "middle random choice is a rest");
  requireNear(compiled[0].spec.hz, 1.33,
              "compiled random step uses first numeric choice as base spec");

  compiled = requireCompiles("{~4?|~5?86}");
  require(compiled.size() == 1,
          "random rest choice probability compiles to one step");
  require(compiled[0].literal.randomChoices.size() == 2,
          "random rest choice probabilities are preserved");
  require(compiled[0].literal.randomChoices[0].probability == 50,
          "bare random choice question defaults to 50 percent");
  require(compiled[0].literal.randomChoices[1].probability == 86,
          "explicit random choice probability is preserved");

  compiled = requireCompiles(
      "{{3.3333hz|~4.hz}|{1.0hz|1.75hz|3.333333hz}|"
      "{1.75hz|~1.5hz|4.25hz}}");
  require(compiled.size() == 1, "nested random choices compile to one step");
  require(compiled[0].literal.randomChoices.size() == 8,
          "nested random choices flatten into runtime choices");
  require(compiled[0].literal.randomChoices[1].restChoice,
          "nested random preserves first rest choice");
  require(compiled[0].literal.randomChoices[6].restChoice,
          "nested random preserves second rest choice");

  compiled = requireCompiles(
      "[{(1.75 ~4.66 1.33 ~3. 2.666666 ~5)|(~4.75 ~2 ~1 "
      "1.75)}]hz#7");
  require(compiled.size() == 6,
          "random sequence spread compiles to one step per row");
  for (size_t i = 0; i < compiled.size(); i++) {
    require(compiled[i].hasRandomValue,
            "random sequence row is a random runtime step");
    require(compiled[i].literal.unit == language::ClockUnit::Hertz,
            "random sequence suffix unit reaches every row");
    require(compiled[i].literal.randomChoices.size() == 2,
            "random sequence row has one choice per lane");
    require(compiled[i].hasTotalDurationGroup,
            "random sequence row is in total duration group");
    require(compiled[i].totalDurationIsTickCount,
            "random sequence group is tick-count based");
    require(compiled[i].totalDurationTicks == 7,
            "random sequence group tick count is preserved");
    require(compiled[i].totalDurationGroupStart == 0,
            "random sequence group starts at first row");
    require(compiled[i].totalDurationGroupEnd == 6,
            "random sequence group ends after final row");
  }
  require(compiled[0].literal.randomChoices[1].restChoice,
          "random sequence preserves rest on alternate lane");
  requireNear(compiled[4].literal.randomChoices[0].minValue, 2.666666,
              "random sequence preserves long lane row value");
  requireNear(compiled[4].literal.randomChoices[1].minValue, 4.75,
              "random sequence wraps short lane row value");

  compiled = requireCompiles("[{3-6}hz#8 {3-5}hz#8]@2");
  require(compiled.size() == 2,
          "bracketed total-duration blocks compile to two steps");
  for (size_t i = 0; i < compiled.size(); i++) {
    require(compiled[i].repeat == 2,
            "outer repeat applies to total-duration block");
    require(compiled[i].hasTotalDurationGroup,
            "total-duration block keeps its cycle window");
    require(compiled[i].totalDurationIsTickCount,
            "total-duration block remains tick-count based");
    require(compiled[i].totalDurationTicks == 8,
            "total-duration block keeps tick count");
    require(compiled[i].totalDurationGroupStart == (int)i,
            "per-block total duration starts on that block");
    require(compiled[i].totalDurationGroupEnd == (int)i + 1,
            "per-block total duration ends after that block");
  }

  compiled = requireCompiles("[{3-6|~2}hz#8 {2-7}hz#8]@3");
  require(compiled.size() == 2,
          "rest random total-duration blocks compile to two steps");
  for (size_t i = 0; i < compiled.size(); i++) {
    require(compiled[i].repeat == 3,
            "outer repeat applies to rest random total-duration block");
    require(compiled[i].repeatHighlightBegin == 25,
            "rest random total-duration repeat highlight starts at count");
    require(compiled[i].repeatHighlightEnd == 26,
            "rest random total-duration repeat highlight ends after count");
    require(compiled[i].hasTotalDurationGroup,
            "rest random total-duration block keeps cycle window");
    require(compiled[i].totalDurationTicks == 8,
            "rest random total-duration block keeps tick count");
  }
  require(compiled[0].literal.randomChoices.size() == 2,
          "rest random total-duration first block keeps choices");
  require(compiled[0].literal.randomChoices[1].restChoice,
          "rest random total-duration first block preserves rest choice");

  BlunchProgramStep step;
  step.hasTotalDurationGroup = true;
  require(!program::isBranchLocalTotalDuration(step),
          "plain total duration group is not branch local");
  step.totalDurationBranchLocal = true;
  require(program::isBranchLocalTotalDuration(step),
          "explicit branch-local flag marks branch-local duration");
  program::captureBaseStepState(step);
  step.totalDurationBranchLocal = false;
  program::restoreBaseStepState(step);
  require(program::isBranchLocalTotalDuration(step),
          "base-state restore preserves branch-local duration flag");

  std::puts("blunch program compiler tests passed");
  return 0;
}
