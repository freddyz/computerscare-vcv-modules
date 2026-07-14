#include "BlunchProgramCompiler.hpp"

#include <algorithm>

namespace blunch {
namespace program {

namespace {
const float DEFAULT_CLOCK_BPM = 120.f;
const float DEFAULT_CLOCK_HZ = DEFAULT_CLOCK_BPM / 60.f;

int mapExternalClock(ExternalClockInputMapper mapper, char clock) {
  return mapper ? mapper(clock) : -1;
}
}  // namespace

bool randomChoiceHasExplicitUnit(
    const blunch::language::RandomChoiceAst& choice) {
  return choice.unitRange.end > choice.unitRange.begin;
}

bool randomChoiceIsExternalClock(
    const blunch::language::RandomChoiceAst& choice) {
  return choice.externalClockChoice;
}

bool randomChoiceHasModifiers(const blunch::language::RandomChoiceAst& choice) {
  return choice.hasRepeatModifier || choice.hasTotalDurationModifier;
}

blunch::language::ClockLiteralAst literalForRandomChoice(
    const blunch::language::ClockLiteralAst& randomAst,
    const blunch::language::RandomChoiceAst& choice, double value) {
  blunch::language::ClockLiteralAst literal;
  if (randomChoiceIsExternalClock(choice)) {
    literal.kind = blunch::language::ClockLiteralKind::ExternalClock;
    literal.externalClock = choice.externalClock;
    literal.range = choice.range;
    return literal;
  }

  literal.kind = blunch::language::ClockLiteralKind::Numeric;
  literal.value = value;
  literal.valueLexeme = choice.minValueLexeme;
  literal.range = choice.range;
  literal.unit =
      randomChoiceHasExplicitUnit(choice) ? choice.unit : randomAst.unit;
  literal.unitRange = randomChoiceHasExplicitUnit(choice) ? choice.unitRange
                                                          : randomAst.unitRange;
  return literal;
}

void captureBaseStepState(BlunchProgramStep& step) {
  step.baseIsRest = step.isRest;
  step.baseRepeat = step.repeat;
  step.baseRepeatExternalClockInput = step.repeatExternalClockInput;
  step.baseHasDuration = step.hasDuration;
  step.baseDurationSeconds = step.durationSeconds;
  step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
  step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
  step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
  step.baseHasTotalDurationGroup = step.hasTotalDurationGroup;
  step.baseTotalDurationBranchLocal = step.totalDurationBranchLocal;
  step.baseTotalDurationGroupId = step.totalDurationGroupId;
  step.baseTotalDurationGroupStart = step.totalDurationGroupStart;
  step.baseTotalDurationGroupEnd = step.totalDurationGroupEnd;
  step.baseTotalDurationIsRandom = step.totalDurationIsRandom;
  step.baseTotalDurationIsTickCount = step.totalDurationIsTickCount;
  step.baseTotalDurationTicks = step.totalDurationTicks;
  step.baseTotalDurationExternalClockInput =
      step.totalDurationExternalClockInput;
  step.baseTotalDurationSeconds = step.totalDurationSeconds;
  step.baseTotalDurationHighlightBegin = step.totalDurationHighlightBegin;
  step.baseTotalDurationHighlightEnd = step.totalDurationHighlightEnd;
}

void restoreBaseStepState(BlunchProgramStep& step) {
  step.isRest = step.baseIsRest;
  step.repeat = step.baseRepeat;
  step.repeatExternalClockInput = step.baseRepeatExternalClockInput;
  step.hasDuration = step.baseHasDuration;
  step.durationSeconds = step.baseDurationSeconds;
  step.repeatHighlightBegin = step.baseRepeatHighlightBegin;
  step.repeatHighlightEnd = step.baseRepeatHighlightEnd;
  step.repeatHighlightIsOwn = step.baseRepeatHighlightIsOwn;
  step.hasTotalDurationGroup = step.baseHasTotalDurationGroup;
  step.totalDurationBranchLocal = step.baseTotalDurationBranchLocal;
  step.totalDurationGroupId = step.baseTotalDurationGroupId;
  step.totalDurationGroupStart = step.baseTotalDurationGroupStart;
  step.totalDurationGroupEnd = step.baseTotalDurationGroupEnd;
  step.totalDurationIsRandom = step.baseTotalDurationIsRandom;
  step.totalDurationIsTickCount = step.baseTotalDurationIsTickCount;
  step.totalDurationTicks = step.baseTotalDurationTicks;
  step.totalDurationExternalClockInput =
      step.baseTotalDurationExternalClockInput;
  step.totalDurationSeconds = step.baseTotalDurationSeconds;
  step.totalDurationHighlightBegin = step.baseTotalDurationHighlightBegin;
  step.totalDurationHighlightEnd = step.baseTotalDurationHighlightEnd;
}

bool isBranchLocalTotalDuration(const BlunchProgramStep& step) {
  return step.hasTotalDurationGroup && step.totalDurationBranchLocal;
}

bool compileLineProgram(const std::string& lineText, int lineBegin,
                        ExternalClockInputMapper externalClockInputIndex,
                        std::vector<BlunchProgramStep>& program) {
  blunch::language::ParseResult parse =
      blunch::language::parseClockLiteral(lineText);
  if (!parse.ok()) {
    return false;
  }

  program.clear();
  for (size_t i = 0; i < parse.program.blocks.size(); i++) {
    const blunch::language::ClockBlockAst& block = parse.program.blocks[i];

    BlunchProgramStep step;
    step.sourceLineBegin = lineBegin;
    step.literal = block.literal;
    step.isRest = block.rest;
    step.spec.bpm = DEFAULT_CLOCK_BPM;
    step.spec.hz = DEFAULT_CLOCK_HZ;
    step.spec.periodSeconds = 1.f / DEFAULT_CLOCK_HZ;
    if (block.literal.kind ==
        blunch::language::ClockLiteralKind::ExternalClock) {
      step.externalClockInput = mapExternalClock(externalClockInputIndex,
                                                 block.literal.externalClock);
    } else if (block.literal.kind ==
               blunch::language::ClockLiteralKind::RandomRange) {
      bool hasNumericChoice = false;
      for (size_t choiceIndex = 0;
           choiceIndex < block.literal.randomChoices.size(); choiceIndex++) {
        if (!randomChoiceIsExternalClock(
                block.literal.randomChoices[choiceIndex])) {
          hasNumericChoice = true;
          break;
        }
      }
      if (hasNumericChoice) {
        for (size_t choiceIndex = 0;
             choiceIndex < block.literal.randomChoices.size(); choiceIndex++) {
          const blunch::language::RandomChoiceAst& choice =
              block.literal.randomChoices[choiceIndex];
          if (randomChoiceIsExternalClock(choice)) {
            continue;
          }
          blunch::language::ClockLiteralAst literal =
              literalForRandomChoice(block.literal, choice, choice.minValue);
          blunch::language::EvaluationResult eval =
              blunch::language::evaluateClockLiteral(literal);
          if (!eval.ok()) {
            program.clear();
            return false;
          }
          step.spec = eval.spec;
          break;
        }
      }
    } else if (block.literal.kind !=
               blunch::language::ClockLiteralKind::Empty) {
      blunch::language::EvaluationResult eval =
          blunch::language::evaluateClockLiteral(block.literal);
      if (!eval.ok()) {
        program.clear();
        return false;
      }
      step.spec = eval.spec;
    }
    step.hasRandomValue =
        block.literal.kind == blunch::language::ClockLiteralKind::RandomRange;
    if (step.hasRandomValue) {
      for (size_t choiceIndex = 0;
           choiceIndex < block.literal.randomChoices.size(); choiceIndex++) {
        const blunch::language::RandomChoiceAst& choice =
            block.literal.randomChoices[choiceIndex];
        if (randomChoiceIsExternalClock(choice)) {
          continue;
        }
        blunch::language::ClockLiteralAst minLiteral =
            literalForRandomChoice(block.literal, choice, choice.minValue);
        blunch::language::ClockLiteralAst maxLiteral =
            literalForRandomChoice(block.literal, choice, choice.maxValue);
        blunch::language::EvaluationResult minEval =
            blunch::language::evaluateClockLiteral(minLiteral);
        blunch::language::EvaluationResult maxEval =
            blunch::language::evaluateClockLiteral(maxLiteral);
        if (!minEval.ok() || !maxEval.ok()) {
          program.clear();
          return false;
        }
      }
    }
    step.repeat = std::max(1, block.repeat);
    step.repeatIsRandom = block.repeatIsRandom;
    step.repeatRandomIsDuration = block.repeatIsDuration;
    step.repeatRandom = block.repeatRandom;
    if (block.repeatUsesExternalClock) {
      step.repeatExternalClockInput =
          mapExternalClock(externalClockInputIndex, block.repeatExternalClock);
    }
    if (block.repeatIsDuration && !block.repeatIsRandom) {
      blunch::language::EvaluationResult durationEval =
          blunch::language::evaluateClockLiteral(block.repeatDuration);
      if (!durationEval.ok()) {
        program.clear();
        return false;
      }
      step.hasDuration = true;
      step.durationSeconds = std::max(0.0, durationEval.spec.periodSeconds);
    }
    step.probability = std::max(0, std::min(100, block.probability));
    if (block.rest && block.range.end > block.range.begin) {
      step.highlightBegin = lineBegin + block.range.begin;
      step.highlightEnd = lineBegin + block.range.end;
    } else {
      step.highlightBegin = lineBegin + block.literal.range.begin;
      step.highlightEnd = lineBegin + block.literal.range.end;
    }
    step.repeatHighlightBegin = lineBegin + block.repeatValueRange.begin;
    step.repeatHighlightEnd = lineBegin + block.repeatValueRange.end;
    step.repeatHighlightIsOwn = block.repeatValueIsOwn;
    if (block.repeatIsDuration && block.repeatIsRandom) {
      step.hasDuration = true;
      step.durationSeconds = 0.f;
    }
    if (block.hasTotalDuration) {
      step.totalDurationIsRandom = block.totalDurationIsRandom;
      step.totalDurationRandom = block.totalDurationRandom;
      step.totalDurationIsTickCount = block.totalDurationIsTickCount;
      step.totalDurationTicks = block.totalDurationTicks;
      if (block.totalDurationUsesExternalClock) {
        step.totalDurationExternalClockInput = mapExternalClock(
            externalClockInputIndex, block.totalDurationExternalClock);
      }
      if (!block.totalDurationIsTickCount && !block.totalDurationIsRandom) {
        blunch::language::EvaluationResult totalDurationEval =
            blunch::language::evaluateClockLiteral(block.totalDuration);
        if (!totalDurationEval.ok()) {
          program.clear();
          return false;
        }
        step.totalDurationSeconds =
            std::max(0.0, totalDurationEval.spec.periodSeconds);
      }
      step.hasTotalDurationGroup = true;
      step.totalDurationGroupId = block.totalDurationGroupId;
      step.totalDurationHighlightBegin =
          lineBegin + block.totalDurationValueRange.begin;
      step.totalDurationHighlightEnd =
          lineBegin + block.totalDurationValueRange.end;
    }
    program.push_back(step);
  }

  for (size_t i = 0; i < program.size(); i++) {
    if (!program[i].hasTotalDurationGroup) {
      continue;
    }

    int groupId = program[i].totalDurationGroupId;
    int begin = (int)i;
    while (begin > 0 && program[begin - 1].hasTotalDurationGroup &&
           program[begin - 1].totalDurationGroupId == groupId) {
      begin--;
    }

    int end = (int)i + 1;
    while (end < (int)program.size() && program[end].hasTotalDurationGroup &&
           program[end].totalDurationGroupId == groupId) {
      end++;
    }

    program[i].totalDurationGroupStart = begin;
    program[i].totalDurationGroupEnd = end;
  }

  for (size_t i = 0; i < program.size(); i++) {
    captureBaseStepState(program[i]);
  }

  return !program.empty();
}

}  // namespace program
}  // namespace blunch
