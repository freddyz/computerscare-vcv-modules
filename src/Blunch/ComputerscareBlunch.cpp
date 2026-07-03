#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "../BlunchLanguage/BlunchLanguage.hpp"
#include "../ComputerscareResizableHandle.hpp"
#include "../ComputerscareTextEditor.hpp"

struct BlunchLineInfo {
  int begin = 0;
  int end = 0;
  std::string text;
};

static BlunchLineInfo getLineInfo(const std::string& text, int line) {
  BlunchLineInfo info;
  int currentLine = 0;
  int length = text.size();
  info.begin = 0;

  for (int i = 0; i < length; i++) {
    if (currentLine == line) {
      info.begin = i;
      break;
    }
    if (text[i] == '\n') {
      currentLine++;
      info.begin = i + 1;
    }
  }

  if (line > currentLine) {
    info.begin = length;
  }

  info.end = length;
  for (int i = info.begin; i < length; i++) {
    if (text[i] == '\n') {
      info.end = i;
      break;
    }
  }
  info.text = text.substr(info.begin, info.end - info.begin);
  return info;
}

static int getLineCount(const std::string& text) {
  int count = 1;
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == '\n') {
      count++;
    }
  }
  return count;
}

static bool isBlankLine(const std::string& text) {
  for (size_t i = 0; i < text.size(); i++) {
    if (std::isspace(static_cast<unsigned char>(text[i])) == 0) {
      return false;
    }
  }
  return true;
}

static bool choiceHasExplicitUnit(
    const blunch::language::RandomChoiceAst& choice) {
  return choice.unitRange.end > choice.unitRange.begin;
}

static bool choiceIsExternalClock(
    const blunch::language::RandomChoiceAst& choice) {
  return choice.externalClockChoice;
}

static const blunch::language::RandomChoiceAst& chooseRandomChoice(
    const blunch::language::ClockLiteralAst& ast) {
  size_t choiceIndex =
      std::min((size_t)(random::uniform() * ast.randomChoices.size()),
               ast.randomChoices.size() - 1);
  return ast.randomChoices[choiceIndex];
}

static double sampleRandomChoiceValue(
    const blunch::language::RandomChoiceAst& choice) {
  double lowValue = std::min(choice.minValue, choice.maxValue);
  double highValue = std::max(choice.minValue, choice.maxValue);
  if (highValue <= lowValue) {
    return lowValue;
  }
  return lowValue + random::uniform() * (highValue - lowValue);
}

static int sampleRandomChoiceInteger(
    const blunch::language::RandomChoiceAst& choice) {
  int lowValue = (int)std::min(choice.minValue, choice.maxValue);
  int highValue = (int)std::max(choice.minValue, choice.maxValue);
  if (highValue <= lowValue) {
    return lowValue;
  }
  return lowValue +
         (int)std::floor(random::uniform() * (float)(highValue - lowValue + 1));
}

static blunch::language::ClockLiteralAst literalForRandomChoice(
    const blunch::language::ClockLiteralAst& randomAst,
    const blunch::language::RandomChoiceAst& choice, double value) {
  blunch::language::ClockLiteralAst literal;
  if (choiceIsExternalClock(choice)) {
    literal.kind = blunch::language::ClockLiteralKind::ExternalClock;
    literal.externalClock = choice.externalClock;
    literal.range = choice.range;
    return literal;
  }

  literal.kind = blunch::language::ClockLiteralKind::Numeric;
  literal.value = value;
  literal.valueLexeme = choice.minValueLexeme;
  literal.range = choice.range;
  literal.unit = choiceHasExplicitUnit(choice) ? choice.unit : randomAst.unit;
  literal.unitRange =
      choiceHasExplicitUnit(choice) ? choice.unitRange : randomAst.unitRange;
  return literal;
}

struct BlunchSpeedOfTimeParamQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    float exponent = getValue();
    float factor = std::pow(2.f, exponent);
    if (std::fabs(factor - 1.f) < 0.0001f) {
      return "*1";
    }
    if (factor > 1.f) {
      return string::f("*%.3g", factor);
    }
    return string::f("/%.3g", 1.f / factor);
  }
};

struct BlunchProgramStep {
  blunch::language::ClockLiteralAst literal;
  blunch::language::ClockSpec spec;
  bool isRest = false;
  int externalClockInput = -1;
  int repeat = 1;
  bool repeatIsRandom = false;
  bool repeatRandomIsDuration = false;
  blunch::language::ClockLiteralAst repeatRandom;
  int repeatExternalClockInput = -1;
  bool hasDuration = false;
  float durationSeconds = 0.f;
  int probability = 100;
  int highlightBegin = 0;
  int highlightEnd = 0;
  int repeatHighlightBegin = 0;
  int repeatHighlightEnd = 0;
  bool repeatHighlightIsOwn = false;
  bool hasTotalDurationGroup = false;
  int totalDurationGroupId = -1;
  int totalDurationGroupStart = 0;
  int totalDurationGroupEnd = 0;
  bool totalDurationIsRandom = false;
  blunch::language::ClockLiteralAst totalDurationRandom;
  bool totalDurationIsTickCount = false;
  int totalDurationTicks = 0;
  int totalDurationExternalClockInput = -1;
  float totalDurationSeconds = 0.f;
  int totalDurationHighlightBegin = 0;
  int totalDurationHighlightEnd = 0;
  bool hasRandomValue = false;
  int sourceLineBegin = 0;
};

struct ComputerscareBlunch : Module {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;
  static constexpr float MIN_WIDTH = 150.f;

  enum ParamIds {
    SPEED_OF_TIME_PARAM,
    AUTO_ADVANCE_PARAM,
    EDITOR_FONT_SIZE_PARAM,
    EDITOR_FONT_WIDTH_PARAM,
    EDITOR_FONT_HEIGHT_PARAM,
    EDITOR_LETTER_SPACING_PARAM,
    AUTO_BLOCK_ADVANCE_PARAM,
    RUN_PARAM,
    NUM_PARAMS
  };
  enum InputIds {
    EXTERNAL_CLOCK_W_INPUT,
    EXTERNAL_CLOCK_X_INPUT,
    EXTERNAL_CLOCK_Y_INPUT,
    EXTERNAL_CLOCK_Z_INPUT,
    ADVANCE_INPUT,
    ADVANCE_TOKEN_INPUT,
    ADVANCE_LINE_INPUT,
    RESET_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    CLOCK_OUTPUT,
    EOC1_OUTPUT,
    EOC2_OUTPUT,
    EOC3_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds { SYNTAX_ERROR_LIGHT, NUM_LIGHTS };

