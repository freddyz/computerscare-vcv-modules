#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "../BlunchLanguage/BlunchLanguage.hpp"
#include "../Computerscare.hpp"
#include "../ComputerscarePolyModule.hpp"
#include "../ComputerscareResizableHandle.hpp"
#include "../ComputerscareTextEditor.hpp"
#include "BlunchEditorViews.hpp"
#include "BlunchRandomProgram.hpp"
#include "BlunchSequencerEngine.hpp"
#include "BlunchSequencerRuntime.hpp"

struct BlunchLineInfo {
  int begin = 0;
  int end = 0;
  std::string text;
};

static thread_local int blunchProcessingChannel = -1;

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

static int mapBlunchSourceOffsetToVisibleLine(const BlunchLineInfo& sourceLine,
                                              const BlunchLineInfo& visibleLine,
                                              int sourceOffset) {
  int relativeOffset = sourceOffset - sourceLine.begin;
  return std::max(
      visibleLine.begin,
      std::min(visibleLine.begin + relativeOffset, visibleLine.end));
}

static void addBlunchSequencerHighlights(
    std::vector<ComputerscareTextHighlight>& highlights,
    const BlunchSequencerRuntime& seq, const BlunchLineInfo& sourceLine,
    const BlunchLineInfo& visibleLine, bool includeClockProgress) {
  ComputerscareTextHighlight tokenHighlight;
  tokenHighlight.begin = mapBlunchSourceOffsetToVisibleLine(
      sourceLine, visibleLine, seq.activeHighlightBegin);
  tokenHighlight.end =
      std::max(tokenHighlight.begin,
               mapBlunchSourceOffsetToVisibleLine(sourceLine, visibleLine,
                                                  seq.activeHighlightEnd));
  tokenHighlight.hasBackground = seq.activeClockOutputHigh;
  tokenHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, 0x66);
  tokenHighlight.hasBorder = true;
  tokenHighlight.border = nvgRGBA(0xff, 0xee, 0x9a, 0xdd);
  tokenHighlight.hasProgress =
      includeClockProgress && seq.running &&
      !blunch::sequencer::activeStepUsesExternalClock(seq);
  tokenHighlight.progress = seq.activeClockRamp;
  tokenHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
  if (tokenHighlight.begin < tokenHighlight.end) {
    highlights.push_back(tokenHighlight);
  }

  blunch::sequencer::RepeatProgress repeatProgress;
  if (blunch::sequencer::getActiveRepeatProgressHighlight(seq,
                                                          repeatProgress)) {
    ComputerscareTextHighlight repeatProgressHighlight;
    repeatProgressHighlight.begin = mapBlunchSourceOffsetToVisibleLine(
        sourceLine, visibleLine, repeatProgress.begin);
    repeatProgressHighlight.end =
        std::max(repeatProgressHighlight.begin,
                 mapBlunchSourceOffsetToVisibleLine(sourceLine, visibleLine,
                                                    repeatProgress.end));
    repeatProgressHighlight.hasBackground = false;
    repeatProgressHighlight.hasProgress = true;
    repeatProgressHighlight.progress = repeatProgress.progress;
    repeatProgressHighlight.progressSegments = repeatProgress.segments;
    repeatProgressHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
    if (repeatProgressHighlight.begin < repeatProgressHighlight.end) {
      highlights.push_back(repeatProgressHighlight);
    }
  }
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

struct ComputerscareBlunch : ComputerscarePolyModule {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;
  static constexpr float MIN_WIDTH = 150.f;
  static constexpr int MAX_POLY_CHANNELS = 16;

  enum ParamIds {
    SPEED_OF_TIME_PARAM,
    POLY_CHANNELS_PARAM,
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

  ComputerscareTextEditorState sequenceEditorStates[MAX_POLY_CHANNELS];
  ComputerscareTextEditorState channelsEditorState;
  dsp::PulseGenerator tokenMovePulses[MAX_POLY_CHANNELS];
  dsp::PulseGenerator lineCyclePulses[MAX_POLY_CHANNELS];
  dsp::PulseGenerator wrapPulses[MAX_POLY_CHANNELS];
  dsp::SchmittTrigger externalClockTriggers[4];
  dsp::SchmittTrigger advanceTrigger;
  dsp::SchmittTrigger advanceTokenTrigger;
  dsp::SchmittTrigger advanceLineTrigger;
  dsp::SchmittTrigger resetTrigger;
  BlunchSequencerRuntime sequencers[MAX_POLY_CHANNELS];
  bool syntaxError = false;
  int selectedLine = 0;
  bool selectedLineDirty = false;
  bool randomizeCursorRestorePending = false;
  int randomizeCursorRestoreLine = 0;
  bool randomizeCursorRestoreAtEnd = false;
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
  int lastOpenCount = 0;
  int lastStopCount = 0;
  int lastHardStopCount = 0;
  int lastStartAllCount = 0;
  bool startAllRequested = false;
  int lastSwitchViewCount = 0;
  int lastNavigateChannelCount = 0;
  BlunchEditorViewMode editorViewMode = BlunchEditorViewMode::Sequence;
  int focusedChannel = 0;
  int playbackChannel = 0;
  int channelsCursorChannel = 0;
  int forcedChannelsCursorChannel = -1;
  bool editorViewDirty = true;
  bool lastRunHigh = true;
  bool externalClockHigh[4] = {false, false, false, false};
  float width = MIN_WIDTH;
  bool editorLineWrapping = true;

  int clampChannel(int channel) const {
    return std::max(0, std::min(channel, MAX_POLY_CHANNELS - 1));
  }

  int activeContextChannel() const {
    return blunchProcessingChannel >= 0 ? clampChannel(blunchProcessingChannel)
                                        : clampChannel(focusedChannel);
  }

  BlunchSequencerRuntime& activeSequencer() {
    return sequencerForChannel(activeContextChannel());
  }

  const BlunchSequencerRuntime& activeSequencer() const {
    return sequencerForChannel(activeContextChannel());
  }

  BlunchSequencerRuntime& sequencerForChannel(int channel) {
    return sequencers[clampChannel(channel)];
  }

  const BlunchSequencerRuntime& sequencerForChannel(int channel) const {
    return sequencers[clampChannel(channel)];
  }

  ComputerscareTextEditorState& sequenceEditorState() {
    return sequenceEditorStates[activeContextChannel()];
  }

  const ComputerscareTextEditorState& sequenceEditorState() const {
    return sequenceEditorStates[activeContextChannel()];
  }

  bool showingSequenceView() const {
    return editorViewMode == BlunchEditorViewMode::Sequence;
  }

  bool showingChannelsView() const {
    return editorViewMode == BlunchEditorViewMode::Channels;
  }

  bool autoLineAdvanceEnabled() {
    return params[AUTO_ADVANCE_PARAM].getValue() > 0.5f;
  }

  bool channelsEditorEditingEnabled() { return !autoLineAdvanceEnabled(); }

