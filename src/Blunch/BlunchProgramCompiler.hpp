#pragma once

#include <string>
#include <vector>

#include "BlunchSequencerRuntime.hpp"

namespace blunch {
namespace program {

typedef int (*ExternalClockInputMapper)(char clock);

bool randomChoiceHasExplicitUnit(
    const blunch::language::RandomChoiceAst& choice);
bool randomChoiceIsExternalClock(
    const blunch::language::RandomChoiceAst& choice);
bool randomChoiceHasModifiers(const blunch::language::RandomChoiceAst& choice);

blunch::language::ClockLiteralAst literalForRandomChoice(
    const blunch::language::ClockLiteralAst& randomAst,
    const blunch::language::RandomChoiceAst& choice, double value);

void captureBaseStepState(BlunchProgramStep& step);
void restoreBaseStepState(BlunchProgramStep& step);
bool isBranchLocalTotalDuration(const BlunchProgramStep& step);

bool compileLineProgram(const std::string& lineText, int lineBegin,
                        ExternalClockInputMapper externalClockInputIndex,
                        std::vector<BlunchProgramStep>& program);

}  // namespace program
}  // namespace blunch