  ComputerscareTextEditorState editorState;
  dsp::PulseGenerator tokenMovePulse;
  dsp::PulseGenerator lineCyclePulse;
  dsp::PulseGenerator wrapPulse;
  dsp::SchmittTrigger externalClockTriggers[4];
  dsp::SchmittTrigger advanceTrigger;
  dsp::SchmittTrigger advanceTokenTrigger;
  dsp::SchmittTrigger advanceLineTrigger;
  dsp::SchmittTrigger resetTrigger;
  float clockPhase = 0.f;
  float activeClockRamp = 0.f;
  bool clockHigh = false;
  int clockStartLowSamples = 0;
  bool clockStartHighPending = false;
  bool syntaxError = false;
  int selectedLine = 0;
  bool selectedLineDirty = false;
  int activeLine = 0;
  std::string activeLineText;
  int pendingLine = -1;
  std::string pendingLineText;
  std::vector<BlunchProgramStep> pendingProgram;
  bool viewingPendingLine = true;
  int errorLine = -1;
  int errorLineRevertLine = -1;
  std::string errorLineRevertText;
  std::string checkedFocusedLineText;
  int lastSubmitCount = 0;
  int lastCancelCount = 0;
  int lastSwitchViewCount = 0;
  int activeHighlightBegin = 0;
  int activeHighlightEnd = 6;
  blunch::language::ClockSpec activeClockSpec;
  std::vector<BlunchProgramStep> activeProgram;
  int activeProgramIndex = 0;
  int activeProgramBeat = 0;
  float activeProgramElapsedSeconds = 0.f;
  int activeTotalDurationGroupId = -1;
  float activeTotalDurationElapsedSeconds = 0.f;
  int activeTotalDurationTicks = 0;
  bool activeStepPlays = true;
  bool activeClockOutputHigh = false;
  bool externalClockHigh[4] = {false, false, false, false};
  float width = MIN_WIDTH;
  bool editorLineWrapping = true;

  ComputerscareBlunch() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam<BlunchSpeedOfTimeParamQuantity>(SPEED_OF_TIME_PARAM, -5.f, 5.f,
                                                0.f, "Speed of time");
    configSwitch(AUTO_ADVANCE_PARAM, 0.f, 1.f, 1.f, "Line advance",
                 {"Manual", "Automatic"});
    configSwitch(AUTO_BLOCK_ADVANCE_PARAM, 0.f, 1.f, 1.f, "Block advance",
                 {"Manual", "Automatic"});
    configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
    configParam(EDITOR_FONT_SIZE_PARAM, 8.f, 24.f, BND_LABEL_FONT_SIZE,
                "Editor font size");
    configParam(EDITOR_FONT_WIDTH_PARAM, -0.35f, 0.75f, 0.f,
                "Editor font width");
    configParam(EDITOR_FONT_HEIGHT_PARAM, -0.35f, 0.75f, 0.f,
                "Editor font height");
    configParam(EDITOR_LETTER_SPACING_PARAM, -2.f, 4.f, 0.f,
                "Editor letter spacing");
    getParamQuantity(SPEED_OF_TIME_PARAM)->randomizeEnabled = false;
    getParamQuantity(AUTO_ADVANCE_PARAM)->randomizeEnabled = false;
    getParamQuantity(AUTO_BLOCK_ADVANCE_PARAM)->randomizeEnabled = false;
    getParamQuantity(RUN_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_FONT_SIZE_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_FONT_WIDTH_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_FONT_HEIGHT_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_LETTER_SPACING_PARAM)->randomizeEnabled = false;
    configInput(ADVANCE_INPUT, "Advance");
    configInput(ADVANCE_TOKEN_INPUT, "Advance token");
    configInput(ADVANCE_LINE_INPUT, "Advance line");
    configInput(RESET_INPUT, "Reset");
    configInput(EXTERNAL_CLOCK_W_INPUT, "External clock W");
    configInput(EXTERNAL_CLOCK_X_INPUT, "External clock X");
    configInput(EXTERNAL_CLOCK_Y_INPUT, "External clock Y");
    configInput(EXTERNAL_CLOCK_Z_INPUT, "External clock Z");
    configOutput(CLOCK_OUTPUT, "Clock");
    configOutput(EOC1_OUTPUT, "Token move EOC");
    configOutput(EOC2_OUTPUT, "Line cycle EOC");
    configOutput(EOC3_OUTPUT, "All lines wrap EOC");
    editorState.text = "120bpm\n33hz\n40ms\n";
    activeClockSpec.bpm = CLOCK_BPM;
    activeClockSpec.hz = CLOCK_HZ;
    activeClockSpec.periodSeconds = 1.f / CLOCK_HZ;
    commitLine(0, true);
  }