  ComputerscareTextEditorState& visibleEditorState() {
    return showingChannelsView() ? channelsEditorState : sequenceEditorState();
  }

  bool commitLineForChannel(int channel, int line, bool resetPhase) {
    int previousFocusedChannel = focusedChannel;
    focusedChannel = std::max(0, std::min(channel, MAX_POLY_CHANNELS - 1));
    bool committed = commitLine(line, resetPhase);
    focusedChannel = previousFocusedChannel;
    return committed;
  }

  void refreshChannelsEditorState(bool preserveCursor = false) {
    int cursorColumn = 0;
    int selectionColumn = 0;
    if (preserveCursor) {
      int cursorCurrentLine = 0;
      int selectionCurrentLine = 0;
      for (int i = 0; i < channelsEditorState.cursor &&
                      i < (int)channelsEditorState.text.size();
           i++) {
        if (channelsEditorState.text[i] == '\n') {
          cursorCurrentLine++;
          cursorColumn = 0;
        } else {
          cursorColumn++;
        }
      }
      for (int i = 0; i < channelsEditorState.selection &&
                      i < (int)channelsEditorState.text.size();
           i++) {
        if (channelsEditorState.text[i] == '\n') {
          selectionCurrentLine++;
          selectionColumn = 0;
        } else {
          selectionColumn++;
        }
      }
      if (cursorCurrentLine != selectionCurrentLine) {
        selectionColumn = cursorColumn;
      }
    }

    std::string nextText =
        buildBlunchChannelsViewText(sequencers, MAX_POLY_CHANNELS);
    bool textChanged = channelsEditorState.text != nextText;
    if (textChanged) {
      channelsEditorState.text = nextText;
      channelsEditorState.dirty = true;
    }
    int channelLine =
        std::max(0, std::min(channelsCursorChannel, MAX_POLY_CHANNELS - 1));
    BlunchLineInfo channelLineInfo =
        getLineInfo(channelsEditorState.text, channelLine);
    if (preserveCursor) {
      channelsEditorState.cursor =
          std::min(channelLineInfo.begin + cursorColumn, channelLineInfo.end);
      channelsEditorState.selection = std::min(
          channelLineInfo.begin + selectionColumn, channelLineInfo.end);
    } else {
      channelsEditorState.cursor = channelLineInfo.begin;
      channelsEditorState.selection = channelsEditorState.cursor;
    }
    if (!preserveCursor || textChanged) {
      editorViewDirty = true;
    }
  }

  void switchEditorView() {
    editorViewMode = switchBlunchEditorView(editorViewMode);
    editorViewDirty = true;
    if (showingChannelsView()) {
      channelsCursorChannel =
          std::max(0, std::min(focusedChannel, MAX_POLY_CHANNELS - 1));
      refreshChannelsEditorState();
    }
  }

  void openFocusedChannelEditPage() {
    int previousFocusedChannel = focusedChannel;
    focusedChannel =
        std::max(0, std::min(channelsCursorChannel, MAX_POLY_CHANNELS - 1));
    if (focusedChannel != previousFocusedChannel) {
      clearPendingLine();
      clearSyntaxError();
      checkedFocusedLineText.clear();
      randomizeCursorRestorePending = false;
    }
    selectedLine = activeSequencer().activeLine;
    selectedLineDirty = true;
    editorViewMode = BlunchEditorViewMode::Sequence;
    editorViewDirty = true;
  }

  void navigateFocusedChannel(int direction) {
    int previousFocusedChannel = focusedChannel;
    focusedChannel =
        (focusedChannel + direction + MAX_POLY_CHANNELS) % MAX_POLY_CHANNELS;
    if (focusedChannel != previousFocusedChannel) {
      clearPendingLine();
      clearSyntaxError();
      checkedFocusedLineText.clear();
      randomizeCursorRestorePending = false;
    }
    selectedLine = activeSequencer().activeLine;
    selectedLineDirty = true;
    editorViewMode = BlunchEditorViewMode::Sequence;
    editorViewDirty = true;
  }

  void returnToChannelsView() {
    channelsCursorChannel =
        std::max(0, std::min(focusedChannel, MAX_POLY_CHANNELS - 1));
    forcedChannelsCursorChannel = channelsCursorChannel;
    editorViewMode = BlunchEditorViewMode::Channels;
    editorViewDirty = true;
    refreshChannelsEditorState();
  }

  void stopSequencer(int channel) {
    BlunchSequencerRuntime& seq = sequencerForChannel(channel);
    seq.stopPlayback();
  }

  void hardStopAllSequencers() {
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      stopSequencer(channel);
    }
  }

  void startSequencer(int channel) {
    channel = std::max(0, std::min(channel, MAX_POLY_CHANNELS - 1));
    playbackChannel = channel;
    sequencerForChannel(channel).running = true;
    params[RUN_PARAM].setValue(1.f);
  }

  bool channelHasFormula(int channel) const {
    return !sequencerForChannel(channel).activeProgram.empty();
  }

