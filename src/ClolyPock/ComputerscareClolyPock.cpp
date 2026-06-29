#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "../ClolyPockLanguage/ClolyPockLanguage.hpp"
#include "../ComputerscareResizableHandle.hpp"
#include "../ComputerscareTextEditor.hpp"

struct ClolyPockLineInfo {
  int begin = 0;
  int end = 0;
  std::string text;
};

static ClolyPockLineInfo getLineInfo(const std::string& text, int line) {
  ClolyPockLineInfo info;
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

struct ClolyPockProgramStep {
  cloly::language::ClockLiteralAst literal;
  cloly::language::ClockSpec spec;
  int repeat = 1;
  bool hasDuration = false;
  float durationSeconds = 0.f;
  int probability = 100;
  int highlightBegin = 0;
  int highlightEnd = 0;
  int repeatHighlightBegin = 0;
  int repeatHighlightEnd = 0;
  bool repeatHighlightIsOwn = false;
  bool hasRandomValue = false;
};

struct ComputerscareClolyPock : Module {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;
  static constexpr float MIN_WIDTH = 150.f;

  enum ParamIds {
    AUTO_ADVANCE_PARAM,
    EDITOR_FONT_SIZE_PARAM,
    EDITOR_FONT_WIDTH_PARAM,
    EDITOR_FONT_HEIGHT_PARAM,
    EDITOR_LETTER_SPACING_PARAM,
    NUM_PARAMS
  };
  enum InputIds { ADVANCE_INPUT, NUM_INPUTS };
  enum OutputIds {
    CLOCK_OUTPUT,
    EOC1_OUTPUT,
    EOC2_OUTPUT,
    EOC3_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds { SYNTAX_ERROR_LIGHT, NUM_LIGHTS };

  ComputerscareTextEditorState editorState;
  dsp::SchmittTrigger advanceInputTrigger;
  dsp::PulseGenerator tokenMovePulse;
  dsp::PulseGenerator lineCyclePulse;
  dsp::PulseGenerator wrapPulse;
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
  std::vector<ClolyPockProgramStep> pendingProgram;
  bool viewingPendingLine = true;
  int errorLine = -1;
  std::string checkedFocusedLineText;
  int lastSubmitCount = 0;
  int lastCancelCount = 0;
  int lastSwitchViewCount = 0;
  int activeHighlightBegin = 0;
  int activeHighlightEnd = 6;
  cloly::language::ClockSpec activeClockSpec;
  std::vector<ClolyPockProgramStep> activeProgram;
  int activeProgramIndex = 0;
  int activeProgramBeat = 0;
  float activeProgramElapsedSeconds = 0.f;
  bool activeStepPlays = true;
  float width = MIN_WIDTH;
  bool editorLineWrapping = true;

  ComputerscareClolyPock() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configSwitch(AUTO_ADVANCE_PARAM, 0.f, 1.f, 1.f, "Line advance",
                 {"Manual", "Automatic"});
    configParam(EDITOR_FONT_SIZE_PARAM, 8.f, 24.f, BND_LABEL_FONT_SIZE,
                "Editor font size");
    configParam(EDITOR_FONT_WIDTH_PARAM, -0.35f, 0.75f, 0.f,
                "Editor font width");
    configParam(EDITOR_FONT_HEIGHT_PARAM, -0.35f, 0.75f, 0.f,
                "Editor font height");
    configParam(EDITOR_LETTER_SPACING_PARAM, -2.f, 4.f, 0.f,
                "Editor letter spacing");
    configInput(ADVANCE_INPUT, "Advance line");
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
    if (params[AUTO_ADVANCE_PARAM].getValue() <= 0.5f &&
        advanceInputTrigger.process(inputs[ADVANCE_INPUT].getVoltage())) {
      moveToNextLine(true);
    }

    clockPhase += args.sampleTime * activeClockSpec.hz;
    while (clockPhase >= 1.f) {
      clockPhase -= 1.f;
      advanceActiveProgramBeat();
    }
    advanceActiveProgramDuration(args.sampleTime);
    activeClockRamp = clockPhase;
    clockHigh = nextClockGateHigh();
    outputs[CLOCK_OUTPUT].setVoltage(clockHigh && activeStepPlays ? 10.f : 0.f);
    outputs[EOC1_OUTPUT].setVoltage(
        tokenMovePulse.process(args.sampleTime) ? 10.f : 0.f);
    outputs[EOC2_OUTPUT].setVoltage(
        lineCyclePulse.process(args.sampleTime) ? 10.f : 0.f);
    outputs[EOC3_OUTPUT].setVoltage(wrapPulse.process(args.sampleTime) ? 10.f
                                                                       : 0.f);
    lights[SYNTAX_ERROR_LIGHT].setBrightness(syntaxError ? 1.f : 0.f);
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
                        std::vector<ClolyPockProgramStep>& program) {
    cloly::language::ParseResult parse =
        cloly::language::parseClockLiteral(lineText);
    if (!parse.ok()) {
      return false;
    }

    program.clear();
    for (size_t i = 0; i < parse.program.blocks.size(); i++) {
      const cloly::language::ClockBlockAst& block = parse.program.blocks[i];
      cloly::language::EvaluationResult eval =
          cloly::language::evaluateClockLiteral(block.literal);
      if (!eval.ok()) {
        program.clear();
        return false;
      }

      ClolyPockProgramStep step;
      step.literal = block.literal;
      step.spec = eval.spec;
      step.hasRandomValue =
          block.literal.kind == cloly::language::ClockLiteralKind::RandomRange;
      if (step.hasRandomValue) {
        for (size_t choiceIndex = 0;
             choiceIndex < block.literal.randomChoices.size(); choiceIndex++) {
          const cloly::language::RandomChoiceAst& choice =
              block.literal.randomChoices[choiceIndex];
          cloly::language::EvaluationResult minEval =
              cloly::language::evaluateClockLiteralWithValue(block.literal,
                                                             choice.minValue);
          cloly::language::EvaluationResult maxEval =
              cloly::language::evaluateClockLiteralWithValue(block.literal,
                                                             choice.maxValue);
          if (!minEval.ok() || !maxEval.ok()) {
            program.clear();
            return false;
          }
        }
      }
      step.repeat = std::max(1, block.repeat);
      if (block.repeatIsDuration) {
        cloly::language::EvaluationResult durationEval =
            cloly::language::evaluateClockLiteral(block.repeatDuration);
        if (!durationEval.ok()) {
          program.clear();
          return false;
        }
        step.hasDuration = true;
        step.durationSeconds = std::max(0.0, durationEval.spec.periodSeconds);
      }
      step.probability = std::max(0, std::min(100, block.probability));
      step.highlightBegin = lineBegin + block.literal.range.begin;
      step.highlightEnd = lineBegin + block.literal.range.end;
      step.repeatHighlightBegin = lineBegin + block.repeatValueRange.begin;
      step.repeatHighlightEnd = lineBegin + block.repeatValueRange.end;
      step.repeatHighlightIsOwn = block.repeatValueIsOwn;
      program.push_back(step);
    }

    return !program.empty();
  }

  bool commitLine(int line, bool resetPhase) {
    ClolyPockLineInfo lineInfo = getLineInfo(editorState.text, line);
    std::vector<ClolyPockProgramStep> program;
    if (!parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      syntaxError = true;
      errorLine = line;
      return false;
    }

    syntaxError = false;
    errorLine = -1;
    activeLine = line;
    activeLineText = lineInfo.text;
    activeProgram = program;
    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.f;
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
    ClolyPockLineInfo lineInfo = getLineInfo(editorState.text, line);
    editorState.text.replace(lineInfo.begin, lineInfo.end - lineInfo.begin,
                             replacement);
    editorState.dirty = true;
  }

  void cancelPendingLine(int line) {
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
      errorLine = -1;
      syntaxError = false;
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
    ClolyPockLineInfo lineInfo = getLineInfo(editorState.text, line);
    if (line != activeLine) {
      if (focusChanged || lineInfo.text == checkedFocusedLineText) {
        checkedFocusedLineText = lineInfo.text;
        return;
      }

      checkedFocusedLineText = lineInfo.text;
      std::vector<ClolyPockProgramStep> program;
      if (parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
        pendingLine = line;
        pendingLineText = lineInfo.text;
        pendingProgram = program;
        if (errorLine == line) {
          errorLine = -1;
        }
        syntaxError = false;
      } else {
        if (pendingLine == line) {
          clearPendingLine();
        }
        errorLine = line;
        syntaxError = true;
      }
      return;
    }

    if (lineInfo.text == activeLineText) {
      if (pendingLine == line && viewingPendingLine) {
        clearPendingLine();
      }
      if (errorLine == line) {
        errorLine = -1;
        syntaxError = false;
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
    std::vector<ClolyPockProgramStep> program;
    if (parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      pendingLine = line;
      pendingLineText = lineInfo.text;
      pendingProgram = program;
      viewingPendingLine = true;
      if (errorLine == line) {
        errorLine = -1;
      }
      syntaxError = false;
    } else {
      if (pendingLine == line) {
        clearPendingLine();
      }
      errorLine = line;
      syntaxError = true;
    }
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
    activeStepPlays = true;
    if (resetPhase) {
      clockPhase = 0.f;
      activeClockRamp = 0.f;
    }
    clearPendingLine();
    syntaxError = false;
    errorLine = -1;
    applyActiveProgramStep();
  }

  void applyActiveProgramStep() {
    if (activeProgram.empty()) {
      return;
    }

    activeProgramIndex = std::max(
        0, std::min(activeProgramIndex, (int)activeProgram.size() - 1));
    const ClolyPockProgramStep& step = activeProgram[activeProgramIndex];
    refreshActiveClockSpecForStep();
    activeHighlightBegin = step.highlightBegin;
    activeHighlightEnd = step.highlightEnd;
    activeProgramElapsedSeconds = 0.f;
    scheduleTokenStartTick();
    chooseActiveStepPlayback();
  }

  void advanceActiveProgramBeat() {
    if (activeProgram.empty()) {
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

  void advanceActiveProgramStep() {
    bool hadMultipleSteps = activeProgram.size() > 1;
    activeProgramBeat = 0;
    activeProgramIndex++;
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

      ClolyPockLineInfo lineInfo = getLineInfo(editorState.text, nextLine);
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
    if (probability >= 100) {
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

    const ClolyPockProgramStep& step = activeProgram[activeProgramIndex];
    if (!step.hasRandomValue) {
      activeClockSpec = step.spec;
      return;
    }

    double minValue = step.literal.minValue;
    double maxValue = step.literal.maxValue;
    if (!step.literal.randomChoices.empty()) {
      size_t choiceIndex = std::min(
          (size_t)(random::uniform() * step.literal.randomChoices.size()),
          step.literal.randomChoices.size() - 1);
      minValue = step.literal.randomChoices[choiceIndex].minValue;
      maxValue = step.literal.randomChoices[choiceIndex].maxValue;
    }
    double lowValue = std::min(minValue, maxValue);
    double highValue = std::max(minValue, maxValue);
    double sampledValue = lowValue;
    if (highValue > lowValue) {
      sampledValue = lowValue + random::uniform() * (highValue - lowValue);
    }

    cloly::language::EvaluationResult eval =
        cloly::language::evaluateClockLiteralWithValue(step.literal,
                                                       sampledValue);
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

    const ClolyPockProgramStep& step = activeProgram[activeProgramIndex];
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

struct ClolyPockTitle : TransparentWidget {
  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 18.f);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgText(args.vg, box.size.x / 2.f, 0.f, "Cloly Pock", NULL);
  }
};

struct ClolyPockPanel : Widget {
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

struct ComputerscareClolyPockWidget : ModuleWidget {
  ClolyPockPanel* panel = nullptr;
  ClolyPockTitle* title = nullptr;
  ComputerscareTextEditor* editor = nullptr;
  Widget* syntaxErrorLight = nullptr;
  PortWidget* clockOutput = nullptr;
  PortWidget* eoc1Output = nullptr;
  PortWidget* eoc2Output = nullptr;
  PortWidget* eoc3Output = nullptr;
  ComputerscareResizeHandle* leftHandle = nullptr;
  ComputerscareResizeHandle* rightHandle = nullptr;
  ComputerscareTextEditorState browserEditorState;
  int lastCursorLine = -1;

  ComputerscareClolyPockWidget(ComputerscareClolyPock* module) {
    setModule(module);
    box.size =
        Vec(module ? module->width : ComputerscareClolyPock::MIN_WIDTH, 380.f);

    panel = new ClolyPockPanel();
    panel->box.size = box.size;
    addChild(panel);

    leftHandle = new ComputerscareResizeHandle();
    leftHandle->minWidth = ComputerscareClolyPock::MIN_WIDTH;
    addChild(leftHandle);

    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->minWidth = ComputerscareClolyPock::MIN_WIDTH;
    addChild(rightHandle);

    title = new ClolyPockTitle();
    title->box.pos = Vec(0.f, 11.f);
    title->box.size = Vec(box.size.x, 24.f);
    addChild(title);

    editor = createWidget<ComputerscareTextEditor>(Vec(3.f, 43.f));
    editor->box.size = Vec(box.size.x - 6.f, 291.f);
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
            Vec(127.f, 10.f), module,
            ComputerscareClolyPock::SYNTAX_ERROR_LIGHT);
    addChild(syntaxErrorLight);

    addInput(createInput<InPort>(Vec(8.f, 8.f), module,
                                 ComputerscareClolyPock::ADVANCE_INPUT));
    addParam(createParam<CKSS>(Vec(42.f, 12.f), module,
                               ComputerscareClolyPock::AUTO_ADVANCE_PARAM));

    clockOutput = createOutput<OutPort>(Vec(8.f, 340.f), module,
                                        ComputerscareClolyPock::CLOCK_OUTPUT);
    eoc1Output = createOutput<OutPort>(Vec(45.f, 340.f), module,
                                       ComputerscareClolyPock::EOC1_OUTPUT);
    eoc2Output = createOutput<OutPort>(Vec(82.f, 340.f), module,
                                       ComputerscareClolyPock::EOC2_OUTPUT);
    eoc3Output = createOutput<OutPort>(Vec(119.f, 340.f), module,
                                       ComputerscareClolyPock::EOC3_OUTPUT);
    addOutput(clockOutput);
    addOutput(eoc1Output);
    addOutput(eoc2Output);
    addOutput(eoc3Output);
    applyLayout();
  }

  void applyLayout() {
    box.size.x = std::max(box.size.x, ComputerscareClolyPock::MIN_WIDTH);
    if (panel) {
      panel->box.size = box.size;
    }
    if (title) {
      title->box.size.x = box.size.x;
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

    const float outputY = 340.f;
    PortWidget* outputs[] = {clockOutput, eoc1Output, eoc2Output, eoc3Output};
    const float outputXs[] = {8.f, 45.f, 82.f, 119.f};
    for (int i = 0; i < 4; i++) {
      if (outputs[i]) {
        outputs[i]->box.pos = Vec(outputXs[i], outputY);
      }
    }
  }

  void step() override {
    ModuleWidget::step();
    box.size.x = std::max(box.size.x, ComputerscareClolyPock::MIN_WIDTH);

    ComputerscareClolyPock* clolyPock =
        dynamic_cast<ComputerscareClolyPock*>(module);
    if (clolyPock) {
      if (std::fabs(clolyPock->width - box.size.x) > 0.01f) {
        clolyPock->width = box.size.x;
      }
    }
    applyLayout();

    if (editor) {
      ComputerscareTextEditorState* state = nullptr;
      bool blinkHigh = false;
      float activeProgress = 0.f;

      if (clolyPock) {
        editor->style.fontSize =
            clolyPock->params[ComputerscareClolyPock::EDITOR_FONT_SIZE_PARAM]
                .getValue();
        editor->style.fontWidthOffset =
            clolyPock->params[ComputerscareClolyPock::EDITOR_FONT_WIDTH_PARAM]
                .getValue();
        editor->style.fontHeightOffset =
            clolyPock->params[ComputerscareClolyPock::EDITOR_FONT_HEIGHT_PARAM]
                .getValue();
        editor->style.letterSpacing =
            clolyPock
                ->params[ComputerscareClolyPock::EDITOR_LETTER_SPACING_PARAM]
                .getValue();
        editor->style.lineWrapping = clolyPock->editorLineWrapping;
        state = &clolyPock->editorState;
        if (clolyPock->selectedLineDirty) {
          editor->setCursorLine(clolyPock->selectedLine);
          clolyPock->selectedLineDirty = false;
        }
        clolyPock->selectedLine = editor->getCursorLine();
        bool focusChanged = clolyPock->selectedLine != lastCursorLine;
        lastCursorLine = clolyPock->selectedLine;
        clolyPock->inspectFocusedLine(clolyPock->selectedLine, focusChanged);
        if (state->submitCount != clolyPock->lastSubmitCount) {
          clolyPock->lastSubmitCount = state->submitCount;
          if (clolyPock->pendingLine == clolyPock->selectedLine) {
            clolyPock->commitPendingLine(true);
          } else {
            clolyPock->commitLine(clolyPock->selectedLine, true);
          }
        }
        if (state->cancelCount != clolyPock->lastCancelCount) {
          clolyPock->lastCancelCount = state->cancelCount;
          clolyPock->cancelPendingLine(clolyPock->selectedLine);
          if (clolyPock->errorLine == clolyPock->selectedLine) {
            clolyPock->errorLine = -1;
            clolyPock->syntaxError = false;
          }
        }
        if (state->switchViewCount != clolyPock->lastSwitchViewCount) {
          clolyPock->lastSwitchViewCount = state->switchViewCount;
          clolyPock->togglePendingView();
        }
        blinkHigh = clolyPock->clockHigh && clolyPock->activeStepPlays;
        activeProgress = clolyPock->activeClockRamp;
      } else {
        state = &browserEditorState;
        float browserPhase = std::fmod(
            (float)rack::system::getTime() * ComputerscareClolyPock::CLOCK_HZ,
            1.f);
        blinkHigh = browserPhase < 0.5f;
        activeProgress = browserPhase;
      }

      state->highlights.clear();
      int line = editor->getCursorLine();
      ClolyPockLineInfo lineInfo = getLineInfo(state->text, line);
      ComputerscareTextHighlight focusHighlight;
      focusHighlight.begin = lineInfo.begin;
      focusHighlight.end = std::max(lineInfo.end, lineInfo.begin + 1);
      focusHighlight.fullLine = true;
      focusHighlight.background = nvgRGBA(0xff, 0xff, 0xff, 0x12);
      state->highlights.push_back(focusHighlight);
      bool showingPendingActiveLine =
          clolyPock && clolyPock->pendingLine == clolyPock->activeLine &&
          clolyPock->viewingPendingLine;
      bool showingActiveVersionOfPending =
          clolyPock && clolyPock->pendingLine == clolyPock->activeLine &&
          !clolyPock->viewingPendingLine;
      bool showingInvalidActiveLine =
          clolyPock && clolyPock->errorLine == clolyPock->activeLine;
      if (clolyPock && clolyPock->pendingLine >= 0 &&
          !showingActiveVersionOfPending) {
        ClolyPockLineInfo pendingLineInfo =
            getLineInfo(state->text, clolyPock->pendingLine);
        float pendingPulse =
            0.5f + 0.5f * std::sin((float)rack::system::getTime() * 3.5f);
        ComputerscareTextHighlight pendingHighlight;
        pendingHighlight.begin = pendingLineInfo.begin;
        pendingHighlight.end =
            std::max(pendingLineInfo.end, pendingLineInfo.begin + 1);
        pendingHighlight.fullLine = true;
        pendingHighlight.background = nvgRGBA(
            0x24, 0xc9, 0xa6, (unsigned char)(0x20 + pendingPulse * 0x22));
        state->highlights.push_back(pendingHighlight);
      }
      if (clolyPock && clolyPock->errorLine >= 0) {
        ClolyPockLineInfo errorLineInfo =
            getLineInfo(state->text, clolyPock->errorLine);
        ComputerscareTextHighlight errorHighlight;
        errorHighlight.begin = errorLineInfo.begin;
        errorHighlight.end =
            std::max(errorLineInfo.end, errorLineInfo.begin + 1);
        errorHighlight.fullLine = true;
        errorHighlight.background = nvgRGBA(0xc4, 0x34, 0x21, 0x35);
        state->highlights.push_back(errorHighlight);
      }
      ComputerscareTextHighlight activeHighlight;
      if (clolyPock) {
        activeHighlight.begin = clolyPock->activeHighlightBegin;
        activeHighlight.end = clolyPock->activeHighlightEnd;
      } else {
        activeHighlight.begin = lineInfo.begin;
        activeHighlight.end = lineInfo.end;
      }
      activeHighlight.hasBackground = blinkHigh;
      activeHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, 0x66);
      activeHighlight.hasBorder = true;
      activeHighlight.border = nvgRGBA(0xff, 0xee, 0x9a, 0xdd);
      activeHighlight.hasProgress = true;
      activeHighlight.progress = activeProgress;
      activeHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
      if (!showingPendingActiveLine && !showingInvalidActiveLine &&
          activeHighlight.begin < activeHighlight.end) {
        state->highlights.push_back(activeHighlight);
      }
      if (clolyPock && !showingPendingActiveLine && !showingInvalidActiveLine) {
        int repeatBegin = 0;
        int repeatEnd = 0;
        int repeatSegments = 0;
        float repeatProgress = 0.f;
        if (clolyPock->getActiveRepeatProgressHighlight(
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
    ComputerscareClolyPock* clolyPock =
        dynamic_cast<ComputerscareClolyPock*>(module);
    if (!clolyPock) {
      return;
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Editor"));
    menu->addChild(new MenuParamSlider(
        clolyPock
            ->paramQuantities[ComputerscareClolyPock::EDITOR_FONT_SIZE_PARAM]));
    menu->addChild(new MenuParamSlider(
        clolyPock->paramQuantities
            [ComputerscareClolyPock::EDITOR_FONT_WIDTH_PARAM]));
    menu->addChild(new MenuParamSlider(
        clolyPock->paramQuantities
            [ComputerscareClolyPock::EDITOR_FONT_HEIGHT_PARAM]));
    menu->addChild(new MenuParamSlider(
        clolyPock->paramQuantities
            [ComputerscareClolyPock::EDITOR_LETTER_SPACING_PARAM]));
    menu->addChild(createBoolPtrMenuItem("Line wrapping", "",
                                         &clolyPock->editorLineWrapping));
  }
};

Model* modelComputerscareClolyPock =
    createModel<ComputerscareClolyPock, ComputerscareClolyPockWidget>(
        "computerscare-cloly-pock");