  void process(const ProcessArgs& args) override {
    float timeScale = getTimeScale();
    float scaledSampleTime = args.sampleTime * timeScale;
    bool externalClockEdges[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
      float voltage = inputs[EXTERNAL_CLOCK_W_INPUT + i].getVoltage();
      externalClockEdges[i] = externalClockTriggers[i].process(voltage);
      externalClockHigh[i] = externalClockTriggers[i].isHigh();
    }

    int activeExternalClock = activeExternalClockInput();
    int activeRepeatClock = activeRepeatExternalClockInput();
    int activeTotalTickClock = activeTotalDurationExternalClockInput();
    bool running = params[RUN_PARAM].getValue() > 0.5f;
    if (running) {
      if (activeExternalClock >= 0) {
        if (!advanceActiveTotalDuration(scaledSampleTime)) {
          advanceActiveProgramDuration(scaledSampleTime);
        }
        activeExternalClock = activeExternalClockInput();
        activeRepeatClock = activeRepeatExternalClockInput();
        activeTotalTickClock = activeTotalDurationExternalClockInput();
        activeClockRamp =
            activeExternalClock >= 0
                ? (externalClockHigh[activeExternalClock] ? 1.f : 0.f)
                : clockPhase;
      } else {
        clockPhase += scaledSampleTime * activeClockSpec.hz;
        while (clockPhase >= 1.f) {
          clockPhase -= 1.f;
          if (activeRepeatClock < 0) {
            advanceActiveProgramBeat(activeTotalTickClock < 0);
            activeRepeatClock = activeRepeatExternalClockInput();
            activeTotalTickClock = activeTotalDurationExternalClockInput();
          }
        }
        if (!advanceActiveTotalDuration(scaledSampleTime)) {
          advanceActiveProgramDuration(scaledSampleTime);
        }
        activeExternalClock = activeExternalClockInput();
        activeRepeatClock = activeRepeatExternalClockInput();
        activeTotalTickClock = activeTotalDurationExternalClockInput();
        activeClockRamp = clockPhase;
      }
      clockHigh = nextClockGateHigh();
    } else {
      activeClockRamp =
          activeExternalClock >= 0
              ? (externalClockHigh[activeExternalClock] ? 1.f : 0.f)
              : clockPhase;
      clockHigh = false;
    }
    bool outputClockHigh = activeExternalClock >= 0
                               ? externalClockHigh[activeExternalClock]
                               : clockHigh;
    activeClockOutputHigh = running && outputClockHigh && activeStepPlays;
    outputs[CLOCK_OUTPUT].setVoltage(activeClockOutputHigh ? 10.f : 0.f);
    outputs[EOC1_OUTPUT].setVoltage(
        tokenMovePulse.process(args.sampleTime) ? 10.f : 0.f);
    outputs[EOC2_OUTPUT].setVoltage(
        lineCyclePulse.process(args.sampleTime) ? 10.f : 0.f);
    outputs[EOC3_OUTPUT].setVoltage(wrapPulse.process(args.sampleTime) ? 10.f
                                                                       : 0.f);
    lights[SYNTAX_ERROR_LIGHT].setBrightness(syntaxError ? 1.f : 0.f);

    bool externalTotalTickAdvanced = false;
    if (running && activeTotalTickClock >= 0 &&
        externalClockEdges[activeTotalTickClock]) {
      externalTotalTickAdvanced = advanceActiveTotalTickCount();
      activeRepeatClock = activeRepeatExternalClockInput();
    }

    if (running && !externalTotalTickAdvanced && activeRepeatClock >= 0 &&
        externalClockEdges[activeRepeatClock]) {
      advanceActiveProgramBeat(activeTotalDurationExternalClockInput() < 0);
    }

    if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
      resetActiveProgram(true);
    }
    if (advanceLineTrigger.process(inputs[ADVANCE_LINE_INPUT].getVoltage())) {
      moveToNextLine(false);
    }
    if (advanceTokenTrigger.process(inputs[ADVANCE_TOKEN_INPUT].getVoltage()) ||
        advanceTrigger.process(inputs[ADVANCE_INPUT].getVoltage())) {
      advanceActiveProgramStep(true);
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(
        rootJ, "text",
        json_stringn(editorState.text.c_str(), editorState.text.size()));
    json_object_set_new(rootJ, "focusedLine", json_integer(selectedLine));
    json_object_set_new(rootJ, "activeLine", json_integer(activeLine));
    json_object_set_new(rootJ, "width", json_real(width));
    json_object_set_new(rootJ, "editorLineWrapping",
                        json_boolean(editorLineWrapping));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* textJ = json_object_get(rootJ, "text");
    if (textJ) {
      editorState.text = json_string_value(textJ);
      editorState.dirty = true;
    }
    json_t* focusedLineJ = json_object_get(rootJ, "focusedLine");
    if (focusedLineJ) {
      selectedLine = json_integer_value(focusedLineJ);
      selectedLineDirty = true;
    }
    json_t* activeLineJ = json_object_get(rootJ, "activeLine");
    if (activeLineJ) {
      activeLine = json_integer_value(activeLineJ);
      commitLine(activeLine, true);
    }
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) {
      width = std::max(MIN_WIDTH, (float)json_number_value(widthJ));
    }
    json_t* editorLineWrappingJ = json_object_get(rootJ, "editorLineWrapping");
    if (editorLineWrappingJ) {
      editorLineWrapping = json_boolean_value(editorLineWrappingJ);
    }
  }

  bool parseLineProgram(int line, int lineBegin, const std::string& lineText,
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
      step.spec.bpm = CLOCK_BPM;
      step.spec.hz = CLOCK_HZ;
      step.spec.periodSeconds = 1.f / CLOCK_HZ;
      if (block.literal.kind ==
          blunch::language::ClockLiteralKind::ExternalClock) {
        step.externalClockInput =
            externalClockInputIndex(block.literal.externalClock);
      } else if (block.literal.kind ==
                 blunch::language::ClockLiteralKind::RandomRange) {
        bool hasNumericChoice = false;
        for (size_t choiceIndex = 0;
             choiceIndex < block.literal.randomChoices.size(); choiceIndex++) {
          if (!choiceIsExternalClock(
                  block.literal.randomChoices[choiceIndex])) {
            hasNumericChoice = true;
            break;
          }
        }
        if (hasNumericChoice) {
          for (size_t choiceIndex = 0;
               choiceIndex < block.literal.randomChoices.size();
               choiceIndex++) {
            const blunch::language::RandomChoiceAst& choice =
                block.literal.randomChoices[choiceIndex];
            if (choiceIsExternalClock(choice)) {
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
          if (choiceIsExternalClock(choice)) {
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
            externalClockInputIndex(block.repeatExternalClock);
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
          step.totalDurationExternalClockInput =
              externalClockInputIndex(block.totalDurationExternalClock);
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

    return !program.empty();
  }

  bool commitLine(int line, bool resetPhase) {
    BlunchLineInfo lineInfo = getLineInfo(editorState.text, line);
    std::vector<BlunchProgramStep> program;
    if (!parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      setSyntaxError(
          line, line == activeLine ? activeLineText : checkedFocusedLineText);
      return false;
    }

    clearSyntaxError();
    activeLine = line;
    activeLineText = lineInfo.text;
    activeProgram = program;
    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.f;
    activeTotalDurationGroupId = -1;
    activeTotalDurationElapsedSeconds = 0.f;
    activeTotalDurationTicks = 0;
    activeStepPlays = true;
    if (resetPhase) {
      clockPhase = 0.f;
      activeClockRamp = 0.f;
    }
    if (pendingLine == line && pendingLineText == lineInfo.text) {
      clearPendingLine();
    }
    applyActiveProgramStep();
    return true;
  }

  void clearPendingLine() {
    pendingLine = -1;
    pendingLineText.clear();
    pendingProgram.clear();
    viewingPendingLine = true;
  }

  void replaceLineText(int line, const std::string& replacement) {
    BlunchLineInfo lineInfo = getLineInfo(editorState.text, line);
    editorState.text.replace(lineInfo.begin, lineInfo.end - lineInfo.begin,
                             replacement);
    editorState.dirty = true;
  }

  void cancelPendingLine(int line) {
    if (errorLine == line) {
      std::string revertText;
      if (line == activeLine) {
        revertText = activeLineText;
      } else if (pendingLine == line && !pendingLineText.empty()) {
        revertText = pendingLineText;
      } else if (errorLineRevertLine == line) {
        revertText = errorLineRevertText;
      }
      replaceLineText(line, revertText);
      if (pendingLine == line && line == activeLine) {
        clearPendingLine();
      }
      checkedFocusedLineText = getLineInfo(editorState.text, line).text;
      clearSyntaxError();
      return;
    }

    if (pendingLine == line) {
      if (line == activeLine) {
        replaceLineText(line, activeLineText);
      }
      clearPendingLine();
    }
    checkedFocusedLineText = getLineInfo(editorState.text, line).text;
  }

  void showPendingLine() {
    if (pendingLine < 0 || pendingLineText.empty()) {
      return;
    }

    replaceLineText(pendingLine, pendingLineText);
    checkedFocusedLineText = pendingLineText;
    viewingPendingLine = true;
  }

  void showActiveLineVersion() {
    if (pendingLine != activeLine) {
      return;
    }

    replaceLineText(activeLine, activeLineText);
    checkedFocusedLineText = activeLineText;
    viewingPendingLine = false;
  }

  void togglePendingView() {
    if (errorLine == activeLine) {
      replaceLineText(activeLine, activeLineText);
      checkedFocusedLineText = activeLineText;
      clearSyntaxError();
      viewingPendingLine = false;
      return;
    }

    if (pendingLine != activeLine) {
      return;
    }

    if (viewingPendingLine) {
      showActiveLineVersion();
    } else {
      showPendingLine();
    }
  }

  void inspectFocusedLine(int line, bool focusChanged) {
    BlunchLineInfo lineInfo = getLineInfo(editorState.text, line);
    std::string previousCheckedLineText = checkedFocusedLineText;
    if (line != activeLine) {
      if (focusChanged || lineInfo.text == checkedFocusedLineText) {
        checkedFocusedLineText = lineInfo.text;
        return;
      }

      checkedFocusedLineText = lineInfo.text;
      std::vector<BlunchProgramStep> program;
      if (parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
        pendingLine = line;
        pendingLineText = lineInfo.text;
        pendingProgram = program;
        if (errorLine == line) {
          clearSyntaxError();
        }
        syntaxError = false;
      } else {
        std::string revertText = pendingLine == line && !pendingLineText.empty()
                                     ? pendingLineText
                                     : previousCheckedLineText;
        setSyntaxError(line, revertText);
      }
      return;
    }

    if (lineInfo.text == activeLineText) {
      if (pendingLine == line && viewingPendingLine) {
        clearPendingLine();
      }
      if (errorLine == line) {
        clearSyntaxError();
      }
      checkedFocusedLineText = lineInfo.text;
      return;
    }

    if (pendingLine == line && lineInfo.text == pendingLineText) {
      checkedFocusedLineText = lineInfo.text;
      viewingPendingLine = true;
      return;
    }

    if (lineInfo.text == checkedFocusedLineText &&
        (pendingLine == line || errorLine == line)) {
      return;
    }

    checkedFocusedLineText = lineInfo.text;
    std::vector<BlunchProgramStep> program;
    if (parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      pendingLine = line;
      pendingLineText = lineInfo.text;
      pendingProgram = program;
      viewingPendingLine = true;
      if (errorLine == line) {
        clearSyntaxError();
      }
      syntaxError = false;
    } else {
      setSyntaxError(line, activeLineText);
    }
  }

  void setSyntaxError(int line, const std::string& revertText) {
    if (errorLine != line || errorLineRevertLine != line) {
      errorLineRevertLine = line;
      errorLineRevertText = revertText;
    }
    errorLine = line;
    syntaxError = true;
  }

  void clearSyntaxError() {
    errorLine = -1;
    errorLineRevertLine = -1;
    errorLineRevertText.clear();
    syntaxError = false;
  }

  void commitPendingLine(bool resetPhase) {
    if (pendingLine < 0 || pendingProgram.empty()) {
      return;
    }

    activeLine = pendingLine;
    activeLineText = pendingLineText;
    activeProgram = pendingProgram;
    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.f;
    activeTotalDurationGroupId = -1;
    activeTotalDurationElapsedSeconds = 0.f;
    activeTotalDurationTicks = 0;
    activeStepPlays = true;
    if (resetPhase) {
      clockPhase = 0.f;
      activeClockRamp = 0.f;
    }
    clearPendingLine();
    clearSyntaxError();
    applyActiveProgramStep();
  }

  void resetActiveProgram(bool resetPhase) {
    if (activeProgram.empty()) {
      commitLine(activeLine, resetPhase);
      return;
    }

    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.f;
    activeTotalDurationGroupId = -1;
    activeTotalDurationElapsedSeconds = 0.f;
    activeTotalDurationTicks = 0;
    activeStepPlays = true;
    if (resetPhase) {
      clockPhase = 0.f;
      activeClockRamp = 0.f;
    }
    applyActiveProgramStep();
  }

  void sampleStepRepeatArgument(BlunchProgramStep& step) {
    if (!step.repeatIsRandom || step.repeatRandom.randomChoices.empty()) {
      return;
    }

    const blunch::language::RandomChoiceAst& choice =
        chooseRandomChoice(step.repeatRandom);
    step.repeatHighlightBegin = step.sourceLineBegin + choice.range.begin;
    step.repeatHighlightEnd = step.sourceLineBegin + choice.range.end;

    if (!step.repeatRandomIsDuration) {
      step.hasDuration = false;
      step.repeat = std::max(1, sampleRandomChoiceInteger(choice));
      return;
    }

    double sampledValue = sampleRandomChoiceValue(choice);
    blunch::language::ClockLiteralAst literal =
        literalForRandomChoice(step.repeatRandom, choice, sampledValue);
    blunch::language::EvaluationResult eval =
        blunch::language::evaluateClockLiteral(literal);
    if (eval.ok()) {
      step.hasDuration = true;
      step.durationSeconds = std::max(0.0, eval.spec.periodSeconds);
    }
  }

  void sampleTotalDurationGroup(int stepIndex) {
    if (activeProgram.empty()) {
      return;
    }

    BlunchProgramStep& step = activeProgram[stepIndex];
    if (!step.totalDurationIsRandom ||
        step.totalDurationRandom.randomChoices.empty()) {
      return;
    }

    const blunch::language::RandomChoiceAst& choice =
        chooseRandomChoice(step.totalDurationRandom);
    bool suffixHasUnit = step.totalDurationRandom.unitRange.end >
                         step.totalDurationRandom.unitRange.begin;
    bool selectedIsDuration = choiceHasExplicitUnit(choice) || suffixHasUnit;

    bool selectedIsTickCount = !selectedIsDuration;
    int selectedTicks = 1;
    float selectedSeconds = 0.f;
    if (selectedIsTickCount) {
      selectedTicks = std::max(1, sampleRandomChoiceInteger(choice));
    } else {
      double sampledValue = sampleRandomChoiceValue(choice);
      blunch::language::ClockLiteralAst literal = literalForRandomChoice(
          step.totalDurationRandom, choice, sampledValue);
      blunch::language::EvaluationResult eval =
          blunch::language::evaluateClockLiteral(literal);
      if (eval.ok()) {
        selectedSeconds = std::max(0.0, eval.spec.periodSeconds);
      }
    }

    int begin = step.totalDurationGroupStart;
    int end = step.totalDurationGroupEnd;
    for (int i = begin; i < end; i++) {
      activeProgram[i].totalDurationIsTickCount = selectedIsTickCount;
      activeProgram[i].totalDurationTicks = selectedTicks;
      activeProgram[i].totalDurationSeconds = selectedSeconds;
      activeProgram[i].totalDurationHighlightBegin =
          activeProgram[i].sourceLineBegin + choice.range.begin;
      activeProgram[i].totalDurationHighlightEnd =
          activeProgram[i].sourceLineBegin + choice.range.end;
    }
  }

  void applyActiveProgramStep() {
    if (activeProgram.empty()) {
      return;
    }

    activeProgramIndex = std::max(
        0, std::min(activeProgramIndex, (int)activeProgram.size() - 1));
    BlunchProgramStep& step = activeProgram[activeProgramIndex];
    sampleStepRepeatArgument(step);
    refreshActiveClockSpecForStep();
    activeHighlightBegin = step.highlightBegin;
    activeHighlightEnd = step.highlightEnd;
    activeProgramElapsedSeconds = 0.f;
    if (step.hasTotalDurationGroup) {
      if (activeTotalDurationGroupId != step.totalDurationGroupId) {
        activeTotalDurationGroupId = step.totalDurationGroupId;
        activeTotalDurationElapsedSeconds = 0.f;
        activeTotalDurationTicks = 0;
        sampleTotalDurationGroup(activeProgramIndex);
      }
    } else {
      activeTotalDurationGroupId = -1;
      activeTotalDurationElapsedSeconds = 0.f;
      activeTotalDurationTicks = 0;
    }
    scheduleTokenStartTick();
    chooseActiveStepPlayback();
  }

  int externalClockInputIndex(char clock) const {
    switch (clock) {
      case 'w':
        return 0;
      case 'x':
        return 1;
      case 'y':
        return 2;
      case 'z':
        return 3;
      default:
        return -1;
    }
  }

  int activeExternalClockInput() const {
    if (activeProgram.empty()) {
      return -1;
    }
    return activeProgram[activeProgramIndex].externalClockInput;
  }

  int activeRepeatExternalClockInput() const {
    if (activeProgram.empty()) {
      return -1;
    }
    const BlunchProgramStep& step = activeProgram[activeProgramIndex];
    if (step.repeatExternalClockInput >= 0) {
      return step.repeatExternalClockInput;
    }
    return step.externalClockInput;
  }

  int activeTotalDurationExternalClockInput() const {
    if (activeProgram.empty()) {
      return -1;
    }
    const BlunchProgramStep& step = activeProgram[activeProgramIndex];
    if (!step.hasTotalDurationGroup || !step.totalDurationIsTickCount) {
      return -1;
    }
    return step.totalDurationExternalClockInput;
  }

  bool activeStepUsesExternalClock() const {
    return activeExternalClockInput() >= 0;
  }

  float getTimeScale() {
    return std::pow(2.f, params[SPEED_OF_TIME_PARAM].getValue());
  }

  void advanceActiveProgramBeat(bool countTotalTick = true) {
    if (activeProgram.empty()) {
      return;
    }

    if (countTotalTick && advanceActiveTotalTickCount()) {
      return;
    }

    if (activeProgram[activeProgramIndex].hasDuration) {
      refreshActiveClockSpecForStep();
      chooseActiveStepPlayback();
      return;
    }

    activeProgramBeat++;
    if (activeProgramBeat >= activeProgram[activeProgramIndex].repeat) {
      advanceActiveProgramStep();
    } else {
      refreshActiveClockSpecForStep();
      chooseActiveStepPlayback();
    }
  }

  void advanceActiveProgramDuration(float sampleTime) {
    if (activeProgram.empty() ||
        !activeProgram[activeProgramIndex].hasDuration) {
      return;
    }

    activeProgramElapsedSeconds += sampleTime;
    if (activeProgramElapsedSeconds >=
        activeProgram[activeProgramIndex].durationSeconds) {
      advanceActiveProgramStep();
    }
  }

  bool advanceActiveTotalDuration(float sampleTime) {
    if (activeProgram.empty()) {
      return false;
    }

    const BlunchProgramStep& step = activeProgram[activeProgramIndex];
    if (!step.hasTotalDurationGroup || step.totalDurationIsTickCount) {
      return false;
    }

    activeTotalDurationElapsedSeconds += sampleTime;
    if (activeTotalDurationElapsedSeconds < step.totalDurationSeconds) {
      return false;
    }

    activeProgramBeat = 0;
    activeProgramIndex = step.totalDurationGroupEnd;
    activeTotalDurationGroupId = -1;
    activeTotalDurationElapsedSeconds = 0.f;
    activeTotalDurationTicks = 0;
    finishActiveTotalDurationGroup();
    return true;
  }

  bool advanceActiveTotalTickCount() {
    if (activeProgram.empty()) {
      return false;
    }

    const BlunchProgramStep& step = activeProgram[activeProgramIndex];
    if (!step.hasTotalDurationGroup || !step.totalDurationIsTickCount) {
      return false;
    }

    activeTotalDurationTicks++;
    if (activeTotalDurationTicks < step.totalDurationTicks) {
      return false;
    }

    activeProgramBeat = 0;
    activeProgramIndex = step.totalDurationGroupEnd;
    activeTotalDurationGroupId = -1;
    activeTotalDurationElapsedSeconds = 0.f;
    activeTotalDurationTicks = 0;
    finishActiveTotalDurationGroup();
    return true;
  }

  void finishActiveTotalDurationGroup() {
    if (activeProgramIndex >= (int)activeProgram.size()) {
      activeProgramIndex = 0;
      bool movedLine = handleLineCycleEnd();
      if (movedLine || activeProgram.size() > 1) {
        tokenMovePulse.trigger(1e-3f);
      }
    } else {
      applyActiveProgramStep();
      tokenMovePulse.trigger(1e-3f);
    }
  }

  bool autoBlockAdvanceEnabled() {
    return params[AUTO_BLOCK_ADVANCE_PARAM].getValue() > 0.5f;
  }

  void repeatActiveProgramStep() {
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.f;
    applyActiveProgramStep();
  }

  void advanceActiveProgramStep(bool forceAdvance = false) {
    if (activeProgram.empty()) {
      return;
    }

    if (!forceAdvance && !autoBlockAdvanceEnabled()) {
      repeatActiveProgramStep();
      return;
    }

    bool hadMultipleSteps = activeProgram.size() > 1;
    const BlunchProgramStep& currentStep = activeProgram[activeProgramIndex];
    activeProgramBeat = 0;
    activeProgramIndex++;
    if (currentStep.hasTotalDurationGroup &&
        activeProgramIndex >= currentStep.totalDurationGroupEnd) {
      activeProgramIndex = currentStep.totalDurationGroupStart;
      applyActiveProgramStep();
      if (hadMultipleSteps) {
        tokenMovePulse.trigger(1e-3f);
      }
      return;
    }

    if (activeProgramIndex >= (int)activeProgram.size()) {
      activeProgramIndex = 0;
      bool movedLine = handleLineCycleEnd();
      if (movedLine || hadMultipleSteps) {
        tokenMovePulse.trigger(1e-3f);
      }
    } else {
      applyActiveProgramStep();
      tokenMovePulse.trigger(1e-3f);
    }
  }

  bool handleLineCycleEnd() {
    lineCyclePulse.trigger(1e-3f);
    if (params[AUTO_ADVANCE_PARAM].getValue() > 0.5f) {
      if (!moveToNextLine(false, false)) {
        applyActiveProgramStep();
        return false;
      }
      return true;
    }

    applyActiveProgramStep();
    return false;
  }

  bool moveToNextLine(bool resetPhase, bool emitTokenMove = true) {
    int lineCount = getLineCount(editorState.text);
    if (lineCount <= 0) {
      return false;
    }

    bool wrapped = false;
    int previousLine = activeLine;
    int previousHighlightBegin = activeHighlightBegin;
    int previousHighlightEnd = activeHighlightEnd;
    for (int offset = 1; offset <= lineCount; offset++) {
      int nextLine = activeLine + offset;
      if (nextLine >= lineCount) {
        nextLine -= lineCount;
        wrapped = true;
      }

      BlunchLineInfo lineInfo = getLineInfo(editorState.text, nextLine);
      if (isBlankLine(lineInfo.text)) {
        continue;
      }

      if (!commitLine(nextLine, resetPhase)) {
        return false;
      }

      if (emitTokenMove && (activeLine != previousLine ||
                            activeHighlightBegin != previousHighlightBegin ||
                            activeHighlightEnd != previousHighlightEnd)) {
        tokenMovePulse.trigger(1e-3f);
      }
      if (wrapped) {
        wrapPulse.trigger(1e-3f);
      }
      return true;
    }

    return false;
  }

  void chooseActiveStepPlayback() {
    if (activeProgram.empty()) {
      activeStepPlays = true;
      return;
    }

    int probability = activeProgram[activeProgramIndex].probability;
    if (activeProgram[activeProgramIndex].isRest) {
      activeStepPlays = false;
    } else if (probability >= 100) {
      activeStepPlays = true;
    } else if (probability <= 0) {
      activeStepPlays = false;
    } else {
      activeStepPlays = random::uniform() * 100.f < probability;
    }
  }

  void refreshActiveClockSpecForStep() {
    if (activeProgram.empty()) {
      return;
    }

    BlunchProgramStep& step = activeProgram[activeProgramIndex];
    if (!step.hasRandomValue) {
      activeClockSpec = step.spec;
      return;
    }

    const blunch::language::RandomChoiceAst* selectedChoice = NULL;
    double minValue = step.literal.minValue;
    double maxValue = step.literal.maxValue;
    if (!step.literal.randomChoices.empty()) {
      size_t choiceIndex = std::min(
          (size_t)(random::uniform() * step.literal.randomChoices.size()),
          step.literal.randomChoices.size() - 1);
      const blunch::language::RandomChoiceAst& choice =
          step.literal.randomChoices[choiceIndex];
      selectedChoice = &choice;
      step.highlightBegin = step.sourceLineBegin + choice.range.begin;
      step.highlightEnd = step.sourceLineBegin + choice.range.end;
      if (choiceIsExternalClock(choice)) {
        step.externalClockInput = externalClockInputIndex(choice.externalClock);
        activeClockSpec = step.spec;
        return;
      }
      step.externalClockInput = -1;
      minValue = step.literal.randomChoices[choiceIndex].minValue;
      maxValue = step.literal.randomChoices[choiceIndex].maxValue;
    }
    double lowValue = std::min(minValue, maxValue);
    double highValue = std::max(minValue, maxValue);
    double sampledValue = lowValue;
    if (highValue > lowValue) {
      sampledValue = lowValue + random::uniform() * (highValue - lowValue);
    }

    blunch::language::ClockLiteralAst sampledLiteral =
        selectedChoice ? literalForRandomChoice(step.literal, *selectedChoice,
                                                sampledValue)
                       : step.literal;
    blunch::language::EvaluationResult eval =
        selectedChoice ? blunch::language::evaluateClockLiteral(sampledLiteral)
                       : blunch::language::evaluateClockLiteralWithValue(
                             sampledLiteral, sampledValue);
    if (eval.ok()) {
      activeClockSpec = eval.spec;
    } else {
      activeClockSpec = step.spec;
    }
  }

  void scheduleTokenStartTick() {
    clockPhase = 0.f;
    activeClockRamp = 0.f;
    clockStartLowSamples = 1;
    clockStartHighPending = true;
  }

  bool nextClockGateHigh() {
    if (activeExternalClockInput() >= 0) {
      clockStartLowSamples = 0;
      clockStartHighPending = false;
      return false;
    }

    if (clockStartLowSamples > 0) {
      clockStartLowSamples--;
      return false;
    }

    if (clockStartHighPending) {
      clockStartHighPending = false;
      clockPhase = 0.f;
      activeClockRamp = 0.f;
    }

    return clockPhase < 0.5f;
  }

  bool getActiveRepeatProgressHighlight(int& begin, int& end, float& progress) {
    int unusedSegments = 0;
    return getActiveRepeatProgressHighlight(begin, end, progress,
                                            unusedSegments);
  }

  bool getActiveRepeatProgressHighlight(int& begin, int& end, float& progress,
                                        int& segments) {
    if (activeProgram.empty()) {
      return false;
    }

    const BlunchProgramStep& step = activeProgram[activeProgramIndex];
    if (step.hasTotalDurationGroup) {
      if (step.totalDurationHighlightEnd <= step.totalDurationHighlightBegin) {
        return false;
      }

      begin = step.totalDurationHighlightBegin;
      end = step.totalDurationHighlightEnd;
      if (step.totalDurationIsTickCount) {
        segments = std::max(1, step.totalDurationTicks);
        progress = step.totalDurationTicks > 0
                       ? (float)(activeTotalDurationTicks + 1) /
                             step.totalDurationTicks
                       : 0.f;
      } else {
        segments = 0;
        progress =
            step.totalDurationSeconds > 0.f
                ? activeTotalDurationElapsedSeconds / step.totalDurationSeconds
                : 0.f;
      }
      progress = std::max(0.f, std::min(progress, 1.f));
      return true;
    }

    if (step.repeatHighlightEnd <= step.repeatHighlightBegin) {
      return false;
    }
    begin = step.repeatHighlightBegin;
    end = step.repeatHighlightEnd;
    if (step.hasDuration) {
      segments = 0;
      progress = step.durationSeconds > 0.f
                     ? activeProgramElapsedSeconds / step.durationSeconds
                     : 0.f;
    } else {
      segments = std::max(1, step.repeat);
      progress =
          step.repeat > 0 ? (float)(activeProgramBeat + 1) / step.repeat : 0.f;
    }
    progress = std::max(0.f, std::min(progress, 1.f));
    return true;
  }
};

struct BlunchTitle : TransparentWidget {
  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 18.f);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    nvgText(args.vg, box.size.x - 8.f, 0.f, "Blunch", NULL);
  }
};

struct BlunchExternalClockLabels : TransparentWidget {
  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 14.f);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

    const char* labels[] = {"w", "x", "y", "z"};
    const float jackCenters[] = {13.f, 48.f, 83.f, 118.f};
    for (int i = 0; i < 4; i++) {
      nvgText(args.vg, jackCenters[i], 2.f, labels[i], NULL);
    }
  }
};