  int automaticOutputChannelCount() const {
    int count = 1;
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      if (channelHasFormula(channel)) {
        count = channel + 1;
      }
    }
    return count;
  }

  int selectedOutputChannelCount() {
    int knobValue = (int)std::round(params[POLY_CHANNELS_PARAM].getValue());
    if (knobValue <= 0) {
      return automaticOutputChannelCount();
    }
    return std::max(1, std::min(knobValue, MAX_POLY_CHANNELS));
  }

  bool channelLabelIsActive(int channel) {
    return channel >= 0 && channel < selectedOutputChannelCount();
  }

  void triggerTokenMovePulse() {
    tokenMovePulses[activeContextChannel()].trigger(1e-3f);
  }

  void triggerLineCyclePulse() {
    lineCyclePulses[activeContextChannel()].trigger(1e-3f);
  }

  void triggerWrapPulse() { wrapPulses[activeContextChannel()].trigger(1e-3f); }

  ComputerscareBlunch() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam<BlunchSpeedOfTimeParamQuantity>(SPEED_OF_TIME_PARAM, -5.f, 5.f,
                                                0.f, "Speed of time");
    configSwitch(POLY_CHANNELS_PARAM, 0.f, 16.f, 0.f, "Poly Channels",
                 polyChannelLabels(true));
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
    getParamQuantity(POLY_CHANNELS_PARAM)->randomizeEnabled = false;
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
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      sequencers[channel].activeClockSpec.bpm = CLOCK_BPM;
      sequencers[channel].activeClockSpec.hz = CLOCK_HZ;
      sequencers[channel].activeClockSpec.periodSeconds = 1.f / CLOCK_HZ;
    }
    sequenceEditorStates[0].text = "120bpm\n33hz\n40ms\n";
    commitLine(0, true);
  }

  void processSequencerChannel(int channel, float scaledSampleTime,
                               const bool externalClockEdges[4], bool runHigh,
                               bool resetPressed, bool advanceLinePressed,
                               bool advanceTokenPressed) {
    int previousProcessingChannel = blunchProcessingChannel;
    blunchProcessingChannel = clampChannel(channel);
    BlunchSequencerRuntime& seq = activeSequencer();
    if (seq.activeProgram.empty()) {
      seq.clockHigh = false;
      seq.activeClockOutputHigh = false;
      seq.clockStartLowSamples = 0;
      seq.clockStartHighPending = false;
      outputs[CLOCK_OUTPUT].setVoltage(0.f, channel);
      blunchProcessingChannel = previousProcessingChannel;
      return;
    }
    int activeExternalClock = activeExternalClockInput(seq);
    int activeRepeatClock = activeRepeatExternalClockInput(seq);
    int activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
    bool running = runHigh && seq.running;
    if (running) {
      if (activeExternalClock >= 0) {
        if (!advanceActiveTotalDuration(seq, scaledSampleTime)) {
          advanceActiveProgramDuration(seq, scaledSampleTime);
        }
        activeExternalClock = activeExternalClockInput(seq);
        activeRepeatClock = activeRepeatExternalClockInput(seq);
        activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
        seq.activeClockRamp =
            activeExternalClock >= 0
                ? (externalClockHigh[activeExternalClock] ? 1.f : 0.f)
                : seq.clockPhase;
      } else {
        seq.clockPhase += scaledSampleTime * seq.activeClockSpec.hz;
        while (seq.clockPhase >= 1.f) {
          seq.clockPhase -= 1.f;
          if (activeRepeatClock < 0) {
            advanceActiveProgramBeat(seq, activeTotalTickClock < 0);
            activeRepeatClock = activeRepeatExternalClockInput(seq);
            activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
          }
        }
        if (!advanceActiveTotalDuration(seq, scaledSampleTime)) {
          advanceActiveProgramDuration(seq, scaledSampleTime);
        }
        activeExternalClock = activeExternalClockInput(seq);
        activeRepeatClock = activeRepeatExternalClockInput(seq);
        activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
        seq.activeClockRamp = seq.clockPhase;
      }
      seq.clockHigh = nextClockGateHigh(seq);
    } else {
      seq.activeClockRamp =
          activeExternalClock >= 0
              ? (externalClockHigh[activeExternalClock] ? 1.f : 0.f)
              : seq.clockPhase;
      seq.clockHigh = false;
    }
    bool outputClockHigh = activeExternalClock >= 0
                               ? externalClockHigh[activeExternalClock]
                               : seq.clockHigh;
    seq.activeClockOutputHigh =
        running && outputClockHigh && seq.activeStepPlays;
    outputs[CLOCK_OUTPUT].setVoltage(seq.activeClockOutputHigh ? 10.f : 0.f,
                                     channel);

    bool externalTotalTickAdvanced = false;
    if (running && activeTotalTickClock >= 0 &&
        externalClockEdges[activeTotalTickClock]) {
      externalTotalTickAdvanced = advanceActiveTotalTickCount(seq);
      activeRepeatClock = activeRepeatExternalClockInput(seq);
    }

    if (running && !externalTotalTickAdvanced && activeRepeatClock >= 0 &&
        externalClockEdges[activeRepeatClock]) {
      advanceActiveProgramBeat(seq,
                               activeTotalDurationExternalClockInput(seq) < 0);
    }

    if (runHigh && resetPressed) {
      resetActiveProgram(true);
    }
    if (runHigh && advanceLinePressed) {
      moveToNextLine(false);
    }
    if (runHigh && advanceTokenPressed) {
      advanceActiveProgramStep(seq, true);
    }

    blunchProcessingChannel = previousProcessingChannel;
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

    bool runHigh = params[RUN_PARAM].getValue() > 0.5f;
    bool startAllNow = startAllRequested;
    startAllRequested = false;
    if (startAllNow) {
      params[RUN_PARAM].setValue(1.f);
      runHigh = true;
      for (BlunchSequencerRuntime& channelSequencer : sequencers) {
        channelSequencer.running = true;
        if (!channelSequencer.activeProgram.empty()) {
          applyActiveProgramStep(channelSequencer);
        }
      }
    }
    if (!runHigh && lastRunHigh) {
      hardStopAllSequencers();
    }
    lastRunHigh = runHigh;
    bool resetPressed = resetTrigger.process(inputs[RESET_INPUT].getVoltage());
    bool advanceLinePressed =
        advanceLineTrigger.process(inputs[ADVANCE_LINE_INPUT].getVoltage());
    bool advanceTokenPressed =
        advanceTokenTrigger.process(inputs[ADVANCE_TOKEN_INPUT].getVoltage()) ||
        advanceTrigger.process(inputs[ADVANCE_INPUT].getVoltage());

    polyChannels = selectedOutputChannelCount();
    outputs[CLOCK_OUTPUT].setChannels(polyChannels);
    outputs[EOC1_OUTPUT].setChannels(polyChannels);
    outputs[EOC2_OUTPUT].setChannels(polyChannels);
    outputs[EOC3_OUTPUT].setChannels(polyChannels);
    for (int channel = 0; channel < polyChannels; channel++) {
      processSequencerChannel(channel, scaledSampleTime, externalClockEdges,
                              runHigh, resetPressed, advanceLinePressed,
                              advanceTokenPressed);
    }
    for (int channel = polyChannels; channel < MAX_POLY_CHANNELS; channel++) {
      sequencers[channel].activeClockOutputHigh = false;
    }
    for (int channel = 0; channel < polyChannels; channel++) {
      outputs[EOC1_OUTPUT].setVoltage(
          tokenMovePulses[channel].process(args.sampleTime) ? 10.f : 0.f,
          channel);
      outputs[EOC2_OUTPUT].setVoltage(
          lineCyclePulses[channel].process(args.sampleTime) ? 10.f : 0.f,
          channel);
      outputs[EOC3_OUTPUT].setVoltage(
          wrapPulses[channel].process(args.sampleTime) ? 10.f : 0.f, channel);
    }
    for (int channel = polyChannels; channel < MAX_POLY_CHANNELS; channel++) {
      tokenMovePulses[channel].process(args.sampleTime);
      lineCyclePulses[channel].process(args.sampleTime);
      wrapPulses[channel].process(args.sampleTime);
    }
    lights[SYNTAX_ERROR_LIGHT].setBrightness(syntaxError ? 1.f : 0.f);
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_t* channelTextsJ = json_array();
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      const std::string& text = sequenceEditorStates[channel].text;
      json_array_append_new(channelTextsJ,
                            json_stringn(text.c_str(), text.size()));
    }
    json_object_set_new(rootJ, "channelTexts", channelTextsJ);
    json_object_set_new(rootJ, "text",
                        json_stringn(sequenceEditorStates[0].text.c_str(),
                                     sequenceEditorStates[0].text.size()));
    json_t* activeLinesJ = json_array();
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      json_array_append_new(activeLinesJ,
                            json_integer(sequencers[channel].activeLine));
    }
    json_object_set_new(rootJ, "activeLines", activeLinesJ);
    json_object_set_new(rootJ, "focusedLine", json_integer(selectedLine));
    json_object_set_new(rootJ, "focusedChannel", json_integer(focusedChannel));
    json_object_set_new(rootJ, "playbackChannel",
                        json_integer(playbackChannel));
    json_object_set_new(rootJ, "activeLine",
                        json_integer(activeSequencer().activeLine));
    json_object_set_new(rootJ, "width", json_real(width));
    json_object_set_new(
        rootJ, "editorViewMode",
        json_string(showingChannelsView() ? "channels" : "sequence"));
    json_object_set_new(rootJ, "editorLineWrapping",
                        json_boolean(editorLineWrapping));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* channelTextsJ = json_object_get(rootJ, "channelTexts");
    if (channelTextsJ) {
      size_t index;
      json_t* textJ;
      json_array_foreach(channelTextsJ, index, textJ) {
        if (index >= MAX_POLY_CHANNELS) {
          break;
        }
        if (json_is_string(textJ)) {
          sequenceEditorStates[index].text = json_string_value(textJ);
          sequenceEditorStates[index].dirty = true;
        }
      }
    }
    json_t* textJ = json_object_get(rootJ, "text");
    if (textJ && !channelTextsJ) {
      sequenceEditorStates[0].text = json_string_value(textJ);
      sequenceEditorStates[0].dirty = true;
    }
    json_t* focusedLineJ = json_object_get(rootJ, "focusedLine");
    if (focusedLineJ) {
      selectedLine = json_integer_value(focusedLineJ);
      selectedLineDirty = true;
    }
    json_t* focusedChannelJ = json_object_get(rootJ, "focusedChannel");
    if (focusedChannelJ) {
      focusedChannel =
          std::max(0, std::min((int)json_integer_value(focusedChannelJ),
                               MAX_POLY_CHANNELS - 1));
      channelsCursorChannel = focusedChannel;
    }
    json_t* playbackChannelJ = json_object_get(rootJ, "playbackChannel");
    if (playbackChannelJ) {
      playbackChannel =
          std::max(0, std::min((int)json_integer_value(playbackChannelJ),
                               MAX_POLY_CHANNELS - 1));
    }
    json_t* activeLinesJ = json_object_get(rootJ, "activeLines");
    if (activeLinesJ) {
      size_t index;
      json_t* activeLineJ;
      json_array_foreach(activeLinesJ, index, activeLineJ) {
        if (index >= MAX_POLY_CHANNELS) {
          break;
        }
        if (json_is_integer(activeLineJ)) {
          sequencers[index].activeLine = json_integer_value(activeLineJ);
        }
      }
    }
    json_t* activeLineJ = json_object_get(rootJ, "activeLine");
    if (activeLineJ && !activeLinesJ) {
      activeSequencer().activeLine = json_integer_value(activeLineJ);
    }
    int previousFocusedChannel = focusedChannel;
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      if (sequenceEditorStates[channel].text.empty()) {
        continue;
      }
      focusedChannel = channel;
      int line = std::max(
          0, std::min(sequencers[channel].activeLine,
                      getLineCount(sequenceEditorStates[channel].text) - 1));
      sequencers[channel].activeLine = line;
      commitLine(line, true);
    }
    focusedChannel = previousFocusedChannel;
    json_t* editorViewModeJ = json_object_get(rootJ, "editorViewMode");
    if (editorViewModeJ) {
      std::string mode = json_string_value(editorViewModeJ);
      editorViewMode = mode == "channels" ? BlunchEditorViewMode::Channels
                                          : BlunchEditorViewMode::Sequence;
      editorViewDirty = true;
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

  void onRandomize() override {
    if (showingChannelsView()) {
      randomizeActiveChannelsLine();
    } else {
      randomizeSelectedLine();
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
    BlunchLineInfo lineInfo = getLineInfo(sequenceEditorState().text, line);
    std::vector<BlunchProgramStep> program;
    if (!parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      setSyntaxError(line, line == activeSequencer().activeLine
                               ? activeSequencer().activeLineText
                               : checkedFocusedLineText);
      return false;
    }

    clearSyntaxError();
    activeSequencer().loadActiveProgram(line, lineInfo.text, program,
                                        resetPhase);
    if (pendingLine == line && pendingLineText == lineInfo.text) {
      clearPendingLine();
    }
    applyActiveProgramStep(activeSequencer());
    return true;
  }

  void clearPendingLine() {
    pendingLine = -1;
    pendingLineText.clear();
    pendingProgram.clear();
    viewingPendingLine = true;
  }

  void replaceLineText(int line, const std::string& replacement) {
    BlunchLineInfo lineInfo = getLineInfo(sequenceEditorState().text, line);
    sequenceEditorState().text.replace(
        lineInfo.begin, lineInfo.end - lineInfo.begin, replacement);
    sequenceEditorState().dirty = true;
  }

  void replaceLineTextForChannel(int channel, int line,
                                 const std::string& replacement) {
    channel = clampChannel(channel);
    BlunchLineInfo lineInfo =
        getLineInfo(sequenceEditorStates[channel].text, line);
    sequenceEditorStates[channel].text.replace(
        lineInfo.begin, lineInfo.end - lineInfo.begin, replacement);
    sequenceEditorStates[channel].dirty = true;
  }

  void syncChannelsEditorEdit(int channel) {
    if (!channelsEditorEditingEnabled()) {
      return;
    }
    if (channelsEditorState.text.empty()) {
      return;
    }
    channel = clampChannel(channel);
    if (channel >= getLineCount(channelsEditorState.text)) {
      return;
    }

    BlunchSequencerRuntime& seq = sequencerForChannel(channel);
    BlunchLineInfo rowInfo = getLineInfo(channelsEditorState.text, channel);
    if (rowInfo.text == seq.activeLineText) {
      return;
    }

    int activeLine = std::max(
        0, std::min(seq.activeLine,
                    getLineCount(sequenceEditorStates[channel].text) - 1));
    replaceLineTextForChannel(channel, activeLine, rowInfo.text);
    seq.activeLine = activeLine;
    seq.activeLineText = rowInfo.text;
    if (isBlankLine(rowInfo.text)) {
      seq.activeProgram.clear();
      seq.activeClockOutputHigh = false;
      seq.clockHigh = false;
    }
  }

  void randomizeLineForChannel(int channel, int line, bool restoreCursor) {
    int previousFocusedChannel = focusedChannel;
    focusedChannel = clampChannel(channel);
    unsigned int entropy =
        (unsigned int)std::floor(random::uniform() * 4294967295.f);
    std::string program =
        blunch::random_program::generateMusicalClockProgram(entropy);
    line = std::max(
        0, std::min(line, getLineCount(sequenceEditorState().text) - 1));
    BlunchLineInfo oldLineInfo = getLineInfo(sequenceEditorState().text, line);
    bool cursorWasAtEnd = sequenceEditorState().cursor >= oldLineInfo.end;
    replaceLineText(line, program);
    selectedLine = line;
    BlunchLineInfo newLineInfo = getLineInfo(sequenceEditorState().text, line);
    if (restoreCursor) {
      sequenceEditorState().cursor =
          cursorWasAtEnd ? newLineInfo.end : newLineInfo.begin;
      sequenceEditorState().selection = sequenceEditorState().cursor;
      randomizeCursorRestorePending = true;
      randomizeCursorRestoreLine = line;
      randomizeCursorRestoreAtEnd = cursorWasAtEnd;
    }
    checkedFocusedLineText = program;
    clearSyntaxError();
    if (line == activeSequencer().activeLine) {
      clearPendingLine();
      commitLine(line, true);
      focusedChannel = previousFocusedChannel;
      return;
    }

    std::vector<BlunchProgramStep> parsedProgram;
    if (parseLineProgram(line, newLineInfo.begin, program, parsedProgram)) {
      pendingLine = line;
      pendingLineText = program;
      pendingProgram = parsedProgram;
      viewingPendingLine = true;
    }
    focusedChannel = previousFocusedChannel;
  }

  void randomizeSelectedLine() {
    randomizeLineForChannel(focusedChannel, selectedLine, true);
  }

  void randomizeActiveChannelsLine() {
    int channel = clampChannel(channelsCursorChannel);
    randomizeLineForChannel(channel, sequencerForChannel(channel).activeLine,
                            false);
    refreshChannelsEditorState(true);
  }

  void cancelPendingLine(int line) {
    if (errorLine == line) {
      std::string revertText;
      if (line == activeSequencer().activeLine) {
        revertText = activeSequencer().activeLineText;
      } else if (pendingLine == line && !pendingLineText.empty()) {
        revertText = pendingLineText;
      } else if (errorLineRevertLine == line) {
        revertText = errorLineRevertText;
      }
      replaceLineText(line, revertText);
      if (pendingLine == line && line == activeSequencer().activeLine) {
        clearPendingLine();
      }
      checkedFocusedLineText =
          getLineInfo(sequenceEditorState().text, line).text;
      clearSyntaxError();
      return;
    }

    if (pendingLine == line) {
      if (line == activeSequencer().activeLine) {
        replaceLineText(line, activeSequencer().activeLineText);
      }
      clearPendingLine();
    }
    checkedFocusedLineText = getLineInfo(sequenceEditorState().text, line).text;
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
    if (pendingLine != activeSequencer().activeLine) {
      return;
    }

    replaceLineText(activeSequencer().activeLine,
                    activeSequencer().activeLineText);
    checkedFocusedLineText = activeSequencer().activeLineText;
    viewingPendingLine = false;
  }

  void togglePendingView() {
    if (errorLine == activeSequencer().activeLine) {
      replaceLineText(activeSequencer().activeLine,
                      activeSequencer().activeLineText);
      checkedFocusedLineText = activeSequencer().activeLineText;
      clearSyntaxError();
      viewingPendingLine = false;
      return;
    }

    if (pendingLine != activeSequencer().activeLine) {
      return;
    }

    if (viewingPendingLine) {
      showActiveLineVersion();
    } else {
      showPendingLine();
    }
  }

  void inspectFocusedLine(int line, bool focusChanged) {
    BlunchLineInfo lineInfo = getLineInfo(sequenceEditorState().text, line);
    std::string previousCheckedLineText = checkedFocusedLineText;
    if (line != activeSequencer().activeLine) {
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

    if (lineInfo.text == activeSequencer().activeLineText) {
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
      setSyntaxError(line, activeSequencer().activeLineText);
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

    activeSequencer().loadActiveProgram(pendingLine, pendingLineText,
                                        pendingProgram, resetPhase);
    clearPendingLine();
    clearSyntaxError();
    applyActiveProgramStep(activeSequencer());
  }

  void resetActiveProgram(bool resetPhase) {
    if (activeSequencer().activeProgram.empty()) {
      commitLine(activeSequencer().activeLine, resetPhase);
      return;
    }

    activeSequencer().resetActiveProgramState(resetPhase);
    applyActiveProgramStep(activeSequencer());
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

  void sampleTotalDurationGroup(BlunchSequencerRuntime& seq, int stepIndex) {
    if (seq.activeProgram.empty()) {
      return;
    }

    BlunchProgramStep& step = seq.activeProgram[stepIndex];
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
      seq.activeProgram[i].totalDurationIsTickCount = selectedIsTickCount;
      seq.activeProgram[i].totalDurationTicks = selectedTicks;
      seq.activeProgram[i].totalDurationSeconds = selectedSeconds;
      seq.activeProgram[i].totalDurationHighlightBegin =
          seq.activeProgram[i].sourceLineBegin + choice.range.begin;
      seq.activeProgram[i].totalDurationHighlightEnd =
          seq.activeProgram[i].sourceLineBegin + choice.range.end;
    }
  }

  void applyActiveProgramStep(BlunchSequencerRuntime& seq) {
    if (seq.activeProgram.empty()) {
      return;
    }

    seq.activeProgramIndex = std::max(
        0, std::min(seq.activeProgramIndex, (int)seq.activeProgram.size() - 1));
    BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    sampleStepRepeatArgument(step);
    refreshActiveClockSpecForStep(seq);
    seq.activeHighlightBegin = step.highlightBegin;
    seq.activeHighlightEnd = step.highlightEnd;
    seq.activeProgramElapsedSeconds = 0.f;
    if (step.hasTotalDurationGroup) {
      if (seq.activeTotalDurationGroupId != step.totalDurationGroupId) {
        seq.activeTotalDurationGroupId = step.totalDurationGroupId;
        seq.activeTotalDurationElapsedSeconds = 0.f;
        seq.activeTotalDurationTicks = 0;
        sampleTotalDurationGroup(seq, seq.activeProgramIndex);
      }
    } else {
      seq.activeTotalDurationGroupId = -1;
      seq.activeTotalDurationElapsedSeconds = 0.f;
      seq.activeTotalDurationTicks = 0;
    }
    scheduleTokenStartTick(seq);
    chooseActiveStepPlayback(seq);
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

  int activeExternalClockInput(const BlunchSequencerRuntime& seq) const {
    return blunch::sequencer::activeExternalClockInput(seq);
  }

  int activeRepeatExternalClockInput(const BlunchSequencerRuntime& seq) const {
    return blunch::sequencer::activeRepeatExternalClockInput(seq);
  }

  int activeTotalDurationExternalClockInput(
      const BlunchSequencerRuntime& seq) const {
    return blunch::sequencer::activeTotalDurationExternalClockInput(seq);
  }

  bool activeStepUsesExternalClock() const {
    return blunch::sequencer::activeStepUsesExternalClock(activeSequencer());
  }

  float getTimeScale() {
    return std::pow(2.f, params[SPEED_OF_TIME_PARAM].getValue());
  }

  void advanceActiveProgramBeat(BlunchSequencerRuntime& seq,
                                bool countTotalTick = true) {
    if (seq.activeProgram.empty()) {
      return;
    }

    if (countTotalTick && advanceActiveTotalTickCount(seq)) {
      return;
    }

    if (seq.activeProgram[seq.activeProgramIndex].hasDuration) {
      refreshActiveClockSpecForStep(seq);
      chooseActiveStepPlayback(seq);
      return;
    }

    seq.activeProgramBeat++;
    if (seq.activeProgramBeat >=
        seq.activeProgram[seq.activeProgramIndex].repeat) {
      advanceActiveProgramStep(seq);
    } else {
      refreshActiveClockSpecForStep(seq);
      chooseActiveStepPlayback(seq);
    }
  }

  void advanceActiveProgramDuration(BlunchSequencerRuntime& seq,
                                    float sampleTime) {
    if (blunch::sequencer::advanceProgramDuration(seq, sampleTime)) {
      advanceActiveProgramStep(seq);
    }
  }

  bool advanceActiveTotalDuration(BlunchSequencerRuntime& seq,
                                  float sampleTime) {
    if (!blunch::sequencer::advanceTotalDuration(seq, sampleTime)) {
      return false;
    }
    finishActiveTotalDurationGroup(seq);
    return true;
  }

  bool advanceActiveTotalTickCount(BlunchSequencerRuntime& seq) {
    if (!blunch::sequencer::advanceTotalTickCount(seq)) {
      return false;
    }
    finishActiveTotalDurationGroup(seq);
    return true;
  }

  void finishActiveTotalDurationGroup(BlunchSequencerRuntime& seq) {
    if (seq.activeProgramIndex >= (int)seq.activeProgram.size()) {
      seq.activeProgramIndex = 0;
      bool movedLine = handleLineCycleEnd();
      if (movedLine || seq.activeProgram.size() > 1) {
        triggerTokenMovePulse();
      }
    } else {
      applyActiveProgramStep(seq);
      triggerTokenMovePulse();
    }
  }

  bool autoBlockAdvanceEnabled() {
    return params[AUTO_BLOCK_ADVANCE_PARAM].getValue() > 0.5f;
  }

  void repeatActiveProgramStep(BlunchSequencerRuntime& seq) {
    blunch::sequencer::repeatProgramStep(seq);
    applyActiveProgramStep(seq);
  }

  void advanceActiveProgramStep(BlunchSequencerRuntime& seq,
                                bool forceAdvance = false) {
    if (seq.activeProgram.empty()) {
      return;
    }

    if (!forceAdvance && !autoBlockAdvanceEnabled()) {
      repeatActiveProgramStep(seq);
      return;
    }

    bool hadMultipleSteps = seq.activeProgram.size() > 1;
    const BlunchProgramStep& currentStep =
        seq.activeProgram[seq.activeProgramIndex];
    seq.activeProgramBeat = 0;
    seq.activeProgramIndex++;
    if (currentStep.hasTotalDurationGroup &&
        seq.activeProgramIndex >= currentStep.totalDurationGroupEnd) {
      seq.activeProgramIndex = currentStep.totalDurationGroupStart;
      applyActiveProgramStep(seq);
      if (hadMultipleSteps) {
        triggerTokenMovePulse();
      }
      return;
    }

    if (seq.activeProgramIndex >= (int)seq.activeProgram.size()) {
      seq.activeProgramIndex = 0;
      bool movedLine = handleLineCycleEnd();
      if (movedLine || hadMultipleSteps) {
        triggerTokenMovePulse();
      }
    } else {
      applyActiveProgramStep(seq);
      triggerTokenMovePulse();
    }
  }

  bool handleLineCycleEnd() {
    triggerLineCyclePulse();
    if (params[AUTO_ADVANCE_PARAM].getValue() > 0.5f) {
      if (!moveToNextLine(false, false)) {
        applyActiveProgramStep(activeSequencer());
        return false;
      }
      return true;
    }

    applyActiveProgramStep(activeSequencer());
    return false;
  }

  bool moveToNextLine(bool resetPhase, bool emitTokenMove = true) {
    int lineCount = getLineCount(sequenceEditorState().text);
    if (lineCount <= 0) {
      return false;
    }

    bool wrapped = false;
    int previousLine = activeSequencer().activeLine;
    int previousHighlightBegin = activeSequencer().activeHighlightBegin;
    int previousHighlightEnd = activeSequencer().activeHighlightEnd;
    for (int offset = 1; offset <= lineCount; offset++) {
      int nextLine = activeSequencer().activeLine + offset;
      if (nextLine >= lineCount) {
        nextLine -= lineCount;
        wrapped = true;
      }

      BlunchLineInfo lineInfo =
          getLineInfo(sequenceEditorState().text, nextLine);
      if (isBlankLine(lineInfo.text)) {
        continue;
      }

      if (!commitLine(nextLine, resetPhase)) {
        return false;
      }

      if (emitTokenMove &&
          (activeSequencer().activeLine != previousLine ||
           activeSequencer().activeHighlightBegin != previousHighlightBegin ||
           activeSequencer().activeHighlightEnd != previousHighlightEnd)) {
        triggerTokenMovePulse();
      }
      if (wrapped) {
        triggerWrapPulse();
      }
      return true;
    }

    return false;
  }

  void chooseActiveStepPlayback(BlunchSequencerRuntime& seq) {
    blunch::sequencer::chooseStepPlayback(seq, random::uniform());
  }

  void refreshActiveClockSpecForStep(BlunchSequencerRuntime& seq) {
    if (seq.activeProgram.empty()) {
      return;
    }

    BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    if (!step.hasRandomValue) {
      seq.activeClockSpec = step.spec;
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
        seq.activeClockSpec = step.spec;
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
      seq.activeClockSpec = eval.spec;
    } else {
      seq.activeClockSpec = step.spec;
    }
  }

  void scheduleTokenStartTick(BlunchSequencerRuntime& seq) {
    seq.scheduleTokenStartTick();
  }

  bool nextClockGateHigh(BlunchSequencerRuntime& seq) {
    return blunch::sequencer::nextClockGateHigh(
        seq, activeExternalClockInput(seq) >= 0);
  }

  bool getActiveRepeatProgressHighlight(int& begin, int& end, float& progress) {
    int unusedSegments = 0;
    return getActiveRepeatProgressHighlight(begin, end, progress,
                                            unusedSegments);
  }

  bool getActiveRepeatProgressHighlight(int& begin, int& end, float& progress,
                                        int& segments) {
    blunch::sequencer::RepeatProgress highlight;
    if (!blunch::sequencer::getActiveRepeatProgressHighlight(activeSequencer(),
                                                             highlight)) {
      return false;
    }

    begin = highlight.begin;
    end = highlight.end;
    progress = highlight.progress;
    segments = highlight.segments;
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
    nvgFontSize(args.vg, 15.f);
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

struct BlunchChannelLabel : TransparentWidget {
  ComputerscareBlunch* module = nullptr;

  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    int channel = 0;
    if (module) {
      channel = module->showingChannelsView() ? module->channelsCursorChannel
                                              : module->focusedChannel;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13.f);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    std::string label = module && module->showingChannelsView()
                            ? "All - Ch" + std::to_string(channel + 1)
                            : "Ch." + std::to_string(channel + 1) + " Sequence";
    nvgText(args.vg, box.size.x * 0.5f, 0.f, label.c_str(), NULL);
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
  BlunchChannelLabel* channelLabel = nullptr;
  PolyOutputChannelsWidget* polyChannelsWidget = nullptr;
  BlunchExternalClockLabels* externalClockLabels = nullptr;
  ComputerscareTextEditor* editor = nullptr;
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
    title->box.pos = Vec(0.f, 1.f);
    title->box.size = Vec(box.size.x, 18.f);
    addChild(title);

    speedOfTimeLabel = new BlunchSpeedOfTimeLabel();
    speedOfTimeLabel->box.pos = Vec(4.f, 4.f);
    speedOfTimeLabel->box.size = Vec(28.f, 9.f);
    addChild(speedOfTimeLabel);

    channelLabel = new BlunchChannelLabel();
    channelLabel->module = module;
    channelLabel->box.pos = Vec(0.f, 30.f);
    channelLabel->box.size = Vec(box.size.x, 13.f);
    addChild(channelLabel);

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
      editor->setState(&module->sequenceEditorState());
    } else {
      browserEditorState.text = "120bpm\n33hz\n40ms\n";
      editor->setState(&browserEditorState);
    }
    addChild(editor);

    BlunchAdvanceModeButton* runButton = createParam<BlunchAdvanceModeButton>(
        Vec(36.f, 8.f), module, ComputerscareBlunch::RUN_PARAM);
    runButton->label = "Run";
    addParam(runButton);

    BlunchAdvanceModeButton* lineAdvanceButton =
        createParam<BlunchAdvanceModeButton>(
            Vec(58.f, 8.f), module, ComputerscareBlunch::AUTO_ADVANCE_PARAM);
    lineAdvanceButton->label = "Line";
    addParam(lineAdvanceButton);

    BlunchAdvanceModeButton* blockAdvanceButton =
        createParam<BlunchAdvanceModeButton>(
            Vec(80.f, 8.f), module,
            ComputerscareBlunch::AUTO_BLOCK_ADVANCE_PARAM);
    blockAdvanceButton->label = "Block";
    addParam(blockAdvanceButton);

    polyChannelsWidget =
        new PolyOutputChannelsWidget(Vec(box.size.x - 27.f, 18.f), module,
                                     ComputerscareBlunch::POLY_CHANNELS_PARAM);
    addChild(polyChannelsWidget);

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
      title->box.pos = Vec(0.f, 1.f);
      title->box.size.x = box.size.x;
    }
    if (speedOfTimeLabel) {
      speedOfTimeLabel->box.pos = Vec(4.f, 4.f);
    }
    if (channelLabel) {
      channelLabel->box.pos = Vec(0.f, 30.f);
      channelLabel->box.size.x = box.size.x;
    }
    if (externalClockLabels) {
      externalClockLabels->box.size.x = box.size.x;
    }
    if (editor) {
      editor->box.size.x = box.size.x - 6.f;
    }
    if (polyChannelsWidget) {
      polyChannelsWidget->box.pos = Vec(0.f, 0.f);
      if (polyChannelsWidget->channelCountDisplay) {
        polyChannelsWidget->channelCountDisplay->box.pos =
            Vec(box.size.x - 27.f, 18.f);
      }
      if (polyChannelsWidget->channelsKnob) {
        polyChannelsWidget->channelsKnob->box.pos =
            Vec(box.size.x - 20.f, 21.f);
      }
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
        editor->style.lineWrapping =
            blunch->showingSequenceView() && blunch->editorLineWrapping;
        editor->openOnEnter = blunch->showingChannelsView();
        editor->stopShortcutEnabled = true;
        editor->readOnly = blunch->showingChannelsView() &&
                           !blunch->channelsEditorEditingEnabled();
        if (editor->commands.switchViewCount() != blunch->lastSwitchViewCount) {
          blunch->lastSwitchViewCount = editor->commands.switchViewCount();
          if (blunch->showingChannelsView() &&
              editor->state == &blunch->channelsEditorState) {
            blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                editor->getCursorLine(),
                ComputerscareBlunch::MAX_POLY_CHANNELS);
            blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
          }
          blunch->switchEditorView();
        }
        if (editor->commands.navigateChannelCount() !=
            blunch->lastNavigateChannelCount) {
          blunch->lastNavigateChannelCount =
              editor->commands.navigateChannelCount();
          if (blunch->showingSequenceView()) {
            int direction =
                editor->commands.navigateChannelForwardCount >=
                        editor->commands.navigateChannelBackwardCount
                    ? 1
                    : -1;
            blunch->navigateFocusedChannel(direction);
          }
        }
        if (editor->commands.openCount != blunch->lastOpenCount) {
          blunch->lastOpenCount = editor->commands.openCount;
          if (blunch->showingChannelsView()) {
            blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                editor->getCursorLine(),
                ComputerscareBlunch::MAX_POLY_CHANNELS);
            if (editor->state == &blunch->channelsEditorState) {
              blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
            }
            blunch->openFocusedChannelEditPage();
          }
        }
        if (editor->commands.startAllCount != blunch->lastStartAllCount) {
          blunch->lastStartAllCount = editor->commands.startAllCount;
          blunch->startAllRequested = true;
        }
        if (editor->commands.hardStopCount != blunch->lastHardStopCount) {
          blunch->lastHardStopCount = editor->commands.hardStopCount;
          blunch->params[ComputerscareBlunch::RUN_PARAM].setValue(0.f);
        }
        if (editor->commands.stopCount != blunch->lastStopCount) {
          blunch->lastStopCount = editor->commands.stopCount;
          if (blunch->showingChannelsView()) {
            blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                editor->getCursorLine(),
                ComputerscareBlunch::MAX_POLY_CHANNELS);
            if (editor->state == &blunch->channelsEditorState) {
              blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
            }
            blunch->stopSequencer(blunch->channelsCursorChannel);
            blunch->refreshChannelsEditorState();
          } else {
            blunch->stopSequencer(blunch->focusedChannel);
          }
        }
        if (editor->commands.submitCount != blunch->lastSubmitCount &&
            blunch->showingChannelsView()) {
          blunch->lastSubmitCount = editor->commands.submitCount;
          if (editor->state == &blunch->channelsEditorState) {
            blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                editor->getCursorLine(),
                ComputerscareBlunch::MAX_POLY_CHANNELS);
            blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
          }
          int previousFocusedChannel = blunch->focusedChannel;
          blunch->focusedChannel = blunch->channelsCursorChannel;
          bool committed =
              blunch->commitLine(blunch->activeSequencer().activeLine, true);
          if (committed) {
            blunch->startSequencer(blunch->focusedChannel);
            blunch->refreshChannelsEditorState();
          }
          blunch->focusedChannel = previousFocusedChannel;
        }
        if (blunch->showingChannelsView()) {
          if (blunch->forcedChannelsCursorChannel >= 0) {
            blunch->channelsCursorChannel = std::max(
                0, std::min(blunch->forcedChannelsCursorChannel,
                            ComputerscareBlunch::MAX_POLY_CHANNELS - 1));
            blunch->forcedChannelsCursorChannel = -1;
          } else if (editor->state == &blunch->channelsEditorState) {
            blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
            blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                editor->getCursorLine(),
                ComputerscareBlunch::MAX_POLY_CHANNELS);
            blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
          }
          blunch->refreshChannelsEditorState(true);
        }
        state = &blunch->visibleEditorState();
        if (editor->state != state || blunch->editorViewDirty) {
          editor->state = state;
          editor->syncFromState();
          blunch->editorViewDirty = false;
          lastCursorLine = editor->getCursorLine();
        }
        if (blunch->showingSequenceView()) {
          if (blunch->selectedLineDirty) {
            editor->setCursorLine(blunch->selectedLine);
            blunch->selectedLineDirty = false;
          }
          if (blunch->randomizeCursorRestorePending) {
            editor->setCursorLineEdge(blunch->randomizeCursorRestoreLine,
                                      blunch->randomizeCursorRestoreAtEnd);
            blunch->randomizeCursorRestorePending = false;
          }
          blunch->selectedLine = editor->getCursorLine();
          bool focusChanged = blunch->selectedLine != lastCursorLine;
          lastCursorLine = blunch->selectedLine;
          blunch->inspectFocusedLine(blunch->selectedLine, focusChanged);
          if (editor->commands.submitCount != blunch->lastSubmitCount) {
            blunch->lastSubmitCount = editor->commands.submitCount;
            bool committed = false;
            if (blunch->pendingLine == blunch->selectedLine) {
              blunch->commitPendingLine(true);
              committed = true;
            } else {
              committed = blunch->commitLine(blunch->selectedLine, true);
            }
            if (committed) {
              blunch->startSequencer(blunch->focusedChannel);
            }
          }
          if (editor->commands.cancelCount != blunch->lastCancelCount) {
            blunch->lastCancelCount = editor->commands.cancelCount;
            blunch->cancelPendingLine(blunch->selectedLine);
            blunch->returnToChannelsView();
            editor->state = &blunch->channelsEditorState;
            editor->syncFromState();
            lastCursorLine = editor->getCursorLine();
          }
        } else {
          blunch->lastCancelCount = editor->commands.cancelCount;
          blunch->lastOpenCount = editor->commands.openCount;
          blunch->lastStopCount = editor->commands.stopCount;
          blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
              editor->getCursorLine(), ComputerscareBlunch::MAX_POLY_CHANNELS);
        }
        const BlunchSequencerRuntime& visibleSequencer =
            blunch->showingChannelsView()
                ? blunch->sequencerForChannel(blunch->channelsCursorChannel)
                : blunch->activeSequencer();
        blinkHigh = visibleSequencer.activeClockOutputHigh;
        activeProgress = visibleSequencer.activeClockRamp;
      } else {
        editor->openOnEnter = false;
        editor->stopShortcutEnabled = false;
        editor->readOnly = false;
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
      bool showingSequenceView = !blunch || blunch->showingSequenceView();
      bool showingPendingActiveLine =
          blunch && showingSequenceView &&
          blunch->pendingLine == blunch->activeSequencer().activeLine &&
          blunch->viewingPendingLine;
      bool showingActiveVersionOfPending =
          blunch && showingSequenceView &&
          blunch->pendingLine == blunch->activeSequencer().activeLine &&
          !blunch->viewingPendingLine;
      bool showingInvalidActiveLine =
          blunch && showingSequenceView &&
          blunch->errorLine == blunch->activeSequencer().activeLine;
      if (blunch && showingSequenceView && blunch->pendingLine >= 0 &&
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
      if (blunch && showingSequenceView && blunch->errorLine >= 0) {
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
      if (blunch && blunch->showingChannelsView()) {
        for (int channel = 0; channel < ComputerscareBlunch::MAX_POLY_CHANNELS;
             channel++) {
          const BlunchSequencerRuntime& channelSequencer =
              blunch->sequencerForChannel(channel);
          BlunchLineInfo channelLineInfo = getLineInfo(state->text, channel);
          BlunchLineInfo sourceLineInfo =
              getLineInfo(blunch->sequenceEditorStates[channel].text,
                          channelSequencer.activeLine);
          addBlunchSequencerHighlights(state->highlights, channelSequencer,
                                       sourceLineInfo, channelLineInfo, true);
        }
      } else if (blunch) {
        BlunchLineInfo sourceLineInfo =
            getLineInfo(blunch->sequenceEditorState().text,
                        blunch->activeSequencer().activeLine);
        if (!showingPendingActiveLine && !showingInvalidActiveLine) {
          addBlunchSequencerHighlights(state->highlights,
                                       blunch->activeSequencer(),
                                       sourceLineInfo, sourceLineInfo, true);
        }
      } else {
        ComputerscareTextHighlight activeHighlight;
        activeHighlight.begin = lineInfo.begin;
        activeHighlight.end = lineInfo.end;
        activeHighlight.hasBackground = blinkHigh;
        activeHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, 0x66);
        activeHighlight.hasBorder = true;
        activeHighlight.border = nvgRGBA(0xff, 0xee, 0x9a, 0xdd);
        activeHighlight.hasProgress = showingSequenceView;
        activeHighlight.progress = activeProgress;
        activeHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
        if (activeHighlight.begin < activeHighlight.end) {
          state->highlights.push_back(activeHighlight);
        }
      }
    }
  }

  void onHoverKey(const HoverKeyEvent& e) override {
    bool isPeriod = e.key == GLFW_KEY_PERIOD || e.keyName == ".";
    if (isPeriod && (e.mods & RACK_MOD_MASK) == GLFW_MOD_CONTROL) {
      if (e.action == GLFW_PRESS) {
        ComputerscareBlunch* blunch =
            dynamic_cast<ComputerscareBlunch*>(module);
        if (blunch) {
          blunch->params[ComputerscareBlunch::RUN_PARAM].setValue(0.f);
        }
      }
      e.consume(this);
      return;
    }
    ModuleWidget::onHoverKey(e);
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