struct BlunchSpeedOfTimeLabel : TransparentWidget {
  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 8.f);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgText(args.vg, box.size.x * 0.5f, 0.f, "time", NULL);
  }
};

struct BlunchAdvanceModeButton : ComputerscareBlankButton {
  static constexpr float DRAW_SCALE_X = 0.72f;
  std::string label = "Line";

  BlunchAdvanceModeButton() {
    momentary = false;
    box.size.x *= DRAW_SCALE_X;
  }

  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, DRAW_SCALE_X, 1.f);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);

    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    ParamQuantity* quantity = getParamQuantity();
    bool pressed = quantity && quantity->getValue() > 0.5f;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, label.size() > 4 ? 9.5f : 10.5f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgText(args.vg, box.size.x * 0.5f + (pressed ? 2.1f : -1.1f),
            box.size.y * 0.48f + (pressed ? 3.3f : 0.f), label.c_str(), NULL);
  }
};

struct BlunchPanel : Widget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(0xf4, 0xf2, 0xec));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, 42.f);
    nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xfa));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, 0.f, 42.f);
    nvgLineTo(args.vg, box.size.x, 42.f);
    nvgStrokeColor(args.vg, nvgRGB(0xd6, 0xd2, 0xc8));
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);
  }
};

struct ComputerscareBlunchWidget : ModuleWidget {
  BlunchPanel* panel = nullptr;
  BlunchTitle* title = nullptr;
  BlunchSpeedOfTimeLabel* speedOfTimeLabel = nullptr;
  BlunchExternalClockLabels* externalClockLabels = nullptr;
  ComputerscareTextEditor* editor = nullptr;
  Widget* syntaxErrorLight = nullptr;
  PortWidget* externalClockWInput = nullptr;
  PortWidget* externalClockXInput = nullptr;
  PortWidget* externalClockYInput = nullptr;
  PortWidget* externalClockZInput = nullptr;
  PortWidget* advanceInput = nullptr;
  PortWidget* advanceTokenInput = nullptr;
  PortWidget* advanceLineInput = nullptr;
  PortWidget* resetInput = nullptr;
  PortWidget* clockOutput = nullptr;
  PortWidget* eoc1Output = nullptr;
  PortWidget* eoc2Output = nullptr;
  PortWidget* eoc3Output = nullptr;
  ComputerscareResizeHandle* leftHandle = nullptr;
  ComputerscareResizeHandle* rightHandle = nullptr;
  ComputerscareTextEditorState browserEditorState;
  int lastCursorLine = -1;

  ComputerscareBlunchWidget(ComputerscareBlunch* module) {
    setModule(module);
    box.size =
        Vec(module ? module->width : ComputerscareBlunch::MIN_WIDTH, 380.f);

    panel = new BlunchPanel();
    panel->box.size = box.size;
    addChild(panel);

    leftHandle = new ComputerscareResizeHandle();
    leftHandle->minWidth = ComputerscareBlunch::MIN_WIDTH;
    addChild(leftHandle);

    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->minWidth = ComputerscareBlunch::MIN_WIDTH;
    addChild(rightHandle);

    title = new BlunchTitle();
    title->box.pos = Vec(0.f, 11.f);
    title->box.size = Vec(box.size.x, 24.f);
    addChild(title);

    speedOfTimeLabel = new BlunchSpeedOfTimeLabel();
    speedOfTimeLabel->box.pos = Vec(4.f, 4.f);
    speedOfTimeLabel->box.size = Vec(28.f, 9.f);
    addChild(speedOfTimeLabel);

    addParam(createParam<SmallKnob>(Vec(8.f, 14.f), module,
                                    ComputerscareBlunch::SPEED_OF_TIME_PARAM));

    externalClockLabels = new BlunchExternalClockLabels();
    externalClockLabels->box.pos = Vec(0.f, 282.f);
    externalClockLabels->box.size = Vec(box.size.x, 12.f);
    addChild(externalClockLabels);

    editor = createWidget<ComputerscareTextEditor>(Vec(3.f, 43.f));
    editor->box.size = Vec(box.size.x - 6.f, 245.f);
    editor->placeholder = "type a sequence...";
    editor->style.backgroundColor = nvgRGB(0x05, 0x06, 0x08);
    editor->style.borderColor = nvgRGB(0x55, 0x5b, 0x64);
    editor->style.textColor = nvgRGB(0xee, 0xee, 0xea);
    editor->style.selectionColor = nvgRGBA(0xe4, 0xc4, 0x21, 0x66);
    editor->style.placeholderColor = nvgRGBA(0xee, 0xee, 0xea, 0x66);
    editor->submitOnEnter = true;
    if (module) {
      editor->setState(&module->editorState);
    } else {
      browserEditorState.text = "120bpm\n33hz\n40ms\n";
      editor->setState(&browserEditorState);
    }
    addChild(editor);

    syntaxErrorLight =
        createLight<ComputerscareSmallLight<ComputerscareRedLight>>(
            Vec(127.f, 10.f), module, ComputerscareBlunch::SYNTAX_ERROR_LIGHT);
    addChild(syntaxErrorLight);

    BlunchAdvanceModeButton* runButton = createParam<BlunchAdvanceModeButton>(
        Vec(36.f, 8.f), module, ComputerscareBlunch::RUN_PARAM);
    runButton->label = "Run";
    addParam(runButton);

    BlunchAdvanceModeButton* lineAdvanceButton =
        createParam<BlunchAdvanceModeButton>(
            Vec(66.f, 8.f), module, ComputerscareBlunch::AUTO_ADVANCE_PARAM);
    lineAdvanceButton->label = "Line";
    addParam(lineAdvanceButton);

    BlunchAdvanceModeButton* blockAdvanceButton =
        createParam<BlunchAdvanceModeButton>(
            Vec(96.f, 8.f), module,
            ComputerscareBlunch::AUTO_BLOCK_ADVANCE_PARAM);
    blockAdvanceButton->label = "Block";
    addParam(blockAdvanceButton);

    externalClockWInput = createInput<PointingUpPentagonPort>(
        Vec(7.f, 295.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_W_INPUT);
    externalClockXInput = createInput<PointingUpPentagonPort>(
        Vec(42.f, 295.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_X_INPUT);
    externalClockYInput = createInput<PointingUpPentagonPort>(
        Vec(77.f, 295.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_Y_INPUT);
    externalClockZInput = createInput<PointingUpPentagonPort>(
        Vec(112.f, 295.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_Z_INPUT);
    addInput(externalClockWInput);
    addInput(externalClockXInput);
    addInput(externalClockYInput);
    addInput(externalClockZInput);

    advanceInput = createInput<PointingUpPentagonPort>(
        Vec(7.f, 322.f), module, ComputerscareBlunch::ADVANCE_INPUT);
    advanceTokenInput = createInput<PointingUpPentagonPort>(
        Vec(42.f, 322.f), module, ComputerscareBlunch::ADVANCE_TOKEN_INPUT);
    advanceLineInput = createInput<PointingUpPentagonPort>(
        Vec(77.f, 322.f), module, ComputerscareBlunch::ADVANCE_LINE_INPUT);
    resetInput = createInput<PointingUpPentagonPort>(
        Vec(112.f, 322.f), module, ComputerscareBlunch::RESET_INPUT);
    addInput(advanceInput);
    addInput(advanceTokenInput);
    addInput(advanceLineInput);
    addInput(resetInput);

    clockOutput = createOutput<InPort>(Vec(7.f, 350.f), module,
                                       ComputerscareBlunch::CLOCK_OUTPUT);
    eoc1Output = createOutput<InPort>(Vec(42.f, 350.f), module,
                                      ComputerscareBlunch::EOC1_OUTPUT);
    eoc2Output = createOutput<InPort>(Vec(77.f, 350.f), module,
                                      ComputerscareBlunch::EOC2_OUTPUT);
    eoc3Output = createOutput<InPort>(Vec(112.f, 350.f), module,
                                      ComputerscareBlunch::EOC3_OUTPUT);
    addOutput(clockOutput);
    addOutput(eoc1Output);
    addOutput(eoc2Output);
    addOutput(eoc3Output);
    applyLayout();
  }

  void applyLayout() {
    box.size.x = std::max(box.size.x, ComputerscareBlunch::MIN_WIDTH);
    if (panel) {
      panel->box.size = box.size;
    }
    if (title) {
      title->box.size.x = box.size.x;
    }
    if (speedOfTimeLabel) {
      speedOfTimeLabel->box.pos = Vec(4.f, 4.f);
    }
    if (externalClockLabels) {
      externalClockLabels->box.size.x = box.size.x;
    }
    if (editor) {
      editor->box.size.x = box.size.x - 6.f;
    }
    if (syntaxErrorLight) {
      syntaxErrorLight->box.pos.x = box.size.x - 23.f;
    }
    if (rightHandle) {
      rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    }

    const float externalInputY = 295.f;
    const float inputY = 322.f;
    const float outputY = 350.f;
    PortWidget* externalInputs[] = {externalClockWInput, externalClockXInput,
                                    externalClockYInput, externalClockZInput};
    PortWidget* inputs[] = {advanceInput, advanceTokenInput, advanceLineInput,
                            resetInput};
    PortWidget* outputs[] = {clockOutput, eoc1Output, eoc2Output, eoc3Output};
    const float jackXs[] = {7.f, 42.f, 77.f, 112.f};
    for (int i = 0; i < 4; i++) {
      if (inputs[i]) {
        inputs[i]->box.pos = Vec(jackXs[i], inputY);
      }
      if (externalInputs[i]) {
        externalInputs[i]->box.pos = Vec(jackXs[i], externalInputY);
      }
      if (outputs[i]) {
        outputs[i]->box.pos = Vec(jackXs[i], outputY);
      }
    }
  }

  void step() override {
    ModuleWidget::step();
    box.size.x = std::max(box.size.x, ComputerscareBlunch::MIN_WIDTH);

    ComputerscareBlunch* blunch = dynamic_cast<ComputerscareBlunch*>(module);
    if (blunch) {
      if (std::fabs(blunch->width - box.size.x) > 0.01f) {
        blunch->width = box.size.x;
      }
    }
    applyLayout();

    if (editor) {
      ComputerscareTextEditorState* state = nullptr;
      bool blinkHigh = false;
      float activeProgress = 0.f;

      if (blunch) {
        editor->style.fontSize =
            blunch->params[ComputerscareBlunch::EDITOR_FONT_SIZE_PARAM]
                .getValue();
        editor->style.fontWidthOffset =
            blunch->params[ComputerscareBlunch::EDITOR_FONT_WIDTH_PARAM]
                .getValue();
        editor->style.fontHeightOffset =
            blunch->params[ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM]
                .getValue();
        editor->style.letterSpacing =
            blunch->params[ComputerscareBlunch::EDITOR_LETTER_SPACING_PARAM]
                .getValue();
        editor->style.lineWrapping = blunch->editorLineWrapping;
        state = &blunch->editorState;
        if (blunch->selectedLineDirty) {
          editor->setCursorLine(blunch->selectedLine);
          blunch->selectedLineDirty = false;
        }
        blunch->selectedLine = editor->getCursorLine();
        bool focusChanged = blunch->selectedLine != lastCursorLine;
        lastCursorLine = blunch->selectedLine;
        blunch->inspectFocusedLine(blunch->selectedLine, focusChanged);
        if (state->submitCount != blunch->lastSubmitCount) {
          blunch->lastSubmitCount = state->submitCount;
          if (blunch->pendingLine == blunch->selectedLine) {
            blunch->commitPendingLine(true);
          } else {
            blunch->commitLine(blunch->selectedLine, true);
          }
        }
        if (state->cancelCount != blunch->lastCancelCount) {
          blunch->lastCancelCount = state->cancelCount;
          blunch->cancelPendingLine(blunch->selectedLine);
          editor->syncFromState();
        }
        if (state->switchViewCount != blunch->lastSwitchViewCount) {
          blunch->lastSwitchViewCount = state->switchViewCount;
          blunch->togglePendingView();
        }
        blinkHigh = blunch->activeClockOutputHigh;
        activeProgress = blunch->activeClockRamp;
      } else {
        state = &browserEditorState;
        float browserPhase = std::fmod(
            (float)rack::system::getTime() * ComputerscareBlunch::CLOCK_HZ,
            1.f);
        blinkHigh = browserPhase < 0.5f;
        activeProgress = browserPhase;
      }

      state->highlights.clear();
      int lineCount = getLineCount(state->text);
      for (int zebraLine = 1; zebraLine < lineCount; zebraLine += 2) {
        BlunchLineInfo zebraLineInfo = getLineInfo(state->text, zebraLine);
        ComputerscareTextHighlight zebraHighlight;
        zebraHighlight.begin = zebraLineInfo.begin;
        zebraHighlight.end = zebraLineInfo.end;
        zebraHighlight.fullLine = true;
        zebraHighlight.background = nvgRGBA(0xff, 0xff, 0xff, 0x16);
        state->highlights.push_back(zebraHighlight);
      }
      int line = editor->getCursorLine();
      BlunchLineInfo lineInfo = getLineInfo(state->text, line);
      ComputerscareTextHighlight focusHighlight;
      focusHighlight.begin = lineInfo.begin;
      focusHighlight.end = lineInfo.end;
      focusHighlight.fullLine = true;
      focusHighlight.background = nvgRGBA(0xb8, 0xb8, 0xb8, 0x42);
      state->highlights.push_back(focusHighlight);
      bool showingPendingActiveLine =
          blunch && blunch->pendingLine == blunch->activeLine &&
          blunch->viewingPendingLine;
      bool showingActiveVersionOfPending =
          blunch && blunch->pendingLine == blunch->activeLine &&
          !blunch->viewingPendingLine;
      bool showingInvalidActiveLine =
          blunch && blunch->errorLine == blunch->activeLine;
      if (blunch && blunch->pendingLine >= 0 &&
          !showingActiveVersionOfPending) {
        BlunchLineInfo pendingLineInfo =
            getLineInfo(state->text, blunch->pendingLine);
        float pendingPulse =
            0.5f + 0.5f * std::sin((float)rack::system::getTime() * 3.5f);
        ComputerscareTextHighlight pendingHighlight;
        pendingHighlight.begin = pendingLineInfo.begin;
        pendingHighlight.end = pendingLineInfo.end;
        pendingHighlight.fullLine = true;
        pendingHighlight.background = nvgRGBA(
            0x24, 0xc9, 0xa6, (unsigned char)(0x20 + pendingPulse * 0x22));
        state->highlights.push_back(pendingHighlight);
      }
      if (blunch && blunch->errorLine >= 0) {
        BlunchLineInfo errorLineInfo =
            getLineInfo(state->text, blunch->errorLine);
        float errorPulse =
            0.5f + 0.5f * std::sin((float)rack::system::getTime() * 4.25f);
        ComputerscareTextHighlight errorHighlight;
        errorHighlight.begin = errorLineInfo.begin;
        errorHighlight.end = errorLineInfo.end;
        errorHighlight.fullLine = true;
        errorHighlight.background = nvgRGBA(
            0xc4, 0x34, 0x21, (unsigned char)(0x2c + errorPulse * 0x24));
        state->highlights.push_back(errorHighlight);
      }
      ComputerscareTextHighlight activeHighlight;
      if (blunch) {
        activeHighlight.begin = blunch->activeHighlightBegin;
        activeHighlight.end = blunch->activeHighlightEnd;
      } else {
        activeHighlight.begin = lineInfo.begin;
        activeHighlight.end = lineInfo.end;
      }
      activeHighlight.hasBackground = blinkHigh;
      activeHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, 0x66);
      activeHighlight.hasBorder = true;
      activeHighlight.border = nvgRGBA(0xff, 0xee, 0x9a, 0xdd);
      activeHighlight.hasProgress =
          !(blunch && blunch->activeStepUsesExternalClock());
      activeHighlight.progress = activeProgress;
      activeHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
      if (!showingPendingActiveLine && !showingInvalidActiveLine &&
          activeHighlight.begin < activeHighlight.end) {
        state->highlights.push_back(activeHighlight);
      }
      if (blunch && !showingPendingActiveLine && !showingInvalidActiveLine) {
        int repeatBegin = 0;
        int repeatEnd = 0;
        int repeatSegments = 0;
        float repeatProgress = 0.f;
        if (blunch->getActiveRepeatProgressHighlight(
                repeatBegin, repeatEnd, repeatProgress, repeatSegments)) {
          ComputerscareTextHighlight repeatProgressHighlight;
          repeatProgressHighlight.begin = repeatBegin;
          repeatProgressHighlight.end = repeatEnd;
          repeatProgressHighlight.hasBackground = false;
          repeatProgressHighlight.hasProgress = true;
          repeatProgressHighlight.progress = repeatProgress;
          repeatProgressHighlight.progressSegments = repeatSegments;
          repeatProgressHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
          state->highlights.push_back(repeatProgressHighlight);
        }
      }
    }
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareBlunch* blunch = dynamic_cast<ComputerscareBlunch*>(module);
    if (!blunch) {
      return;
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createSubmenuItem("Editor", "", [=](Menu* submenu) {
      submenu->addChild(new MenuParamSlider(
          blunch
              ->paramQuantities[ComputerscareBlunch::EDITOR_FONT_SIZE_PARAM]));
      submenu->addChild(new MenuParamSlider(
          blunch
              ->paramQuantities[ComputerscareBlunch::EDITOR_FONT_WIDTH_PARAM]));
      submenu->addChild(new MenuParamSlider(
          blunch->paramQuantities
              [ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM]));
      submenu->addChild(new MenuParamSlider(
          blunch->paramQuantities
              [ComputerscareBlunch::EDITOR_LETTER_SPACING_PARAM]));
      submenu->addChild(createBoolPtrMenuItem("Line wrapping", "",
                                              &blunch->editorLineWrapping));
    }));
  }
};

Model* modelComputerscareBlunch =
    createModel<ComputerscareBlunch, ComputerscareBlunchWidget>(
        "computerscare-blunch");
