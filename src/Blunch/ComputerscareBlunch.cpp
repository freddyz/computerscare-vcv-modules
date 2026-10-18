#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../BlunchLanguage/BlunchLanguage.hpp"
#include "../Computerscare.hpp"
#include "../ComputerscarePolyModule.hpp"
#include "../ComputerscareResizableHandle.hpp"
#include "../ComputerscareTextEditor.hpp"
#include "../ComputerscareTextEditorLayout.hpp"
#include "../ModifierMouseControl.hpp"
#include "../Phlooper/Block.hpp"
#include "../Phlooper/Dropdown.hpp"
#include "BlunchEditorViews.hpp"
#include "BlunchKeyboardShortcuts.hpp"
#include "BlunchProgramCompiler.hpp"
#include "BlunchRandomConfig.hpp"
#include "BlunchRandomProgram.hpp"
#include "BlunchSequencerEngine.hpp"
#include "BlunchSequencerRuntime.hpp"
#include "ClockPhase.hpp"
#include "ControlCv.hpp"

struct BlunchLineInfo {
  int begin = 0;
  int end = 0;
  std::string text;
};

struct BlunchAllViewTextPosition {
  int line = 0;
  int column = 0;
};

static thread_local int blunchProcessingChannel = -1;
static const float EXTERNAL_TIMING_DISPLAY_PULSE_SECONDS = 0.14f;
static const int BLUNCH_WAIT_MODE_COUNT = 9;
static const std::string BLUNCH_WAIT_MODE_NAMES[BLUNCH_WAIT_MODE_COUNT] = {
    "Immediate",  "Clock",      "Block",      "Line",      "Page",
    "External W", "External X", "External Y", "External Z"};
static const std::string BLUNCH_WAIT_MODE_DESCRIPTIONS[BLUNCH_WAIT_MODE_COUNT] =
    {"Activate submitted text immediately.",
     "Wait for the next clock pulse.",
     "Wait for the current block to finish.",
     "Wait for the current line to finish.",
     "Wait until the sequence wraps to the first line.",
     "Wait for external clock W.",
     "Wait for external clock X.",
     "Wait for external clock Y.",
     "Wait for external clock Z."};
static const int BLUNCH_ADVANCE_MODE_COUNT = 6;
static const std::string BLUNCH_ADVANCE_MODE_NAMES[BLUNCH_ADVANCE_MODE_COUNT] =
    {"Off",        "Automatic",  "External W",
     "External X", "External Y", "External Z"};
static const std::string
    BLUNCH_LINE_ADVANCE_MODE_DESCRIPTIONS[BLUNCH_ADVANCE_MODE_COUNT] = {
        "Do not advance lines automatically.",
        "Advance to the next line when the current line finishes.",
        "Advance to the next line on external clock W.",
        "Advance to the next line on external clock X.",
        "Advance to the next line on external clock Y.",
        "Advance to the next line on external clock Z."};
static const std::string
    BLUNCH_BLOCK_ADVANCE_MODE_DESCRIPTIONS[BLUNCH_ADVANCE_MODE_COUNT] = {
        "Repeat the current block until advanced manually.",
        "Advance to the next block when the current block finishes.",
        "Advance to the next block on external clock W.",
        "Advance to the next block on external clock X.",
        "Advance to the next block on external clock Y.",
        "Advance to the next block on external clock Z."};
static const int BLUNCH_RUN_INPUT_MODE_COUNT = 2;
static const std::string
    BLUNCH_RUN_INPUT_MODE_NAMES[BLUNCH_RUN_INPUT_MODE_COUNT] = {
        "Gate", "Trigger toggle"};

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

static BlunchAllViewTextPosition getAllViewTextPositionForOffset(
    const std::string& text, int offset) {
  BlunchAllViewTextPosition position;
  int clampedOffset = std::max(0, std::min(offset, (int)text.size()));
  for (int i = 0; i < clampedOffset; i++) {
    if (text[i] == '\n') {
      position.line++;
      position.column = 0;
    } else {
      position.column++;
    }
  }
  return position;
}

static int getOffsetForAllViewTextPosition(const std::string& text,
                                           BlunchAllViewTextPosition position) {
  BlunchLineInfo lineInfo = getLineInfo(text, position.line);
  return std::min(lineInfo.begin + std::max(0, position.column), lineInfo.end);
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

static void addBlunchProgressHighlight(
    std::vector<ComputerscareTextHighlight>& highlights,
    const blunch::sequencer::RepeatProgress& progress,
    const BlunchLineInfo& sourceLine, const BlunchLineInfo& visibleLine) {
  ComputerscareTextHighlight highlight;
  highlight.begin = mapBlunchSourceOffsetToVisibleLine(sourceLine, visibleLine,
                                                       progress.begin);
  highlight.end =
      std::max(highlight.begin, mapBlunchSourceOffsetToVisibleLine(
                                    sourceLine, visibleLine, progress.end));
  highlight.hasBackground = false;
  highlight.hasProgress = true;
  highlight.progress = progress.progress;
  highlight.progressSegments = progress.segments;
  highlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
  if (highlight.begin < highlight.end) {
    highlights.push_back(highlight);
  }
}

static void addBlunchSequencerHighlights(
    std::vector<ComputerscareTextHighlight>& highlights,
    const BlunchSequencerRuntime& seq, const BlunchLineInfo& sourceLine,
    const BlunchLineInfo& visibleLine, bool includeClockProgress,
    int displayStyle, bool showBars) {
  ComputerscareTextHighlight tokenHighlight;
  tokenHighlight.begin = mapBlunchSourceOffsetToVisibleLine(
      sourceLine, visibleLine, seq.activeHighlightBegin);
  tokenHighlight.end =
      std::max(tokenHighlight.begin,
               mapBlunchSourceOffsetToVisibleLine(sourceLine, visibleLine,
                                                  seq.activeHighlightEnd));
  tokenHighlight.hasBackground =
      displayStyle <= 1 && seq.activeClockDisplayPulse > 0.001f;
  unsigned char pulseAlpha = (unsigned char)std::min(
      0xbb, 0x30 + (int)(seq.activeClockDisplayPulse * 0x75));
  tokenHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, pulseAlpha);
  tokenHighlight.hasBorder =
      displayStyle == 0 ||
      (displayStyle == 4 && seq.activeClockDisplayPulse > .001f);
  tokenHighlight.borderWidth = .8f;
  tokenHighlight.borderDistortion = .75f;
  tokenHighlight.backgroundDistortion =
      (displayStyle <= 1 || displayStyle == 4) ? 2.f : 0.f;
  tokenHighlight.border = nvgRGBA(
      0xff, 0xee, 0x9a,
      displayStyle == 4
          ? (unsigned char)(221.f *
                            std::max(
                                0.f,
                                std::min(1.f, seq.activeClockDisplayPulse)))
          : 0xdd);
  tokenHighlight.hasProgress =
      showBars && includeClockProgress && seq.running &&
      !blunch::sequencer::activeStepUsesExternalClock(seq);
  if (displayStyle == 2) {
    tokenHighlight.hasBackground = false;
    tokenHighlight.hasForeground = true;
    // Share the box display's trigger envelope and decay timing.
    float flash = std::max(0.f, std::min(1.f, seq.activeClockDisplayPulse));
    tokenHighlight.foreground =
        nvgLerpRGBA(nvgRGB(0xff, 0xee, 0x9a), COLOR_COMPUTERSCARE_GREEN, flash);
  }
  tokenHighlight.progress = seq.activeClockRamp;
  tokenHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
  if (tokenHighlight.begin < tokenHighlight.end) {
    highlights.push_back(tokenHighlight);
  }

  if (!seq.activeProgram.empty() && seq.activeProgramIndex >= 0 &&
      seq.activeProgramIndex < (int)seq.activeProgram.size() &&
      displayStyle != 3 && seq.activeExternalTimingDisplayPulse > 0.001f) {
    const BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    int externalTimingBegin = 0;
    int externalTimingEnd = 0;
    if (step.hasTotalDurationGroup && step.totalDurationIsTickCount &&
        step.totalDurationExternalClockInput >= 0) {
      externalTimingBegin = step.totalDurationHighlightBegin;
      externalTimingEnd = step.totalDurationHighlightEnd;
    } else if (!step.hasDuration && step.repeatExternalClockInput >= 0) {
      externalTimingBegin = step.repeatHighlightBegin;
      externalTimingEnd = step.repeatHighlightEnd;
    }

    if (externalTimingEnd > externalTimingBegin) {
      ComputerscareTextHighlight externalTimingHighlight;
      externalTimingHighlight.begin = mapBlunchSourceOffsetToVisibleLine(
          sourceLine, visibleLine, externalTimingBegin);
      externalTimingHighlight.end =
          std::max(externalTimingHighlight.begin,
                   mapBlunchSourceOffsetToVisibleLine(sourceLine, visibleLine,
                                                      externalTimingEnd));
      externalTimingHighlight.hasBackground = displayStyle <= 1;
      externalTimingHighlight.hasBorder = displayStyle == 4;
      externalTimingHighlight.borderWidth = .8f;
      externalTimingHighlight.backgroundDistortion =
          (displayStyle <= 1 || displayStyle == 4) ? 2.f : 0.f;
      externalTimingHighlight.hasForeground = displayStyle == 2;
      externalTimingHighlight.foreground = COLOR_COMPUTERSCARE_GREEN;
      float externalTimingFade =
          std::max(0.f, std::min(seq.activeExternalTimingDisplayPulse /
                                     EXTERNAL_TIMING_DISPLAY_PULSE_SECONDS,
                                 1.f));
      unsigned char externalTimingAlpha =
          (unsigned char)(0xbb * externalTimingFade);
      externalTimingHighlight.background =
          nvgRGBA(0xc8, 0x9f, 0x16, externalTimingAlpha);
      externalTimingHighlight.border = nvgRGBA(
          0xff, 0xee, 0x9a, (unsigned char)(221.f * externalTimingFade));
      if (externalTimingHighlight.begin < externalTimingHighlight.end) {
        highlights.push_back(externalTimingHighlight);
      }
    }
  }

  if (!showBars) return;

  std::vector<blunch::sequencer::TimingScopeProgress> scopeProgress =
      blunch::sequencer::getActiveTimingScopeProgressHighlights(seq);
  for (size_t i = 0; i < scopeProgress.size(); i++) {
    addBlunchProgressHighlight(highlights, scopeProgress[i], sourceLine,
                               visibleLine);
  }
}

static const blunch::language::RandomChoiceAst& chooseRandomChoice(
    const blunch::language::ClockLiteralAst& ast) {
  size_t choiceIndex =
      std::min((size_t)(random::uniform() * ast.randomChoices.size()),
               ast.randomChoices.size() - 1);
  return ast.randomChoices[choiceIndex];
}

static const blunch::language::RandomChoiceAst& chooseRandomChoice(
    const blunch::language::ClockLiteralAst& ast, float randomValue) {
  size_t choiceIndex =
      std::min((size_t)(randomValue * ast.randomChoices.size()),
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

static double sampleRandomChoiceValue(
    const blunch::language::RandomChoiceAst& choice, float randomValue) {
  double lowValue = std::min(choice.minValue, choice.maxValue);
  double highValue = std::max(choice.minValue, choice.maxValue);
  if (highValue <= lowValue) {
    return lowValue;
  }
  return lowValue + randomValue * (highValue - lowValue);
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

static int sampleRandomChoiceInteger(
    const blunch::language::RandomChoiceAst& choice, float randomValue) {
  int lowValue = (int)std::min(choice.minValue, choice.maxValue);
  int highValue = (int)std::max(choice.minValue, choice.maxValue);
  if (highValue <= lowValue) {
    return lowValue;
  }
  return lowValue +
         (int)std::floor(randomValue * (float)(highValue - lowValue + 1));
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

struct BlunchWaitModeParamQuantity : SwitchQuantity {
  std::string getDisplayValueString() override {
    int mode = std::max(
        0, std::min((int)std::round(getValue()), BLUNCH_WAIT_MODE_COUNT - 1));
    return BLUNCH_WAIT_MODE_NAMES[mode];
  }

  std::string getString() override {
    return getLabel() + "\n" + getDisplayValueString();
  }

  std::string getDescription() override {
    int mode = std::max(
        0, std::min((int)std::round(getValue()), BLUNCH_WAIT_MODE_COUNT - 1));
    return BLUNCH_WAIT_MODE_DESCRIPTIONS[mode];
  }
};

struct BlunchAdvanceModeParamQuantity : SwitchQuantity {
  std::string getDisplayValueString() override {
    int mode = std::max(0, std::min((int)std::round(getValue()),
                                    BLUNCH_ADVANCE_MODE_COUNT - 1));
    return BLUNCH_ADVANCE_MODE_NAMES[mode];
  }

  std::string getString() override {
    return getLabel() + "\n" + getDisplayValueString();
  }

  std::string getDescription() override {
    int mode = std::max(0, std::min((int)std::round(getValue()),
                                    BLUNCH_ADVANCE_MODE_COUNT - 1));
    if (getLabel() == "Block advance") {
      return BLUNCH_BLOCK_ADVANCE_MODE_DESCRIPTIONS[mode];
    }
    return BLUNCH_LINE_ADVANCE_MODE_DESCRIPTIONS[mode];
  }
};

struct ComputerscareBlunch : ComputerscarePolyModule {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;
  static constexpr float MIN_WIDTH = 240.f;
  static constexpr int MAX_POLY_CHANNELS = 16;
  enum SeededRandomRole {
    SEEDED_RANDOM_CLOCK = 1,
    SEEDED_RANDOM_PROBABILITY = 2,
    SEEDED_RANDOM_REPEAT = 3,
    SEEDED_RANDOM_TOTAL_DURATION = 4
  };

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
    SEED_PARAM,
    WAIT_MODE_PARAM,
    EDITOR_LINE_HEIGHT_PARAM,
    NUM_PARAMS
  };
  enum InputIds {
    EXTERNAL_CLOCK_W_INPUT,
    EXTERNAL_CLOCK_X_INPUT,
    EXTERNAL_CLOCK_Y_INPUT,
    EXTERNAL_CLOCK_Z_INPUT,
    ADVANCE_TOKEN_INPUT,
    ADVANCE_INPUT = ADVANCE_TOKEN_INPUT,
    ADVANCE_LINE_INPUT,
    RESET_BLOCK_INPUT,
    RESET_INPUT = RESET_BLOCK_INPUT,
    RESET_LINE_INPUT,
    RESET_SEQUENCE_INPUT,
    RUN_INPUT,
    TIME_CV_INPUT,
    SEED_CV_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    CLOCK_OUTPUT,
    EOC1_OUTPUT,
    EOC2_OUTPUT,
    EOC3_OUTPUT,
    PHASE_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds { SYNTAX_ERROR_LIGHT, NUM_LIGHTS };

  ComputerscareTextEditorState sequenceEditorStates[MAX_POLY_CHANNELS];
  ComputerscareTextEditorState channelsEditorState;
  dsp::PulseGenerator tokenMovePulses[MAX_POLY_CHANNELS];
  dsp::PulseGenerator lineCyclePulses[MAX_POLY_CHANNELS];
  dsp::PulseGenerator wrapPulses[MAX_POLY_CHANNELS];
  dsp::PulseGenerator clockOutputPulses[MAX_POLY_CHANNELS];
  dsp::SchmittTrigger externalClockTriggers[4];
  computerscare::blunch::ClockPhase externalClockPhases[4];
  std::atomic<unsigned> manualResets{0};
  dsp::SchmittTrigger resetBlockTrigger;
  dsp::SchmittTrigger resetLineTrigger;
  dsp::SchmittTrigger resetSequenceTrigger;
  dsp::SchmittTrigger runInputTriggers[MAX_POLY_CHANNELS];
  dsp::Timer resetClockIgnoreTimer;
  BlunchSequencerRuntime sequencers[MAX_POLY_CHANNELS];
  bool syntaxError = false;
  int selectedLine = 0;
  bool selectedLineDirty = false;
  bool randomizeCursorRestorePending = false;
  int randomizeCursorRestoreLine = 0;
  bool randomizeCursorRestoreAtEnd = false;
  struct PendingFormula {
    int line = -1;
    std::string text;
    std::vector<BlunchProgramStep> program;
    bool viewing = true;
    bool armed = false;
  };
  PendingFormula pendingFormulas[MAX_POLY_CHANNELS];
  std::map<int, std::string> runningLineTexts[MAX_POLY_CHANNELS];
  PendingFormula& pendingEdit() {
    return pendingFormulas[activeContextChannel()];
  }
  bool channelHasPendingFormula(int channel) {
    channel = clampChannel(channel);
    for (const auto& line : runningLineTexts[channel])
      if (getLineInfo(sequenceEditorStates[channel].text, line.first).text !=
          line.second)
        return true;
    const auto& pending = pendingFormulas[channel];
    return pending.line >= 0 &&
           pending.text != sequencers[channel].activeLineText;
  }
  int errorLine = -1;
  int errorLineRevertLine = -1;
  std::string errorLineRevertText;
  std::string checkedFocusedLineText;
  int lastSubmitCount = 0;
  int lastCancelCount = 0;
  int lastOpenCount = 0;
  int lastStopCount = 0;
  int lastHardStopCount = 0;
  int lastRunToggleCount = 0;
  int lastStartAllCount = 0;
  bool startAllRequested = false;
  bool controlMode = false;
  computerscare::blunch::ControlSelection controlSelection;
  std::atomic<unsigned> controlToggleRequests{0}, controlResetRequests{0},
      controlStopRequests{0};
  int lastSwitchViewCount = 0;
  int lastNavigateChannelCount = 0;
  int lastNavigateChannelForwardCount = 0;
  int lastNavigateChannelBackwardCount = 0;
  BlunchEditorViewMode editorViewMode = BlunchEditorViewMode::Sequence;
  int focusedChannel = 0;
  int playbackChannel = 0;
  int channelsCursorChannel = 0;
  int forcedChannelsCursorChannel = -1;
  bool editorViewDirty = true;
  bool lastRunHigh = true;
  bool rememberedRunning[MAX_POLY_CHANNELS] = {
      true, true, true, true, true, true, true, true,
      true, true, true, true, true, true, true, true};
  bool runInputLatched[MAX_POLY_CHANNELS] = {true, true, true, true, true, true,
                                             true, true, true, true, true, true,
                                             true, true, true, true};
  bool resetClockIgnoreActive = false;
  bool externalClockHigh[4] = {false, false, false, false};
  float width = MIN_WIDTH;
  bool editorLineWrapping = true;
  bool editorFitLines = true;
  int runInputMode = 0;
  blunch::language::ClockUnit defaultNumberUnit =
      blunch::language::ClockUnit::Bpm;
  int playbackDisplayStyle = 1;
  bool playbackBars = true;

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

  float seedValue() {
    return computerscare::blunch::seedWithCv(
        params[SEED_PARAM].getValue(), inputs[SEED_CV_INPUT].getVoltage());
  }

  bool seededRandomEnabled() {
    return computerscare::blunch::usesSeed(seedValue());
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
    return (int)std::round(params[AUTO_ADVANCE_PARAM].getValue()) == 1;
  }

  bool channelPlaying(int channel) {
    channel = clampChannel(channel);
    return channel < selectedOutputChannelCount() &&
           params[RUN_PARAM].getValue() > .5f &&
           sequencerForChannel(channel).running && channelHasFormula(channel) &&
           runInputAllowsChannel(runInputChannelCount(), channel);
  }
  bool channelsEditorEditingEnabled(int channel = -1) {
    channel = clampChannel(channel < 0 ? channelsCursorChannel : channel);
    return blunchChannelsRowEditable(channelPlaying(channel),
                                     sequenceEditorStates[channel].text);
  }
  int channelOverviewLine(int channel) {
    return blunchChannelsSourceLine(sequenceEditorStates[channel].text,
                                    sequencerForChannel(channel).activeLine,
                                    sequenceEditorStates[channel].cursor,
                                    channelPlaying(channel));
  }

  enum WaitMode {
    WAIT_IMMEDIATE,
    WAIT_CLOCK,
    WAIT_BLOCK,
    WAIT_LINE,
    WAIT_PAGE,
    WAIT_EXTERNAL_W,
    WAIT_EXTERNAL_X,
    WAIT_EXTERNAL_Y,
    WAIT_EXTERNAL_Z
  };

  enum WaitBoundary {
    WAIT_BOUNDARY_CLOCK,
    WAIT_BOUNDARY_BLOCK,
    WAIT_BOUNDARY_LINE,
    WAIT_BOUNDARY_PAGE
  };

  WaitMode waitMode() {
    return (WaitMode)std::max(
        0, std::min((int)std::round(params[WAIT_MODE_PARAM].getValue()),
                    BLUNCH_WAIT_MODE_COUNT - 1));
  }

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
    BlunchAllViewTextPosition cursorPosition;
    BlunchAllViewTextPosition selectionPosition;
    if (preserveCursor) {
      cursorPosition = getAllViewTextPositionForOffset(
          channelsEditorState.text, channelsEditorState.cursor);
      selectionPosition = getAllViewTextPositionForOffset(
          channelsEditorState.text, channelsEditorState.selection);
      if (cursorPosition.line != selectionPosition.line) {
        selectionPosition.line = cursorPosition.line;
        selectionPosition.column = cursorPosition.column;
      }
    }

    const std::string* sourceTexts[MAX_POLY_CHANNELS];
    bool playing[MAX_POLY_CHANNELS];
    int cursors[MAX_POLY_CHANNELS];
    for (int i = 0; i < MAX_POLY_CHANNELS; ++i) {
      sourceTexts[i] = &sequenceEditorStates[i].text;
      playing[i] = channelPlaying(i);
      cursors[i] = sequenceEditorStates[i].cursor;
    }
    std::string nextText = buildBlunchChannelsViewText(
        sequencers, MAX_POLY_CHANNELS, sourceTexts, playing, cursors);
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
      channelsEditorState.cursor = getOffsetForAllViewTextPosition(
          channelsEditorState.text, cursorPosition);
      channelsEditorState.selection = getOffsetForAllViewTextPosition(
          channelsEditorState.text, selectionPosition);
    } else {
      channelsEditorState.cursor = channelLineInfo.begin;
      channelsEditorState.selection = channelsEditorState.cursor;
    }
    if (!preserveCursor || textChanged) {
      editorViewDirty = true;
    }
  }

  void switchEditorView() {
    if (showingChannelsView()) {
      openFocusedChannelEditPage();
      return;
    }
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
      clearSyntaxError();
      checkedFocusedLineText.clear();
      randomizeCursorRestorePending = false;
    }
    selectedLine = channelOverviewLine(focusedChannel);
    selectedLineDirty = true;
    editorViewMode = BlunchEditorViewMode::Sequence;
    editorViewDirty = true;
  }

  void navigateFocusedChannel(int direction) {
    int previousFocusedChannel = focusedChannel;
    focusedChannel =
        (focusedChannel + direction + MAX_POLY_CHANNELS) % MAX_POLY_CHANNELS;
    if (focusedChannel != previousFocusedChannel) {
      clearSyntaxError();
      checkedFocusedLineText.clear();
      randomizeCursorRestorePending = false;
    }
    selectedLine = channelOverviewLine(focusedChannel);
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

  void setControlMode(bool enabled) {
    if (controlMode == enabled) return;
    if (showingChannelsView()) syncChannelsEditorEdit(channelsCursorChannel);
    controlMode = enabled;
    if (enabled) {
      controlSelection.selectOnly(showingChannelsView() ? channelsCursorChannel
                                                        : focusedChannel);
      editorViewMode = BlunchEditorViewMode::Channels;
      focusControlRow();
    }
  }

  void focusControlRow() {
    channelsCursorChannel = controlSelection.focus;
    forcedChannelsCursorChannel = channelsCursorChannel;
    refreshChannelsEditorState();
    channelsEditorState.dirty = true;
  }

  bool handleControlKey(int key, int mods, int action) {
    if (!controlMode) return false;
    using namespace computerscare::blunch;
    bool navigation = key == GLFW_KEY_UP || key == GLFW_KEY_DOWN ||
                      key == GLFW_KEY_HOME || key == GLFW_KEY_END;
    if (navigation && !showingChannelsView()) return false;
    auto command = handleControlShortcut(key, mods, action, controlSelection);
    if (command == ControlCommand::Unhandled) return false;
    if (command == ControlCommand::Navigate ||
        command == ControlCommand::ToggleSelection) {
      if (showingChannelsView()) focusControlRow();
    } else if (command == ControlCommand::TogglePlayback) {
      controlToggleRequests.fetch_or(controlSelection.targets());
    } else if (command == ControlCommand::Reset) {
      controlResetRequests.fetch_or(controlSelection.targets());
    }
    return true;
  }

  void processControlRequests() {
    unsigned toggle = controlToggleRequests.exchange(0);
    unsigned reset = controlResetRequests.exchange(0);
    unsigned stop = controlStopRequests.exchange(0);
    if (!(toggle | reset | stop)) return;
    bool allRunning = params[RUN_PARAM].getValue() > .5f;
    for (int ch = 0; ch < MAX_POLY_CHANNELS; ++ch)
      if (toggle & (1u << ch))
        allRunning &=
            sequencers[ch].running && !sequencers[ch].activeProgram.empty();
    if (toggle && !allRunning && params[RUN_PARAM].getValue() <= .5f) {
      // Starting a selection from master stop must not restart other channels.
      for (int ch = 0; ch < MAX_POLY_CHANNELS; ++ch) stopSequencer(ch, true);
    }
    int previous = blunchProcessingChannel;
    for (int ch = 0; ch < MAX_POLY_CHANNELS; ++ch) {
      unsigned bit = 1u << ch;
      if (!((toggle | reset | stop) & bit)) continue;
      blunchProcessingChannel = ch;
      if (toggle & bit) {
        if (allRunning)
          stopSequencer(ch, true);
        else {
          if (sequencers[ch].activeProgram.empty()) {
            resetActiveSequence(true);
          } else {
            // Stop clears the current highlight and the scheduled start tick.
            // Restore both before running, just as submitting a line does.
            applyActiveProgramStep(sequencers[ch]);
          }
          if (!sequencers[ch].activeProgram.empty()) startSequencer(ch);
        }
      }
      if (reset & bit) {
        bool running = sequencers[ch].running;
        resetActiveSequence(true);
        sequencers[ch].running = running;
        clockOutputPulses[ch].reset();
        sequencers[ch].activeClockOutputHigh = false;
        sequencers[ch].activeClockDisplayPulse = 0.f;
      }
      if (stop & bit) stopSequencer(ch, true);
    }
    blunchProcessingChannel = previous;
  }

  void stopSequencer(int channel, bool rememberStopped = false) {
    channel = clampChannel(channel);
    BlunchSequencerRuntime& seq = sequencerForChannel(channel);
    seq.stopPlayback();
    if (rememberStopped) {
      rememberedRunning[channel] = false;
    }
    clockOutputPulses[channel].reset();
  }

  void stopAllSequencersRememberingRunning() {
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      rememberedRunning[channel] = sequencers[channel].running;
      stopSequencer(channel);
    }
  }

  void restartRememberedSequencers() {
    int previousProcessingChannel = blunchProcessingChannel;
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      blunchProcessingChannel = channel;
      BlunchSequencerRuntime& channelSequencer = sequencers[channel];
      channelSequencer.running = rememberedRunning[channel];
      if (channelSequencer.running && !channelSequencer.activeProgram.empty()) {
        if (seededRandomEnabled()) {
          resetSeededRandomSerials(channelSequencer);
        }
        applyActiveProgramStep(channelSequencer);
      }
    }
    blunchProcessingChannel = previousProcessingChannel;
  }

  void startSequencer(int channel) {
    channel = std::max(0, std::min(channel, MAX_POLY_CHANNELS - 1));
    playbackChannel = channel;
    rememberedRunning[channel] = true;
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

  int advanceModeExternalClock(int paramId) {
    int mode = std::max(0, std::min((int)std::round(params[paramId].getValue()),
                                    BLUNCH_ADVANCE_MODE_COUNT - 1));
    if (mode < 2) {
      return -1;
    }
    return mode - 2;
  }

  void toggleRunParam() {
    params[RUN_PARAM].setValue(params[RUN_PARAM].getValue() > 0.5f ? 0.f : 1.f);
  }

  int runInputChannelCount() {
    return inputs[RUN_INPUT].isConnected() ? inputs[RUN_INPUT].getChannels()
                                           : 0;
  }

  bool runInputControlsChannel(int inputChannels, int channel) const {
    return inputChannels == 1 || channel < inputChannels;
  }

  int runInputChannelForOutput(int inputChannels, int channel) const {
    return inputChannels == 1 ? 0 : channel;
  }

  void updateRunInputState(bool masterRunHigh) {
    int inputChannels = runInputChannelCount();
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      if (!runInputControlsChannel(inputChannels, channel)) {
        continue;
      }
      int inputChannel = runInputChannelForOutput(inputChannels, channel);
      bool triggered = runInputTriggers[channel].process(
          inputs[RUN_INPUT].getVoltage(inputChannel));
      if (masterRunHigh && runInputMode == 1 && triggered) {
        runInputLatched[channel] = !runInputLatched[channel];
      }
    }
  }

  bool runInputAllowsChannel(int inputChannels, int channel) {
    if (inputChannels == 0) {
      return true;
    }
    if (!runInputControlsChannel(inputChannels, channel)) {
      return false;
    }
    int inputChannel = runInputChannelForOutput(inputChannels, channel);
    if (runInputMode == 1) {
      return runInputLatched[channel];
    }
    return inputs[RUN_INPUT].getVoltage(inputChannel) > 1.f;
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
    polyChannels = 1;
    configParam<BlunchSpeedOfTimeParamQuantity>(SPEED_OF_TIME_PARAM, -5.f, 5.f,
                                                0.f, "Speed of time");
    configSwitch(POLY_CHANNELS_PARAM, 0.f, 16.f, 0.f, "Poly Channels",
                 polyChannelLabels(true));
    configSwitch<BlunchAdvanceModeParamQuantity>(
        AUTO_ADVANCE_PARAM, 0.f, BLUNCH_ADVANCE_MODE_COUNT - 1, 1.f,
        "Line advance",
        {"Off", "Automatic", "External W", "External X", "External Y",
         "External Z"});
    configSwitch<BlunchAdvanceModeParamQuantity>(
        AUTO_BLOCK_ADVANCE_PARAM, 0.f, BLUNCH_ADVANCE_MODE_COUNT - 1, 1.f,
        "Block advance",
        {"Off", "Automatic", "External W", "External X", "External Y",
         "External Z"});
    configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
    configParam(SEED_PARAM, 0.f, 10.f, 0.f, "Seed");
    configSwitch<BlunchWaitModeParamQuantity>(
        WAIT_MODE_PARAM, 0.f, BLUNCH_WAIT_MODE_COUNT - 1, 0.f, "Wait mode",
        {"Immediate", "Clock", "Block", "Line", "Page", "External W",
         "External X", "External Y", "External Z"});
    configParam(EDITOR_LINE_HEIGHT_PARAM, -.5f, 1.f, 0.f, "Editor line height",
                "%", 0.f, 100.f);
    configParam(EDITOR_FONT_SIZE_PARAM, 8.f, 24.f, 14.f, "Editor font size");
    configParam(EDITOR_FONT_WIDTH_PARAM, -0.35f, 0.75f, 0.f,
                "Editor font width");
    configParam(EDITOR_FONT_HEIGHT_PARAM, -0.35f - BLUNCH_FONT_HEIGHT_BASE,
                0.75f - BLUNCH_FONT_HEIGHT_BASE, 0.f, "Editor font height");
    configParam(EDITOR_LETTER_SPACING_PARAM, -2.f, 4.f, 0.f,
                "Editor letter spacing");
    getParamQuantity(SPEED_OF_TIME_PARAM)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS_PARAM)->randomizeEnabled = false;
    getParamQuantity(AUTO_ADVANCE_PARAM)->randomizeEnabled = false;
    getParamQuantity(AUTO_BLOCK_ADVANCE_PARAM)->randomizeEnabled = false;
    getParamQuantity(RUN_PARAM)->randomizeEnabled = false;
    getParamQuantity(SEED_PARAM)->randomizeEnabled = false;
    getParamQuantity(WAIT_MODE_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_LINE_HEIGHT_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_FONT_SIZE_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_FONT_WIDTH_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_FONT_HEIGHT_PARAM)->randomizeEnabled = false;
    getParamQuantity(EDITOR_LETTER_SPACING_PARAM)->randomizeEnabled = false;
    configInput(ADVANCE_TOKEN_INPUT, "Legacy advance token");
    configInput(ADVANCE_LINE_INPUT, "Legacy advance line");
    configInput(RESET_BLOCK_INPUT, "Reset Block");
    configInput(RESET_LINE_INPUT, "Reset Line");
    configInput(RESET_SEQUENCE_INPUT, "Reset Page");
    configInput(RUN_INPUT, "Run");
    configInput(TIME_CV_INPUT, "Time CV (1 V/oct)");
    configInput(SEED_CV_INPUT, "Seed CV");
    configInput(EXTERNAL_CLOCK_W_INPUT, "External clock W");
    configInput(EXTERNAL_CLOCK_X_INPUT, "External clock X");
    configInput(EXTERNAL_CLOCK_Y_INPUT, "External clock Y");
    configInput(EXTERNAL_CLOCK_Z_INPUT, "External clock Z");
    configOutput(PHASE_OUTPUT, "Clock phase (0–10 V, polyphonic)");
    configOutput(CLOCK_OUTPUT, "Clock");
    configOutput(EOC1_OUTPUT, "End of Block");
    configOutput(EOC2_OUTPUT, "End of Line");
    configOutput(EOC3_OUTPUT, "End of Page");
    for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
      sequencers[channel].activeClockSpec.bpm = CLOCK_BPM;
      sequencers[channel].activeClockSpec.hz = CLOCK_HZ;
      sequencers[channel].activeClockSpec.periodSeconds = 1.f / CLOCK_HZ;
    }
    sequenceEditorStates[0].text = generateRandomProgram(true);
    if (sequenceEditorStates[0].text.empty())
      sequenceEditorStates[0].text = "120bpm";
    commitLine(0, true);
  }

  void processSequencerChannel(int channel, float scaledSampleTime,
                               const bool externalClockEdges[4],
                               bool channelRunHigh, bool resetBlockPressed,
                               bool resetLinePressed, bool resetSequencePressed,
                               bool advanceLinePressed,
                               bool advanceTokenPressed) {
    channel = clampChannel(channel);
    int previousProcessingChannel = blunchProcessingChannel;
    blunchProcessingChannel = channel;
    BlunchSequencerRuntime& seq = activeSequencer();
    if (seq.activeProgram.empty()) {
      seq.clockHigh = false;
      seq.activeClockOutputHigh = false;
      seq.activeClockDisplayPulse = 0.f;
      seq.activeExternalTimingDisplayPulse = 0.f;
      seq.clockStartLowSamples = 0;
      seq.clockStartHighPending = false;
      clockOutputPulses[channel].reset();
      blunchProcessingChannel = previousProcessingChannel;
      return;
    }
    int activeExternalClock = activeExternalClockInput(seq);
    int activeRepeatClock = activeRepeatExternalClockInput(seq);
    int activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
    bool running = channelRunHigh && seq.running;
    bool previousClockHigh = seq.clockHigh;
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
    bool outputClockTriggered = activeExternalClock >= 0
                                    ? externalClockEdges[activeExternalClock]
                                    : (!previousClockHigh && outputClockHigh);
    bool externalTimingClockTriggered =
        (activeRepeatClock >= 0 && externalClockEdges[activeRepeatClock]) ||
        (activeTotalTickClock >= 0 && externalClockEdges[activeTotalTickClock]);
    bool resetPressed =
        resetBlockPressed || resetLinePressed || resetSequencePressed;
    bool pendingActivated = false;
    if (running && !resetPressed) {
      for (int i = 0; i < 4; i++) {
        if (externalClockEdges[i] &&
            activatePendingLineAtExternalClock(i, true)) {
          activeExternalClock = activeExternalClockInput(seq);
          activeRepeatClock = activeRepeatExternalClockInput(seq);
          activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
          pendingActivated = true;
          break;
        }
      }
    }
    if (pendingActivated) {
      outputClockHigh = activeExternalClock >= 0
                            ? externalClockHigh[activeExternalClock]
                            : seq.clockHigh;
      outputClockTriggered = activeExternalClock >= 0
                                 ? externalClockEdges[activeExternalClock]
                                 : (!previousClockHigh && outputClockHigh);
      externalTimingClockTriggered =
          (activeRepeatClock >= 0 && externalClockEdges[activeRepeatClock]) ||
          (activeTotalTickClock >= 0 &&
           externalClockEdges[activeTotalTickClock]);
    }
    if (running && !resetPressed && outputClockTriggered &&
        activatePendingLineAtBoundary(WAIT_BOUNDARY_CLOCK, true)) {
      activeExternalClock = activeExternalClockInput(seq);
      activeRepeatClock = activeRepeatExternalClockInput(seq);
      activeTotalTickClock = activeTotalDurationExternalClockInput(seq);
      outputClockHigh = activeExternalClock >= 0
                            ? externalClockHigh[activeExternalClock]
                            : seq.clockHigh;
      outputClockTriggered = activeExternalClock >= 0
                                 ? externalClockEdges[activeExternalClock]
                                 : (!previousClockHigh && outputClockHigh);
      externalTimingClockTriggered =
          (activeRepeatClock >= 0 && externalClockEdges[activeRepeatClock]) ||
          (activeTotalTickClock >= 0 &&
           externalClockEdges[activeTotalTickClock]);
    }
    if (running && !resetPressed && outputClockTriggered &&
        seq.activeStepPlays) {
      clockOutputPulses[channel].trigger(1e-3f);
    }
    if (running && !resetPressed &&
        ((outputClockTriggered && seq.activeStepPlays) ||
         externalTimingClockTriggered)) {
      seq.activeClockDisplayPulse = 1.f;
    }
    if (running && !resetPressed && externalTimingClockTriggered) {
      seq.activeExternalTimingDisplayPulse =
          EXTERNAL_TIMING_DISPLAY_PULSE_SECONDS;
    }

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

    if (channelRunHigh && resetSequencePressed) {
      resetActiveSequence(true);
    } else if (channelRunHigh && resetLinePressed) {
      resetActiveProgram(true);
    } else if (channelRunHigh && resetBlockPressed) {
      resetActiveBlock(true);
    }
    if (channelRunHigh && advanceLinePressed) {
      moveToNextLine(false);
    }
    if (channelRunHigh && advanceTokenPressed) {
      advanceActiveProgramStep(seq, true);
    }

    blunchProcessingChannel = previousProcessingChannel;
  }

  void process(const ProcessArgs& args) override {
    processControlRequests();
    float timeScale = getTimeScale();
    float scaledSampleTime = args.sampleTime * timeScale;
    bool externalClockEdges[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
      float voltage = inputs[EXTERNAL_CLOCK_W_INPUT + i].getVoltage();
      bool wasHigh = externalClockHigh[i];
      externalClockTriggers[i].process(voltage);
      externalClockHigh[i] = externalClockTriggers[i].isHigh();
      externalClockEdges[i] = externalClockHigh[i] && !wasHigh;
      externalClockPhases[i].process(args.sampleTime, externalClockEdges[i]);
    }

    bool runHigh = params[RUN_PARAM].getValue() > 0.5f;
    bool startAllNow = startAllRequested;
    startAllRequested = false;
    if (startAllNow) {
      params[RUN_PARAM].setValue(1.f);
      runHigh = true;
      int previousProcessingChannel = blunchProcessingChannel;
      for (int channel = 0; channel < MAX_POLY_CHANNELS; channel++) {
        blunchProcessingChannel = channel;
        BlunchSequencerRuntime& channelSequencer = sequencers[channel];
        rememberedRunning[channel] = true;
        channelSequencer.running = true;
        if (!channelSequencer.activeProgram.empty()) {
          if (seededRandomEnabled()) {
            resetSeededRandomSerials(channelSequencer);
          }
          applyActiveProgramStep(channelSequencer);
        }
      }
      blunchProcessingChannel = previousProcessingChannel;
    }
    if (runHigh && !lastRunHigh && !startAllNow) {
      restartRememberedSequencers();
    }
    if (!runHigh && lastRunHigh) {
      stopAllSequencersRememberingRunning();
    }
    lastRunHigh = runHigh;
    updateRunInputState(runHigh);
    int runInputChannels = runInputChannelCount();
    unsigned manualReset = manualResets.exchange(0, std::memory_order_relaxed);
    bool resetBlockPressed =
        resetBlockTrigger.process(inputs[RESET_BLOCK_INPUT].getVoltage()) ||
        (manualReset & 1u);
    bool resetLinePressed =
        resetLineTrigger.process(inputs[RESET_LINE_INPUT].getVoltage()) ||
        (manualReset & 2u);
    bool resetSequencePressed =
        resetSequenceTrigger.process(
            inputs[RESET_SEQUENCE_INPUT].getVoltage()) ||
        (manualReset & 4u);
    bool resetPressed =
        resetBlockPressed || resetLinePressed || resetSequencePressed;
    if (resetPressed) {
      resetClockIgnoreTimer.reset();
      resetClockIgnoreActive = true;
    }
    float resetClockIgnoreElapsed =
        resetClockIgnoreActive ? resetClockIgnoreTimer.process(args.sampleTime)
                               : 1e-3f;
    bool ignoreClockInputs =
        resetClockIgnoreActive && resetClockIgnoreElapsed <= 1e-3f;
    if (resetClockIgnoreElapsed > 1e-3f) {
      resetClockIgnoreActive = false;
    }
    if (ignoreClockInputs) {
      for (int i = 0; i < 4; i++) {
        externalClockEdges[i] = false;
      }
    }
    int lineAdvanceClock = advanceModeExternalClock(AUTO_ADVANCE_PARAM);
    int blockAdvanceClock = advanceModeExternalClock(AUTO_BLOCK_ADVANCE_PARAM);
    bool advanceLinePressed = lineAdvanceClock >= 0 && !ignoreClockInputs &&
                              externalClockEdges[lineAdvanceClock];
    bool advanceTokenPressed = blockAdvanceClock >= 0 && !ignoreClockInputs &&
                               externalClockEdges[blockAdvanceClock];

    polyChannels = selectedOutputChannelCount();
    outputs[PHASE_OUTPUT].setChannels(polyChannels);
    outputs[CLOCK_OUTPUT].setChannels(polyChannels);
    outputs[EOC1_OUTPUT].setChannels(polyChannels);
    outputs[EOC2_OUTPUT].setChannels(polyChannels);
    outputs[EOC3_OUTPUT].setChannels(polyChannels);
    for (int channel = 0; channel < polyChannels; channel++) {
      bool channelRunHigh =
          runHigh && runInputAllowsChannel(runInputChannels, channel);
      processSequencerChannel(channel, scaledSampleTime, externalClockEdges,
                              channelRunHigh, resetBlockPressed,
                              resetLinePressed, resetSequencePressed,
                              advanceLinePressed, advanceTokenPressed);
    }
    for (int channel = polyChannels; channel < MAX_POLY_CHANNELS; channel++) {
      sequencers[channel].activeClockOutputHigh = false;
    }
    for (int channel = 0; channel < polyChannels; channel++) {
      bool clockPulseHigh = clockOutputPulses[channel].process(args.sampleTime);
      sequencers[channel].activeClockOutputHigh = clockPulseHigh;
      sequencers[channel].activeClockDisplayPulse *=
          std::max(0.f, 1.f - args.sampleTime * 5.f);
      if (sequencers[channel].activeClockDisplayPulse < 0.001f) {
        sequencers[channel].activeClockDisplayPulse = 0.f;
      }
      sequencers[channel].activeExternalTimingDisplayPulse -= args.sampleTime;
      if (sequencers[channel].activeExternalTimingDisplayPulse <= 0.f) {
        sequencers[channel].activeExternalTimingDisplayPulse = 0.f;
      }
      int sourceClock = activeExternalClockInput(sequencers[channel]);
      float phase = sourceClock >= 0 ? externalClockPhases[sourceClock].phase()
                                     : sequencers[channel].clockPhase;
      outputs[PHASE_OUTPUT].setVoltage(clamp(phase, 0.f, 1.f) * 10.f, channel);
      outputs[CLOCK_OUTPUT].setVoltage(clockPulseHigh ? 10.f : 0.f, channel);
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
      clockOutputPulses[channel].process(args.sampleTime);
      sequencers[channel].activeClockDisplayPulse = 0.f;
      sequencers[channel].activeExternalTimingDisplayPulse = 0.f;
      tokenMovePulses[channel].process(args.sampleTime);
      lineCyclePulses[channel].process(args.sampleTime);
      wrapPulses[channel].process(args.sampleTime);
    }
    lights[SYNTAX_ERROR_LIGHT].setBrightness(syntaxError ? 1.f : 0.f);
  }

  void fromJson(json_t* root) override {
    if (json_object_get(json_object_get(root, "data"), "fontHeightOrigin")) {
      Module::fromJson(root);
      return;
    }
    // Rebase legacy saved heights before Rack clamps them to the new range.
    std::unique_ptr<json_t, void (*)(json_t*)> adjusted(json_deep_copy(root),
                                                        json_decref);
    if (!adjusted) {
      Module::fromJson(root);
      return;
    }
    json_t* saved = json_object_get(adjusted.get(), "params");
    if (!json_is_array(saved)) {
      saved = json_array();
      json_object_set_new(adjusted.get(), "params", saved);
    }
    size_t index;
    json_t* param;
    bool found = false;
    json_array_foreach(saved, index, param) {
      if (json_integer_value(json_object_get(param, "id")) !=
          EDITOR_FONT_HEIGHT_PARAM)
        continue;
      float old = float(json_number_value(json_object_get(param, "value")));
      json_object_set_new(param, "value",
                          json_real(old - BLUNCH_FONT_HEIGHT_BASE));
      found = true;
    }
    if (!found) {
      auto* param = json_object();
      json_object_set_new(param, "id", json_integer(EDITOR_FONT_HEIGHT_PARAM));
      json_object_set_new(param, "value", json_real(-BLUNCH_FONT_HEIGHT_BASE));
      json_array_append_new(saved, param);
    }
    Module::fromJson(adjusted.get());
  }
  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "fontHeightOrigin",
                        json_real(BLUNCH_FONT_HEIGHT_BASE));
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
    json_object_set_new(rootJ, "editorFitLines", json_boolean(editorFitLines));
    json_object_set_new(rootJ, "editorLineWrapping",
                        json_boolean(editorLineWrapping));
    json_object_set_new(rootJ, "runInputMode", json_integer(runInputMode));
    json_object_set_new(
        rootJ, "defaultNumberUnit",
        json_string(
            blunch::language::clockUnitName(defaultNumberUnit).c_str()));
    json_object_set_new(rootJ, "playbackDisplayStyle",
                        json_integer(playbackDisplayStyle));
    json_object_set_new(rootJ, "playbackBars", json_boolean(playbackBars));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    for (int ch = 0; ch < MAX_POLY_CHANNELS; ++ch) {
      runningLineTexts[ch].clear();
      pendingFormulas[ch] = PendingFormula{};
    }
    json_t* fitLinesJ = json_object_get(rootJ, "editorFitLines");
    editorFitLines = !json_is_boolean(fitLinesJ) || json_is_true(fitLinesJ);
    json_t* unitJ = json_object_get(rootJ, "defaultNumberUnit");
    if (json_is_string(unitJ)) {
      auto unit = blunch::language::parseClockUnit(json_string_value(unitJ));
      if (unit != blunch::language::ClockUnit::Unknown)
        defaultNumberUnit = unit;
    }
    json_t* styleJ = json_object_get(rootJ, "playbackDisplayStyle");
    if (json_is_integer(styleJ))
      playbackDisplayStyle =
          std::max(0, std::min(4, (int)json_integer_value(styleJ)));
    json_t* barsJ = json_object_get(rootJ, "playbackBars");
    // Preserve the appearance of older patches that coupled bars to Full.
    playbackBars = json_is_boolean(barsJ)
                       ? json_is_true(barsJ)
                       : !json_is_integer(styleJ) || playbackDisplayStyle == 0;
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
      // A restored empty/invalid page must not inherit constructor examples.
      sequencers[channel].activeProgram.clear();
      if (sequenceEditorStates[channel].text.empty()) {
        stopSequencer(channel, true);
        continue;
      }
      focusedChannel = channel;
      int line = std::max(
          0, std::min(sequencers[channel].activeLine,
                      getLineCount(sequenceEditorStates[channel].text) - 1));
      sequencers[channel].activeLine = line;
      if (!commitLine(line, true)) stopSequencer(channel, true);
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
    json_t* runInputModeJ = json_object_get(rootJ, "runInputMode");
    if (runInputModeJ) {
      runInputMode =
          std::max(0, std::min((int)json_integer_value(runInputModeJ),
                               BLUNCH_RUN_INPUT_MODE_COUNT - 1));
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
    (void)line;
    return blunch::program::compileLineProgram(lineText, lineBegin,
                                               externalClockInputIndex, program,
                                               defaultNumberUnit);
  }

  void captureBaseStepState(BlunchProgramStep& step) {
    blunch::program::captureBaseStepState(step);
  }

  void restoreBaseStepState(BlunchProgramStep& step) {
    blunch::program::restoreBaseStepState(step);
  }

  void resetSeededRandomSerials(BlunchSequencerRuntime& seq) {
    for (size_t i = 0; i < seq.activeProgram.size(); i++) {
      seq.activeProgram[i].seededClockSerial = 0;
      seq.activeProgram[i].seededProbabilitySerial = 0;
      seq.activeProgram[i].seededRepeatSerial = 0;
      seq.activeProgram[i].seededTotalDurationSerial = 0;
    }
  }

  bool isBranchLocalTotalDuration(const BlunchProgramStep& step) const {
    return blunch::program::isBranchLocalTotalDuration(step);
  }

  int seededRandomEventIndex(BlunchSequencerRuntime& seq,
                             BlunchProgramStep& step, int role) {
    if (!seededRandomEnabled()) {
      return 0;
    }

    if (role == SEEDED_RANDOM_CLOCK || role == SEEDED_RANDOM_PROBABILITY) {
      if (step.hasTotalDurationGroup && step.totalDurationIsTickCount &&
          !isBranchLocalTotalDuration(step)) {
        return seq.activeTotalDurationTicks;
      }
      if (!step.hasDuration && step.repeat > 1) {
        return seq.activeProgramBeat;
      }
    }

    switch (role) {
      case SEEDED_RANDOM_CLOCK:
        return step.seededClockSerial++;
      case SEEDED_RANDOM_PROBABILITY:
        return step.seededProbabilitySerial++;
      case SEEDED_RANDOM_REPEAT:
        return step.seededRepeatSerial++;
      case SEEDED_RANDOM_TOTAL_DURATION:
        return step.seededTotalDurationSerial++;
      default:
        return 0;
    }
  }

  float randomFloatForEvent(const BlunchSequencerRuntime& seq, int role,
                            int eventIndex, int drawIndex) {
    if (!seededRandomEnabled()) {
      return random::uniform();
    }
    return blunch::sequencer::seededRandomFloat(
        seedValue(), activeContextChannel(), seq.activeLine,
        seq.activeProgramIndex, role, eventIndex, drawIndex);
  }

  bool commitLine(int line, bool resetPhase, bool acceptEdits = true) {
    BlunchLineInfo lineInfo = getLineInfo(sequenceEditorState().text, line);
    auto& running = runningLineTexts[activeContextChannel()];
    if (running.empty()) {
      for (int i = 0; i < getLineCount(sequenceEditorState().text); ++i)
        running[i] = getLineInfo(sequenceEditorState().text, i).text;
    }
    auto existing = running.find(line);
    if (!acceptEdits && existing != running.end())
      lineInfo.text = existing->second;
    std::vector<BlunchProgramStep> program;
    if (!parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      setSyntaxError(line, line == activeSequencer().activeLine
                               ? activeSequencer().activeLineText
                               : checkedFocusedLineText);
      return false;
    }

    clearSyntaxError();
    running[line] = lineInfo.text;
    activeSequencer().loadActiveProgram(line, lineInfo.text, program,
                                        resetPhase);
    if (seededRandomEnabled()) {
      resetSeededRandomSerials(activeSequencer());
    }
    if (pendingEdit().line == line && pendingEdit().text == lineInfo.text) {
      clearPendingLine();
    }
    applyActiveProgramStep(activeSequencer());
    return true;
  }

  void queuePendingActivation() {
    if (pendingEdit().line < 0 || pendingEdit().program.empty()) {
      return;
    }
    pendingEdit().armed = waitMode() != WAIT_IMMEDIATE;
    pendingEdit().viewing = true;
  }

  void clearPendingLine() {
    pendingEdit().line = -1;
    pendingEdit().text.clear();
    pendingEdit().program.clear();
    pendingEdit().viewing = true;
    pendingEdit().armed = false;
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

  void discardChannelPending(int channel) {
    channel = clampChannel(channel);
    if (!channelHasPendingFormula(channel)) return;
    const auto& running = runningLineTexts[channel];
    std::string restored;
    if (!running.empty()) {
      for (int line = 0; line <= running.rbegin()->first; ++line) {
        if (line) restored += "\n";
        auto found = running.find(line);
        if (found != running.end()) restored += found->second;
      }
    }
    auto& state = sequenceEditorStates[channel];
    state.text = restored;
    state.cursor = std::min(state.cursor, int(restored.size()));
    state.selection = state.cursor;
    state.dirty = true;
    pendingFormulas[channel] = PendingFormula{};
    if (channel == focusedChannel) {
      clearSyntaxError();
      checkedFocusedLineText = getLineInfo(restored, selectedLine).text;
    }
    refreshChannelsEditorState(true);
    editorViewDirty = true;
  }

  void syncChannelsEditorEdit(int channel) {
    if (!channelsEditorEditingEnabled(channel)) {
      return;
    }
    if (channelsEditorState.text.empty()) {
      return;
    }
    channel = clampChannel(channel);
    if (channel >= getLineCount(channelsEditorState.text)) {
      return;
    }

    BlunchLineInfo rowInfo = getLineInfo(channelsEditorState.text, channel);
    int activeLine = channelOverviewLine(channel);
    if (rowInfo.text ==
        getLineInfo(sequenceEditorStates[channel].text, activeLine).text)
      return;
    replaceLineTextForChannel(channel, activeLine, rowInfo.text);
    auto& source = sequenceEditorStates[channel];
    BlunchLineInfo updated = getLineInfo(source.text, activeLine);
    int column = std::max(0, channelsEditorState.cursor - rowInfo.begin);
    source.cursor = updated.begin + std::min(column, int(rowInfo.text.size()));
    source.selection = source.cursor;
  }

  std::string randomTemplateError;
  std::string generateRandomProgram(bool startup) {
    try {
      json_error_t error;
      std::string path =
          asset::plugin(pluginInstance, "res/programs/blunch-random.json");
      std::unique_ptr<json_t, void (*)(json_t*)> root(
          json_load_file(path.c_str(), JSON_REJECT_DUPLICATES, &error),
          json_decref);
      if (!root) throw std::runtime_error(std::string(error.text));
      auto catalogue = blunch::random_program::readCatalogue(root.get());
      unsigned int entropy =
          (unsigned int)std::floor(double(random::uniform()) * 4294967296.);
      std::string result = blunch::random_program::expandTemplate(
          catalogue, startup ? catalogue.startup : catalogue.randomize,
          entropy);
      std::istringstream lines(result);
      std::string line;
      int count = 0;
      while (std::getline(lines, line)) {
        if (!blunch::language::parseClockLiteral(line).ok())
          throw std::runtime_error(
              "Template generated an invalid clock formula: " + line);
        ++count;
      }
      if (count < 1 || count > (startup ? 4 : 1))
        throw std::runtime_error(startup ? "Startup needs 1..4 formula lines"
                                         : "Randomize needs one formula line");
      randomTemplateError.clear();
      return result;
    } catch (const std::exception& error) {
      randomTemplateError = error.what();
      WARN("Blunch random templates: %s", error.what());
      return "";
    }
  }
  void randomizeLineForChannel(int channel, int line, bool restoreCursor) {
    int previousFocusedChannel = focusedChannel;
    focusedChannel = clampChannel(channel);
    std::string program = generateRandomProgram(false);
    if (program.empty()) {
      focusedChannel = previousFocusedChannel;
      return;
    }
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
      pendingEdit().line = line;
      pendingEdit().text = program;
      pendingEdit().program = parsedProgram;
      pendingEdit().viewing = true;
      queuePendingActivation();
    }
    focusedChannel = previousFocusedChannel;
  }

  void randomizeSelectedLine() {
    randomizeLineForChannel(focusedChannel, selectedLine, true);
  }

  void randomizeActiveChannelsLine() {
    int channel = clampChannel(channelsCursorChannel);
    randomizeLineForChannel(channel, channelOverviewLine(channel), false);
    refreshChannelsEditorState(true);
  }

  void cancelPendingLine(int line) {
    if (errorLine == line) {
      std::string revertText;
      if (line == activeSequencer().activeLine) {
        revertText = activeSequencer().activeLineText;
      } else if (pendingEdit().line == line && !pendingEdit().text.empty()) {
        revertText = pendingEdit().text;
      } else if (errorLineRevertLine == line) {
        revertText = errorLineRevertText;
      }
      replaceLineText(line, revertText);
      if (pendingEdit().line == line && line == activeSequencer().activeLine) {
        clearPendingLine();
      }
      checkedFocusedLineText =
          getLineInfo(sequenceEditorState().text, line).text;
      clearSyntaxError();
      return;
    }

    if (pendingEdit().line == line) {
      if (line == activeSequencer().activeLine) {
        replaceLineText(line, activeSequencer().activeLineText);
      }
      clearPendingLine();
    }
    checkedFocusedLineText = getLineInfo(sequenceEditorState().text, line).text;
  }

  void showPendingLine() {
    if (pendingEdit().line < 0 || pendingEdit().text.empty()) {
      return;
    }

    replaceLineText(pendingEdit().line, pendingEdit().text);
    checkedFocusedLineText = pendingEdit().text;
    pendingEdit().viewing = true;
  }

  void showActiveLineVersion() {
    if (pendingEdit().line != activeSequencer().activeLine) {
      return;
    }

    replaceLineText(activeSequencer().activeLine,
                    activeSequencer().activeLineText);
    checkedFocusedLineText = activeSequencer().activeLineText;
    pendingEdit().viewing = false;
  }

  void togglePendingView() {
    if (errorLine == activeSequencer().activeLine) {
      replaceLineText(activeSequencer().activeLine,
                      activeSequencer().activeLineText);
      checkedFocusedLineText = activeSequencer().activeLineText;
      clearSyntaxError();
      pendingEdit().viewing = false;
      return;
    }

    if (pendingEdit().line != activeSequencer().activeLine) {
      return;
    }

    if (pendingEdit().viewing) {
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
        pendingEdit().line = line;
        pendingEdit().text = lineInfo.text;
        pendingEdit().program = program;
        pendingEdit().armed = false;
        if (errorLine == line) {
          clearSyntaxError();
        }
        syntaxError = false;
      } else {
        std::string revertText =
            pendingEdit().line == line && !pendingEdit().text.empty()
                ? pendingEdit().text
                : previousCheckedLineText;
        setSyntaxError(line, revertText);
      }
      return;
    }

    if (lineInfo.text == activeSequencer().activeLineText) {
      if (pendingEdit().line == line && pendingEdit().viewing) {
        clearPendingLine();
      }
      if (errorLine == line) {
        clearSyntaxError();
      }
      checkedFocusedLineText = lineInfo.text;
      return;
    }

    if (pendingEdit().line == line && lineInfo.text == pendingEdit().text) {
      checkedFocusedLineText = lineInfo.text;
      pendingEdit().viewing = true;
      return;
    }

    if (lineInfo.text == checkedFocusedLineText &&
        (pendingEdit().line == line || errorLine == line)) {
      return;
    }

    checkedFocusedLineText = lineInfo.text;
    std::vector<BlunchProgramStep> program;
    if (parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      pendingEdit().line = line;
      pendingEdit().text = lineInfo.text;
      pendingEdit().program = program;
      pendingEdit().viewing = true;
      pendingEdit().armed = false;
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
    if (pendingEdit().line < 0 || pendingEdit().program.empty()) {
      return;
    }

    runningLineTexts[activeContextChannel()][pendingEdit().line] =
        pendingEdit().text;
    activeSequencer().loadActiveProgram(pendingEdit().line, pendingEdit().text,
                                        pendingEdit().program, resetPhase);
    if (seededRandomEnabled()) {
      resetSeededRandomSerials(activeSequencer());
    }
    clearPendingLine();
    clearSyntaxError();
    applyActiveProgramStep(activeSequencer());
  }

  bool activatePendingLineIfArmed(bool resetPhase) {
    if (!pendingEdit().armed || pendingEdit().line < 0 ||
        pendingEdit().program.empty()) {
      return false;
    }

    commitPendingLine(resetPhase);
    startSequencer(activeContextChannel());
    triggerTokenMovePulse();
    return true;
  }

  bool activatePendingLineAtBoundary(WaitBoundary boundary, bool resetPhase) {
    if (!pendingEdit().armed) {
      return false;
    }

    WaitMode mode = waitMode();
    bool matches = (mode == WAIT_CLOCK && boundary == WAIT_BOUNDARY_CLOCK) ||
                   (mode == WAIT_BLOCK && boundary == WAIT_BOUNDARY_BLOCK) ||
                   (mode == WAIT_LINE && boundary == WAIT_BOUNDARY_LINE) ||
                   (mode == WAIT_PAGE && boundary == WAIT_BOUNDARY_PAGE);
    if (!matches) {
      return false;
    }

    return activatePendingLineIfArmed(resetPhase);
  }

  bool activatePendingLineAtExternalClock(int externalClock, bool resetPhase) {
    if (!pendingEdit().armed || externalClock < 0 || externalClock > 3) {
      return false;
    }

    WaitMode mode = waitMode();
    bool matches = (mode == WAIT_EXTERNAL_W && externalClock == 0) ||
                   (mode == WAIT_EXTERNAL_X && externalClock == 1) ||
                   (mode == WAIT_EXTERNAL_Y && externalClock == 2) ||
                   (mode == WAIT_EXTERNAL_Z && externalClock == 3);
    if (!matches) {
      return false;
    }

    return activatePendingLineIfArmed(resetPhase);
  }

  bool shouldDelayExecution() { return waitMode() != WAIT_IMMEDIATE; }

  bool executeLine(int line, bool resetPhase) {
    BlunchLineInfo lineInfo = getLineInfo(sequenceEditorState().text, line);
    std::vector<BlunchProgramStep> program;
    if (!parseLineProgram(line, lineInfo.begin, lineInfo.text, program)) {
      setSyntaxError(line, line == activeSequencer().activeLine
                               ? activeSequencer().activeLineText
                               : checkedFocusedLineText);
      return false;
    }

    pendingEdit().line = line;
    pendingEdit().text = lineInfo.text;
    pendingEdit().program = program;
    pendingEdit().viewing = true;
    clearSyntaxError();

    if (!shouldDelayExecution()) {
      commitPendingLine(resetPhase);
      return true;
    }

    queuePendingActivation();
    return true;
  }

  void resetActiveProgram(bool resetPhase) {
    if (activeSequencer().activeProgram.empty()) {
      commitLine(activeSequencer().activeLine, resetPhase);
      return;
    }

    activeSequencer().resetActiveProgramState(resetPhase);
    if (seededRandomEnabled()) {
      resetSeededRandomSerials(activeSequencer());
    }
    applyActiveProgramStep(activeSequencer());
  }

  void resetActiveBlock(bool resetPhase) {
    BlunchSequencerRuntime& seq = activeSequencer();
    if (seq.activeProgram.empty()) {
      commitLine(seq.activeLine, resetPhase);
      return;
    }

    seq.activeProgramIndex = std::max(
        0, std::min(seq.activeProgramIndex, (int)seq.activeProgram.size() - 1));
    const BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    if (step.hasTotalDurationGroup) {
      seq.activeProgramIndex = step.totalDurationGroupStart;
    }
    seq.activeProgramBeat = 0;
    seq.activeProgramElapsedSeconds = 0.0;
    seq.activeTotalDurationGroupId = -1;
    seq.activeTotalDurationBranchLocal = false;
    seq.activeTotalDurationElapsedSeconds = 0.0;
    seq.activeTotalDurationTicks = 0;
    if (resetPhase) {
      seq.clockPhase = 0.f;
      seq.activeClockRamp = 0.f;
    }
    if (seededRandomEnabled()) {
      resetSeededRandomSerials(seq);
    }
    applyActiveProgramStep(seq);
  }

  void resetActiveSequence(bool resetPhase) {
    int lineCount = getLineCount(sequenceEditorState().text);
    for (int line = 0; line < lineCount; line++) {
      BlunchLineInfo lineInfo = getLineInfo(sequenceEditorState().text, line);
      if (isBlankLine(lineInfo.text)) {
        continue;
      }
      commitLine(line, resetPhase);
      return;
    }
    resetActiveProgram(resetPhase);
  }

  void sampleStepRepeatArgument(BlunchSequencerRuntime& seq,
                                BlunchProgramStep& step) {
    if (step.repeatIsSequence && !step.repeatSequence.empty()) {
      int eventIndex = step.seededRepeatSerial++;
      size_t sequenceIndex =
          (size_t)(eventIndex % (int)step.repeatSequence.size());
      const blunch::language::ClockLiteralAst& repeatItem =
          step.repeatSequence[sequenceIndex];
      step.repeatHighlightBegin = step.sourceLineBegin + repeatItem.range.begin;
      step.repeatHighlightEnd = step.sourceLineBegin + repeatItem.range.end;
      step.repeatHighlightIsOwn = true;

      if (repeatItem.kind == blunch::language::ClockLiteralKind::RandomRange &&
          !repeatItem.randomChoices.empty()) {
        const blunch::language::RandomChoiceAst& choice =
            seededRandomEnabled()
                ? chooseRandomChoice(
                      repeatItem, randomFloatForEvent(seq, SEEDED_RANDOM_REPEAT,
                                                      eventIndex, 0))
                : chooseRandomChoice(repeatItem);
        step.repeatHighlightBegin = step.sourceLineBegin + choice.range.begin;
        step.repeatHighlightEnd = step.sourceLineBegin + choice.range.end;

        if (!step.repeatRandomIsDuration) {
          step.hasDuration = false;
          int sampledRepeat =
              seededRandomEnabled()
                  ? sampleRandomChoiceInteger(
                        choice, randomFloatForEvent(seq, SEEDED_RANDOM_REPEAT,
                                                    eventIndex, 1))
                  : sampleRandomChoiceInteger(choice);
          step.repeat = std::max(1, sampledRepeat);
          step.baseRepeat = step.repeat;
          step.baseHasDuration = step.hasDuration;
          step.baseDurationSeconds = step.durationSeconds;
          step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
          step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
          step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
          return;
        }

        double sampledValue =
            seededRandomEnabled()
                ? sampleRandomChoiceValue(
                      choice, randomFloatForEvent(seq, SEEDED_RANDOM_REPEAT,
                                                  eventIndex, 1))
                : sampleRandomChoiceValue(choice);
        blunch::language::ClockLiteralAst literal =
            blunch::program::literalForRandomChoice(repeatItem, choice,
                                                    sampledValue);
        blunch::language::EvaluationResult eval =
            blunch::language::evaluateClockLiteral(literal);
        if (eval.ok()) {
          step.hasDuration = true;
          step.durationSeconds = std::max(0.0, eval.spec.periodSeconds);
          step.baseHasDuration = step.hasDuration;
          step.baseDurationSeconds = step.durationSeconds;
          step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
          step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
          step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
        }
        return;
      }

      if (!step.repeatRandomIsDuration) {
        step.hasDuration = false;
        step.repeat = std::max(1, (int)std::round(repeatItem.value));
        step.baseRepeat = step.repeat;
        step.baseHasDuration = step.hasDuration;
        step.baseDurationSeconds = step.durationSeconds;
        step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
        step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
        step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
        return;
      }

      blunch::language::EvaluationResult eval =
          blunch::language::evaluateClockLiteral(repeatItem);
      if (eval.ok()) {
        step.hasDuration = true;
        step.durationSeconds = std::max(0.0, eval.spec.periodSeconds);
        step.baseHasDuration = step.hasDuration;
        step.baseDurationSeconds = step.durationSeconds;
        step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
        step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
        step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
      }
      return;
    }

    if (!step.repeatIsRandom || step.repeatRandom.randomChoices.empty()) {
      return;
    }

    int eventIndex = seededRandomEventIndex(seq, step, SEEDED_RANDOM_REPEAT);
    const blunch::language::RandomChoiceAst& choice =
        seededRandomEnabled()
            ? chooseRandomChoice(
                  step.repeatRandom,
                  randomFloatForEvent(seq, SEEDED_RANDOM_REPEAT, eventIndex, 0))
            : chooseRandomChoice(step.repeatRandom);
    step.repeatHighlightBegin = step.sourceLineBegin + choice.range.begin;
    step.repeatHighlightEnd = step.sourceLineBegin + choice.range.end;

    if (!step.repeatRandomIsDuration) {
      step.hasDuration = false;
      int sampledRepeat =
          seededRandomEnabled()
              ? sampleRandomChoiceInteger(
                    choice, randomFloatForEvent(seq, SEEDED_RANDOM_REPEAT,
                                                eventIndex, 1))
              : sampleRandomChoiceInteger(choice);
      step.repeat = std::max(1, sampledRepeat);
      step.baseRepeat = step.repeat;
      step.baseHasDuration = step.hasDuration;
      step.baseDurationSeconds = step.durationSeconds;
      step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
      step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
      step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
      return;
    }

    double sampledValue =
        seededRandomEnabled()
            ? sampleRandomChoiceValue(
                  choice,
                  randomFloatForEvent(seq, SEEDED_RANDOM_REPEAT, eventIndex, 1))
            : sampleRandomChoiceValue(choice);
    blunch::language::ClockLiteralAst literal =
        blunch::program::literalForRandomChoice(step.repeatRandom, choice,
                                                sampledValue);
    blunch::language::EvaluationResult eval =
        blunch::language::evaluateClockLiteral(literal);
    if (eval.ok()) {
      step.hasDuration = true;
      step.durationSeconds = std::max(0.0, eval.spec.periodSeconds);
      step.baseHasDuration = step.hasDuration;
      step.baseDurationSeconds = step.durationSeconds;
      step.baseRepeatHighlightBegin = step.repeatHighlightBegin;
      step.baseRepeatHighlightEnd = step.repeatHighlightEnd;
      step.baseRepeatHighlightIsOwn = step.repeatHighlightIsOwn;
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

    int eventIndex =
        seededRandomEventIndex(seq, step, SEEDED_RANDOM_TOTAL_DURATION);
    const blunch::language::RandomChoiceAst& choice =
        seededRandomEnabled()
            ? chooseRandomChoice(
                  step.totalDurationRandom,
                  randomFloatForEvent(seq, SEEDED_RANDOM_TOTAL_DURATION,
                                      eventIndex, 0))
            : chooseRandomChoice(step.totalDurationRandom);
    bool suffixHasUnit = step.totalDurationRandom.unitRange.end >
                         step.totalDurationRandom.unitRange.begin;
    bool selectedIsDuration =
        blunch::program::randomChoiceHasExplicitUnit(choice) || suffixHasUnit;

    bool selectedIsTickCount = !selectedIsDuration;
    int selectedTicks = 1;
    float selectedSeconds = 0.f;
    if (selectedIsTickCount) {
      int sampledTicks =
          seededRandomEnabled()
              ? sampleRandomChoiceInteger(
                    choice,
                    randomFloatForEvent(seq, SEEDED_RANDOM_TOTAL_DURATION,
                                        eventIndex, 1))
              : sampleRandomChoiceInteger(choice);
      selectedTicks = std::max(1, sampledTicks);
    } else {
      double sampledValue =
          seededRandomEnabled()
              ? sampleRandomChoiceValue(
                    choice,
                    randomFloatForEvent(seq, SEEDED_RANDOM_TOTAL_DURATION,
                                        eventIndex, 1))
              : sampleRandomChoiceValue(choice);
      blunch::language::ClockLiteralAst literal =
          blunch::program::literalForRandomChoice(step.totalDurationRandom,
                                                  choice, sampledValue);
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
      seq.activeProgram[i].baseTotalDurationIsTickCount = selectedIsTickCount;
      seq.activeProgram[i].baseTotalDurationTicks = selectedTicks;
      seq.activeProgram[i].baseTotalDurationSeconds = selectedSeconds;
      seq.activeProgram[i].baseTotalDurationHighlightBegin =
          seq.activeProgram[i].totalDurationHighlightBegin;
      seq.activeProgram[i].baseTotalDurationHighlightEnd =
          seq.activeProgram[i].totalDurationHighlightEnd;
    }
  }

  void applyRandomChoiceModifiers(
      BlunchProgramStep& step, const blunch::language::RandomChoiceAst& choice,
      int stepIndex) {
    if (choice.hasRepeatModifier) {
      step.repeat = std::max(1, choice.repeat);
      step.repeatExternalClockInput =
          choice.repeatUsesExternalClock
              ? externalClockInputIndex(choice.repeatExternalClock)
              : -1;
      step.hasDuration = choice.repeatIsDuration;
      step.durationSeconds =
          choice.repeatIsDuration
              ? (float)std::max(0.0, choice.repeatDurationSeconds)
              : 0.f;
      step.repeatHighlightBegin =
          step.sourceLineBegin + choice.repeatValueRange.begin;
      step.repeatHighlightEnd =
          step.sourceLineBegin + choice.repeatValueRange.end;
      step.repeatHighlightIsOwn = true;
    }

    if (!choice.hasTotalDurationModifier) {
      return;
    }

    step.hasTotalDurationGroup = true;
    step.totalDurationBranchLocal = true;
    step.totalDurationGroupId = stepIndex;
    step.totalDurationGroupStart = stepIndex;
    step.totalDurationGroupEnd = stepIndex + 1;
    step.totalDurationIsRandom = false;
    step.totalDurationIsTickCount = choice.totalDurationIsTickCount;
    step.totalDurationTicks = choice.totalDurationTicks;
    step.totalDurationExternalClockInput =
        choice.totalDurationUsesExternalClock
            ? externalClockInputIndex(choice.totalDurationExternalClock)
            : -1;
    step.totalDurationSeconds =
        choice.totalDurationIsTickCount
            ? 0.f
            : (float)std::max(0.0, choice.totalDurationSeconds);
    step.totalDurationHighlightBegin =
        step.sourceLineBegin + choice.totalDurationValueRange.begin;
    step.totalDurationHighlightEnd =
        step.sourceLineBegin + choice.totalDurationValueRange.end;
  }

  void applyActiveProgramStep(BlunchSequencerRuntime& seq) {
    if (seq.activeProgram.empty()) {
      return;
    }

    seq.activeProgramIndex = std::max(
        0, std::min(seq.activeProgramIndex, (int)seq.activeProgram.size() - 1));
    BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    sampleStepRepeatArgument(seq, step);
    refreshActiveClockSpecForStep(seq);
    blunch::sequencer::syncActiveHighlightFromStep(seq);
    seq.activeProgramElapsedSeconds = 0.0;
    if (step.hasTotalDurationGroup) {
      if (seq.activeTotalDurationGroupId != step.totalDurationGroupId ||
          seq.activeTotalDurationBranchLocal != step.totalDurationBranchLocal) {
        seq.activeTotalDurationGroupId = step.totalDurationGroupId;
        seq.activeTotalDurationBranchLocal = step.totalDurationBranchLocal;
        seq.activeTotalDurationElapsedSeconds = 0.0;
        seq.activeTotalDurationTicks = 0;
        seq.activeTotalDurationStepBeat = 0;
        sampleTotalDurationGroup(seq, seq.activeProgramIndex);
      }
    } else {
      seq.activeTotalDurationGroupId = -1;
      seq.activeTotalDurationBranchLocal = false;
      seq.activeTotalDurationElapsedSeconds = 0.0;
      seq.activeTotalDurationTicks = 0;
      seq.activeTotalDurationStepBeat = 0;
    }
    scheduleTokenStartTick(seq);
    chooseActiveStepPlayback(seq);
  }

  static int externalClockInputIndex(char clock) {
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
    return computerscare::blunch::timeScaleWithCv(
        params[SPEED_OF_TIME_PARAM].getValue(),
        inputs[TIME_CV_INPUT].getVoltage());
  }

  void advanceActiveProgramBeat(BlunchSequencerRuntime& seq,
                                bool countTotalTick = true) {
    if (seq.activeProgram.empty()) {
      return;
    }

    BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    if (countTotalTick && advanceActiveTotalTickCount(seq)) {
      return;
    }

    bool branchLocalTotalDuration = isBranchLocalTotalDuration(step);
    if (branchLocalTotalDuration || step.hasDuration) {
      chooseActiveStepPlayback(seq);
      return;
    }

    if (!blunch::sequencer::activeClockTickAdvancesStepRepeat(seq)) {
      if (blunch::sequencer::advanceRepeatWithinTotalDurationGroup(seq)) {
        refreshActiveClockSpecForStep(seq);
        blunch::sequencer::syncActiveHighlightFromStep(seq);
        chooseActiveStepPlayback(seq);
        return;
      }
      if (blunch::sequencer::advanceWithinTotalDurationGroup(seq)) {
        applyActiveProgramStep(seq);
        triggerTokenMovePulse();
        return;
      }
      refreshActiveClockSpecForStep(seq);
      blunch::sequencer::syncActiveHighlightFromStep(seq);
      chooseActiveStepPlayback(seq);
      return;
    }

    seq.activeProgramBeat++;
    if (seq.activeProgramBeat >= step.repeat) {
      advanceActiveProgramStep(seq);
    } else {
      refreshActiveClockSpecForStep(seq);
      blunch::sequencer::syncActiveHighlightFromStep(seq);
      chooseActiveStepPlayback(seq);
    }
  }

  void advanceActiveProgramDuration(BlunchSequencerRuntime& seq,
                                    float sampleTime) {
    if (blunch::sequencer::advanceProgramDuration(seq, sampleTime)) {
      double elapsedCarry = seq.activeProgramElapsedSeconds;
      advanceActiveProgramStep(seq);
      seq.activeProgramElapsedSeconds += elapsedCarry;
    }
  }

  bool advanceActiveTotalDuration(BlunchSequencerRuntime& seq,
                                  float sampleTime) {
    if (!blunch::sequencer::advanceTotalDuration(seq, sampleTime)) {
      return false;
    }
    double elapsedCarry = seq.activeTotalDurationElapsedSeconds;
    finishActiveTotalDurationGroup(seq);
    if (!seq.activeProgram.empty() && seq.activeProgramIndex >= 0 &&
        seq.activeProgramIndex < (int)seq.activeProgram.size() &&
        seq.activeProgram[seq.activeProgramIndex].hasTotalDurationGroup &&
        !seq.activeProgram[seq.activeProgramIndex].totalDurationIsTickCount) {
      seq.activeTotalDurationElapsedSeconds += elapsedCarry;
    }
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
    if (activatePendingLineAtBoundary(WAIT_BOUNDARY_BLOCK, true)) {
      return;
    }

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
    return (int)std::round(params[AUTO_BLOCK_ADVANCE_PARAM].getValue()) == 1;
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
    if (activatePendingLineAtBoundary(WAIT_BOUNDARY_BLOCK, true)) {
      return;
    }

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
    if (activatePendingLineAtBoundary(WAIT_BOUNDARY_LINE, true)) {
      return true;
    }

    if (autoLineAdvanceEnabled()) {
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
    const auto& running = runningLineTexts[activeContextChannel()];
    int lineCount = running.empty() ? getLineCount(sequenceEditorState().text)
                                    : running.rbegin()->first + 1;
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
      auto submitted = running.find(nextLine);
      if (submitted == running.end()) continue;
      lineInfo.text = submitted->second;
      if (isBlankLine(lineInfo.text)) {
        continue;
      }

      if (wrapped &&
          activatePendingLineAtBoundary(WAIT_BOUNDARY_PAGE, resetPhase)) {
        triggerWrapPulse();
        return true;
      }

      if (!commitLine(nextLine, resetPhase, false)) {
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
    if (seq.activeProgram.empty()) {
      blunch::sequencer::chooseStepPlayback(seq, random::uniform());
      return;
    }
    BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    int eventIndex =
        seededRandomEventIndex(seq, step, SEEDED_RANDOM_PROBABILITY);
    blunch::sequencer::chooseStepPlayback(
        seq,
        randomFloatForEvent(seq, SEEDED_RANDOM_PROBABILITY, eventIndex, 0));
  }

  void refreshActiveClockSpecForStep(BlunchSequencerRuntime& seq) {
    if (seq.activeProgram.empty()) {
      return;
    }

    BlunchProgramStep& step = seq.activeProgram[seq.activeProgramIndex];
    restoreBaseStepState(step);
    if (!step.hasRandomValue) {
      seq.activeClockSpec = step.spec;
      return;
    }

    const blunch::language::RandomChoiceAst* selectedChoice = NULL;
    double minValue = step.literal.minValue;
    double maxValue = step.literal.maxValue;
    int eventIndex = seededRandomEventIndex(seq, step, SEEDED_RANDOM_CLOCK);
    if (!step.literal.randomChoices.empty()) {
      float choiceRandom =
          randomFloatForEvent(seq, SEEDED_RANDOM_CLOCK, eventIndex, 0);
      size_t choiceIndex =
          std::min((size_t)(choiceRandom * step.literal.randomChoices.size()),
                   step.literal.randomChoices.size() - 1);
      const blunch::language::RandomChoiceAst& choice =
          step.literal.randomChoices[choiceIndex];
      selectedChoice = &choice;
      step.isRest = step.baseIsRest || choice.restChoice;
      if (choice.probabilityRange.end > choice.probabilityRange.begin) {
        step.probability = std::max(0, std::min(100, choice.probability));
      }
      step.highlightBegin = step.sourceLineBegin + choice.range.begin;
      step.highlightEnd = step.sourceLineBegin + choice.range.end;
      if (blunch::program::randomChoiceIsExternalClock(choice)) {
        step.externalClockInput = externalClockInputIndex(choice.externalClock);
        if (blunch::program::randomChoiceHasModifiers(choice)) {
          applyRandomChoiceModifiers(step, choice, seq.activeProgramIndex);
        }
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
      sampledValue = lowValue + randomFloatForEvent(seq, SEEDED_RANDOM_CLOCK,
                                                    eventIndex, 1) *
                                    (highValue - lowValue);
    }

    blunch::language::ClockLiteralAst sampledLiteral =
        selectedChoice ? blunch::program::literalForRandomChoice(
                             step.literal, *selectedChoice, sampledValue)
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
    if (selectedChoice &&
        blunch::program::randomChoiceHasModifiers(*selectedChoice)) {
      applyRandomChoiceModifiers(step, *selectedChoice, seq.activeProgramIndex);
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
    blunch::sequencer::TimingScopeProgress highlight;
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

static bool blunchShortcutHelpHeld(bool controlMode = false) {
  return APP->window && APP->window->win &&
         glfwGetKey(APP->window->win, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS &&
         ((APP->window->getMods() & RACK_MOD_MASK) == 0 ||
          (controlMode &&
           (APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_SHIFT));
}

static float blunchPendingPulse() {
  return .35f + .55f * (.5f + .5f * std::sin(float(rack::system::getTime()) *
                                             2.f * float(M_PI) / 2.4f));
}

struct BlunchTextEditor : ComputerscareTextEditor {
  void draw(const DrawArgs& args) override {
    float top = -1.f, bottom = -1.f;
    std::vector<std::pair<float, float>> pendingRows;
    if (module) {
      auto font = loadEditorFont();
      if (font) {
        nvgSave(args.vg);
        nvgScale(args.vg, getFontScaleX(), getFontScaleY());
        applyTextStyle(args.vg, font);
        auto layout = ComputerscareTextEditorLayout::build(
            args.vg, text, getScaledBoxSize(), style);
        nvgRestore(args.vg);
        for (size_t i = 0; i < layout.rows.size(); ++i) {
          const auto& row = layout.rows[i];
          if (row.begin <= cursor && cursor <= row.end) {
            top = row.top * getFontScaleY();
            bottom = row.bottom * getFontScaleY();
          }
          if (module->showingChannelsView() &&
              i < ComputerscareBlunch::MAX_POLY_CHANNELS &&
              module->channelHasPendingFormula(i))
            pendingRows.emplace_back(row.top * getFontScaleY(),
                                     row.bottom * getFontScaleY());
        }
      }
    }
    phlooper::drawDisplayFrame(args.vg, box.size, -1.f, -1.f, 1.f);
    phlooper::drawDisplayWallIndicator(args.vg, box.size, top, bottom,
                                       COLOR_COMPUTERSCARE_GREEN, 1.f);
    float pulse = blunchPendingPulse();
    for (const auto& row : pendingRows)
      phlooper::drawDisplayWallIndicator(args.vg, box.size, row.first,
                                         row.second,
                                         nvgRGBAf(1.f, .93f, .6f, pulse), 1.f);
    ComputerscareTextEditor::draw(args);
  }
  ComputerscareBlunch* module = nullptr;
  void updatePolicy() {
    readOnly =
        module && (module->controlMode ||
                   blunchShortcutHelpHeld(module && module->controlMode) ||
                   (module->showingChannelsView() &&
                    !module->channelsEditorEditingEnabled(getCursorLine())));
    hideCursor = readOnly;
  }
  void constrainSelection() {
    if (!module || !module->showingChannelsView() || readOnly) return;
    BlunchLineInfo line = getLineInfo(text, getCursorLine());
    selection = std::max(line.begin, std::min(selection, line.end));
  }
  void onChange(const ChangeEvent& e) override {
    if (!suppressChangeTracking && module &&
        (module->controlMode || blunchShortcutHelpHeld(module->controlMode)) &&
        state) {
      text = state->text;
      cursor = state->cursor;
      selection = state->selection;
      return;
    }
    if (!suppressChangeTracking) updatePolicy();
    if (!suppressChangeTracking && module && module->showingChannelsView() &&
        state == &module->channelsEditorState &&
        (readOnly || std::count(text.begin(), text.end(), '\n') !=
                         ComputerscareBlunch::MAX_POLY_CHANNELS - 1)) {
      // An All-view row is one formula, never a way to merge/delete channel
      // rows.
      text = state->text;
      cursor = state->cursor;
      selection = state->selection;
      return;
    }
    ComputerscareTextEditor::onChange(e);
    if (!suppressChangeTracking && module && module->showingChannelsView() &&
        state == &module->channelsEditorState) {
      module->channelsCursorChannel = blunchChannelForChannelsViewLine(
          getCursorLine(), ComputerscareBlunch::MAX_POLY_CHANNELS);
      module->syncChannelsEditorEdit(module->channelsCursorChannel);
    }
  }
  void onSelectText(const SelectTextEvent& e) override {
    if ((module && (module->controlMode || blunchShortcutHelpHeld())) ||
        computerscare::blunch::isShortcutHelpCharacter(e.codepoint)) {
      e.consume(this);
      return;
    }
    updatePolicy();
    constrainSelection();
    ComputerscareTextEditor::onSelectText(e);
  }
  void onSelectKey(const SelectKeyEvent& e) override {
    if ((module && blunchShortcutHelpHeld(module && module->controlMode)) ||
        computerscare::blunch::isShortcutHelpKey(e.key, e.keyName, e.mods)) {
      e.consume(this);
      return;
    }
    if (module && module->handleControlKey(e.key, e.mods, e.action)) {
      if (module->showingChannelsView()) {
        state = &module->channelsEditorState;
        syncFromState();
      }
      e.consume(this);
      return;
    }
    updatePolicy();
    constrainSelection();
    ComputerscareTextEditor::onSelectKey(e);
    if (module && module->controlMode && module->showingChannelsView() &&
        (e.key == GLFW_KEY_LEFT || e.key == GLFW_KEY_RIGHT)) {
      module->controlSelection.moveTo(getCursorLine(), false);
      module->channelsCursorChannel = module->controlSelection.focus;
    }
    updatePolicy();
  }
  void onButton(const ButtonEvent& e) override {
    if (module && blunchShortcutHelpHeld(module->controlMode)) {
      e.consume(this);
      return;
    }
    if (module && module->controlMode && module->showingChannelsView() &&
        e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      APP->event->setSelectedWidget(this);
      int offset =
          std::max(0, std::min(getTextPosition(e.pos), (int)text.size()));
      int row = getAllViewTextPositionForOffset(text, offset).line;
      bool additive = (e.mods & RACK_MOD_MASK) == GLFW_MOD_CONTROL ||
                      (e.mods & RACK_MOD_MASK) == GLFW_MOD_SUPER;
      if (additive) {
        module->controlSelection.focus = std::max(0, std::min(15, row));
        module->controlSelection.toggleFocused();
      } else if (e.mods & GLFW_MOD_SHIFT)
        module->controlSelection.moveTo(row, true);
      else
        module->controlSelection.selectOnly(row);
      module->focusControlRow();
      state = &module->channelsEditorState;
      syncFromState();
      e.consume(this);
      return;
    }
    ComputerscareTextEditor::onButton(e);
    updatePolicy();
  }
  void onDoubleClick(const DoubleClickEvent& e) override {
    if (module && (module->controlMode || blunchShortcutHelpHeld())) {
      e.consume(this);
      return;
    }
    ComputerscareTextEditor::onDoubleClick(e);
  }
  void onDragHover(const DragHoverEvent& e) override {
    if (module && (module->controlMode || blunchShortcutHelpHeld())) {
      e.consume(this);
      return;
    }
    ComputerscareTextEditor::onDragHover(e);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    updatePolicy();
    ComputerscareTextEditor::drawLayer(args, layer);
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
    nvgFontSize(args.vg, 16.f);
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

    const char* labels[] = {"w", "x", "y", "z"};
    const float jackCenters[] = {12.f, 42.f, 72.f, 102.f};
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

struct BlunchSeedLabel : TransparentWidget {
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
    nvgText(args.vg, box.size.x * 0.5f, 0.f, "seed", NULL);
  }
};

struct BlunchChannelChoiceItem : MenuItem {
  ComputerscareBlunch* module = nullptr;
  int channel = -1;
  void step() override {
    rightText = CHECKMARK(
        module && (channel < 0 ? module->showingChannelsView()
                               : !module->showingChannelsView() &&
                                     module->focusedChannel == channel));
    MenuItem::step();
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    if (module && channel >= module->selectedOutputChannelCount())
      nvgGlobalAlpha(args.vg, .4f);
    MenuItem::draw(args);
    nvgRestore(args.vg);
  }
  void onAction(const event::Action&) override {
    if (!module) return;
    if (channel < 0) {
      if (!module->showingChannelsView()) module->switchEditorView();
    } else {
      module->channelsCursorChannel = channel;
      module->openFocusedChannelEditPage();
    }
  }
};
struct BlunchChannelLabel : phlooper::WarpedDropdown {
  ComputerscareBlunch* module = nullptr;
  WeakPtr<ui::MenuOverlay> activeMenu;
  std::array<std::unique_ptr<widget::SvgWidget>, 17> captions;
  // Keep complete paths explicit so packaged-asset validation checks every
  // caption.
  std::string captionPath(int channel) {
    switch (channel) {
      case 0:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-0.svg");
      case 1:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-1.svg");
      case 2:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-2.svg");
      case 3:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-3.svg");
      case 4:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-4.svg");
      case 5:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-5.svg");
      case 6:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-6.svg");
      case 7:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-7.svg");
      case 8:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-8.svg");
      case 9:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-9.svg");
      case 10:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-10.svg");
      case 11:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-11.svg");
      case 12:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-12.svg");
      case 13:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-13.svg");
      case 14:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-14.svg");
      case 15:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-15.svg");
      case 16:
        return asset::plugin(pluginInstance,
                             "res/components/blunch-channels/channel-16.svg");
    }
    return "";
  }
  std::string label() override { return ""; }
  bool isPressed() override {
    auto* overlay = activeMenu.get();
    return overlay && !overlay->requestedDelete;
  }
  void draw(const DrawArgs& args) override {
    configure(54.f, 25.f, 2.5f, 1.5f, .7f, 105);
    faceColor = nvgRGB(215, 215, 215);
    textSize = 10;
    phlooper::WarpedDropdown::draw(args);
    int channel = module && module->showingChannelsView() ? 0
                  : module ? module->focusedChannel + 1
                           : 1;
    channel = clamp(channel, 0, 16);
    if (!captions[channel]) {
      captions[channel].reset(new widget::SvgWidget);
      captions[channel]->setSvg(APP->window->loadSvg(captionPath(channel)));
    }
    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, 3, 3, 50, 19);
    nvgTranslate(args.vg, box.size.x - 7.f - captions[channel]->box.size.x,
                 (box.size.y - captions[channel]->box.size.y) * .5f +
                     (isPressed() ? 1.f : -1.f));
    captions[channel]->draw(args);
    nvgRestore(args.vg);
  }
  void onButton(const event::Button& e) override {
    if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS)
      return;
    auto* menu = createMenu();
    activeMenu = menu->getAncestorOfType<ui::MenuOverlay>();
    for (int channel = -1; channel < 16; ++channel) {
      auto* item = new BlunchChannelChoiceItem;
      item->module = module;
      item->channel = channel;
      item->text =
          channel < 0 ? "All" : "Channel " + std::to_string(channel + 1);
      menu->addChild(item);
    }
    e.consume(this);
  }
};

// Editing/control mode selector.
struct BlunchEditControlToggle : phlooper::WarpedDropdown {
  ComputerscareBlunch* blunch = nullptr;
  std::function<void()> changed;
  bool control() const { return blunch && blunch->controlMode; }
  widget::SvgWidget captions[2];
  BlunchEditControlToggle() {
    captions[0].setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/blunch-channels/edit.svg")));
    captions[1].setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/blunch-channels/ctrl.svg")));
  }
  std::string label() override { return ""; }
  bool isPressed() override { return control(); }
  void draw(const DrawArgs& args) override {
    configure(28.f, 12.f, 1.5f, 1.f, .7f, 106);
    faceColor = nvgRGB(205, 205, 205);
    phlooper::WarpedDropdown::draw(args);
    auto& caption = captions[control() ? 1 : 0];
    nvgSave(args.vg);
    nvgTranslate(args.vg, (box.size.x - caption.box.size.x * .8f) * .5f - .5f,
                 (box.size.y - caption.box.size.y * .8f) * .5f +
                     (control() ? .5f : -.5f));
    nvgScale(args.vg, .8f, .8f);
    caption.draw(args);
    nvgRestore(args.vg);
  }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      if (blunch) {
        blunch->setControlMode(!blunch->controlMode);
        if (changed) changed();
      }
      e.consume(this);
    }
  }
};

struct BlunchChannelTransport : phlooper::WarpedDropdown {
  ComputerscareBlunch* blunch = nullptr;
  bool stop = false, cancel = false, pressed = false;
  widget::SvgWidget icons[2];
  WeakPtr<ui::Tooltip> hoverTooltip;
  BlunchChannelTransport() {
    icons[0].setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/components/blunch-play.svg")));
    icons[1].setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/components/blunch-stop.svg")));
  }
  void destroyTooltip() {
    if (auto* tooltip = hoverTooltip.get()) tooltip->requestDelete();
    hoverTooltip = nullptr;
  }
  ~BlunchChannelTransport() override { destroyTooltip(); }
  std::string tooltipText() const {
    if (!blunch) return "";
    int ch = channel();
    if (cancel)
      return pending()
                 ? string::f("Discard pending changes for channel %d", ch + 1)
                 : string::f("Channel %d: no pending changes", ch + 1);
    if (!stop && pending())
      return string::f("Apply pending changes to channel %d", ch + 1);
    return stop ? string::f("Stop channel %d", ch + 1)
                : string::f("Channel %d: %s", ch + 1,
                            blunch->channelPlaying(ch) ? "playing" : "stopped");
  }
  void onEnter(const EnterEvent& e) override {
    phlooper::WarpedDropdown::onEnter(e);
    if (!blunch || !settings::tooltips || hoverTooltip.get()) return;
    auto* tooltip = new ui::Tooltip;
    tooltip->text = tooltipText();
    APP->scene->addChild(tooltip);
    hoverTooltip = tooltip;
  }
  void step() override {
    phlooper::WarpedDropdown::step();
    if (auto* tooltip = hoverTooltip.get()) tooltip->text = tooltipText();
  }
  int channel() const {
    return blunch->showingChannelsView() ? blunch->channelsCursorChannel
                                         : blunch->focusedChannel;
  }
  bool pending() const {
    return blunch && blunch->channelHasPendingFormula(channel());
  }
  std::string label() override { return cancel ? "x" : ""; }
  bool isPressed() override {
    if (!stop && !cancel && pending()) return false;
    return pressed ||
           (!stop && !cancel && blunch && blunch->channelPlaying(channel()));
  }
  void draw(const DrawArgs& args) override {
    configure(cancel ? 11.f : 14.f, 13.f, 2.5f, 1.25f, .6f,
              cancel ? 112
              : stop ? 111
                     : 110);
    textSize = 11.f;
    faceColor = cancel ? nvgRGB(190, 190, 190) : COLOR_COMPUTERSCARE_GREEN;
    if (!stop && !cancel && pending())
      faceColor = nvgLerpRGBA(COLOR_COMPUTERSCARE_GREEN, nvgRGB(255, 238, 154),
                              blunchPendingPulse());
    phlooper::WarpedDropdown::draw(args);
    if (cancel) return;
    float shift = isPressed() ? upDepth + downDepth : 0.f;
    nvgSave(args.vg);
    nvgTranslate(args.vg, (stop ? 3.f : 2.5f) + shift, 2.f + shift * .65f);
    icons[stop ? 1 : 0].draw(args);
    nvgRestore(args.vg);
  }
  void onButton(const ButtonEvent& e) override {
    if (!blunch || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (e.action == GLFW_PRESS) {
      pressed = true;
      unsigned bit = 1u << channel();
      if (cancel)
        blunch->discardChannelPending(channel());
      else if (stop)
        blunch->controlStopRequests.fetch_or(bit);
      else if (pending()) {
        int target = channel();
        int line = blunch->pendingFormulas[target].line;
        if (line < 0)
          line = blunch->showingChannelsView()
                     ? blunch->channelOverviewLine(target)
                     : blunch->selectedLine;
        int previous = blunch->focusedChannel;
        blunch->focusedChannel = target;
        if (blunch->executeLine(line, true)) blunch->startSequencer(target);
        blunch->focusedChannel = previous;
      } else
        blunch->controlToggleRequests.fetch_or(bit);
    } else if (e.action == GLFW_RELEASE)
      pressed = false;
    e.consume(this);
  }
  void onDragEnd(const DragEndEvent&) override { pressed = false; }
  void onLeave(const LeaveEvent& e) override {
    pressed = false;
    destroyTooltip();
    phlooper::WarpedDropdown::onLeave(e);
  }
};

struct BlunchSquareFace : phlooper::WarpedDropdown {
  std::string caption;
  bool down = false;
  std::string label() override { return caption; }
  bool isPressed() override { return down; }
};
struct BlunchManualReset : widget::OpaqueWidget {
  ComputerscareBlunch* module = nullptr;
  unsigned mask = 0;
  BlunchSquareFace face;
  WeakPtr<ui::Tooltip> hoverTooltip;
  void destroyHoverTooltip() {
    if (auto* tooltip = hoverTooltip.get()) tooltip->requestDelete();
    hoverTooltip = nullptr;
  }
  ~BlunchManualReset() override { destroyHoverTooltip(); }
  void onEnter(const event::Enter& e) override {
    widget::OpaqueWidget::onEnter(e);
    if (!settings::tooltips || hoverTooltip.get()) return;
    auto* tooltip = new ui::Tooltip;
    tooltip->text = "Reset " + face.caption;
    APP->scene->addChild(tooltip);
    hoverTooltip = tooltip;
  }
  void draw(const DrawArgs& args) override { face.draw(args); }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      face.down = true;
      if (module)
        module->manualResets.fetch_or(mask, std::memory_order_relaxed);
      e.consume(this);
    }
  }
  void onDragEnd(const event::DragEnd&) override { face.down = false; }
  void onLeave(const event::Leave& e) override {
    widget::OpaqueWidget::onLeave(e);
    face.down = false;
    destroyHoverTooltip();
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
    BlunchSquareFace face;
    face.configure(box.size.x, box.size.y, 3.f, 2.f, .7f, 71);
    face.caption = label;
    face.down = getParamQuantity() && getParamQuantity()->getValue() > .5f;
    face.faceColor = nvgRGB(36, 201, 166);
    face.textSize = 10.f;
    face.draw(args);
  }
};

struct BlunchModeMenuItem : MenuItem {
  ComputerscareBlunch* module = nullptr;
  int paramId = 0;
  int mode = 0;

  void onAction(const event::Action& e) override {
    if (module) {
      module->params[paramId].setValue(mode);
    }
  }

  void step() override {
    if (module) {
      int currentMode = std::max(
          0, std::min((int)std::round(module->params[paramId].getValue()),
                      paramId == ComputerscareBlunch::WAIT_MODE_PARAM
                          ? BLUNCH_WAIT_MODE_COUNT - 1
                          : BLUNCH_ADVANCE_MODE_COUNT - 1));
      rightText = CHECKMARK(currentMode == mode);
    }
    MenuItem::step();
  }
};

struct BlunchRunInputModeMenuItem : MenuItem {
  ComputerscareBlunch* blunch = nullptr;
  int mode = 0;

  void onAction(const event::Action& e) override {
    if (blunch) {
      blunch->runInputMode = mode;
    }
  }

  void step() override {
    rightText = CHECKMARK(blunch && blunch->runInputMode == mode);
    MenuItem::step();
  }
};

struct BlunchModeMenuButton : ComputerscareBlankButton {
  static constexpr float DRAW_SCALE_X = 0.72f;
  ComputerscareBlunch* blunch = nullptr;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  ui::Tooltip* hoverTooltip = NULL;
  int paramId = ComputerscareBlunch::WAIT_MODE_PARAM;
  std::string label = "Wait";
  std::string menuLabel = "Wait";
  int menuFrame = -1;

  BlunchModeMenuButton() {
    momentary = true;
    box.size.x *= DRAW_SCALE_X;
  }

  ~BlunchModeMenuButton() { destroyHoverTooltip(); }

  int modeCount() const {
    return paramId == ComputerscareBlunch::WAIT_MODE_PARAM
               ? BLUNCH_WAIT_MODE_COUNT
               : BLUNCH_ADVANCE_MODE_COUNT;
  }

  int currentMode() const {
    if (!blunch) {
      return paramId == ComputerscareBlunch::WAIT_MODE_PARAM ? 0 : 1;
    }
    return std::max(
        0, std::min((int)std::round(blunch->params[paramId].getValue()),
                    modeCount() - 1));
  }

  const std::string& modeName(int mode) const {
    if (paramId == ComputerscareBlunch::WAIT_MODE_PARAM) {
      return BLUNCH_WAIT_MODE_NAMES[mode];
    }
    return BLUNCH_ADVANCE_MODE_NAMES[mode];
  }

  const std::string& modeDescription(int mode) const {
    if (paramId == ComputerscareBlunch::WAIT_MODE_PARAM) {
      return BLUNCH_WAIT_MODE_DESCRIPTIONS[mode];
    }
    if (paramId == ComputerscareBlunch::AUTO_BLOCK_ADVANCE_PARAM) {
      return BLUNCH_BLOCK_ADVANCE_MODE_DESCRIPTIONS[mode];
    }
    return BLUNCH_LINE_ADVANCE_MODE_DESCRIPTIONS[mode];
  }

  void createHoverTooltip() {
    if (!settings::tooltips || hoverTooltip) {
      return;
    }

    hoverTooltip = new ui::Tooltip;
    APP->scene->addChild(hoverTooltip);
  }

  void updateHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    int mode = currentMode();
    hoverTooltip->text =
        menuLabel + ":\n" + modeName(mode) + "\n" + modeDescription(mode);
  }

  void destroyHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    APP->scene->removeChild(hoverTooltip);
    delete hoverTooltip;
    hoverTooltip = NULL;
  }

  bool isMenuOpen() {
    ui::MenuOverlay* overlay = activeMenuOverlay.get();
    return overlay && !overlay->requestedDelete;
  }

  void updateMenuFrame() {
    int frame = isMenuOpen() ? 1 : 0;
    if (menuFrame == frame || frame >= (int)frames.size()) {
      return;
    }

    sw->setSvg(frames[frame]);
    fb->setDirty();
    menuFrame = frame;
    setIconPressed(frame == 1);
  }

  void step() override {
    ComputerscareBlankButton::step();
    updateMenuFrame();
    updateHoverTooltip();
  }

  void draw(const DrawArgs& args) override {
    updateMenuFrame();
    BlunchSquareFace face;
    face.configure(box.size.x, box.size.y, 3.f, 2.f, .7f, 72);
    face.caption = label;
    face.down = isMenuOpen();
    face.faceColor = nvgRGB(36, 201, 166);
    face.textSize = 10.f;
    face.draw(args);
  }

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && isMenuOpen()) {
      updateMenuFrame();
      return;
    }

    ComputerscareBlankButton::onDragEnd(e);
  }

  void onEnter(const event::Enter& e) override {
    createHoverTooltip();
    updateHoverTooltip();
    ComputerscareBlankButton::onEnter(e);
  }

  void onLeave(const event::Leave& e) override {
    destroyHoverTooltip();
    ComputerscareBlankButton::onLeave(e);
  }

  void onButton(const event::Button& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS) {
      ComputerscareBlankButton::onButton(e);
      return;
    }

    e.consume(this);
    destroyHoverTooltip();
    if (!blunch) {
      return;
    }

    Menu* menu = createMenu();
    activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
    menu->addChild(createMenuLabel(menuLabel));
    if (paramId != ComputerscareBlunch::WAIT_MODE_PARAM) {
      const int menuOrder[BLUNCH_ADVANCE_MODE_COUNT] = {1, 2, 3, 4, 5, 0};
      for (int i = 0; i < BLUNCH_ADVANCE_MODE_COUNT; i++) {
        int mode = menuOrder[i];
        BlunchModeMenuItem* item = new BlunchModeMenuItem();
        item->text = BLUNCH_ADVANCE_MODE_NAMES[mode];
        item->module = blunch;
        item->paramId = paramId;
        item->mode = mode;
        menu->addChild(item);
      }
      updateMenuFrame();
      return;
    }
    for (int i = 0; i < BLUNCH_WAIT_MODE_COUNT; i++) {
      BlunchModeMenuItem* item = new BlunchModeMenuItem();
      item->text = BLUNCH_WAIT_MODE_NAMES[i];
      item->module = blunch;
      item->paramId = paramId;
      item->mode = i;
      menu->addChild(item);
    }
    updateMenuFrame();
  }
};

struct BlunchPanel : Widget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(222, 222, 222));
    nvgFill(args.vg);
    phlooper::WarpedBlock block;
    for (int i = 0; i < 3; ++i) {
      block.configure(
          Rect(Vec(i == 1 ? 1.f : 4.f, i == 0   ? 3
                                       : i == 1 ? 279
                                                : 335),
               Vec(i == 0 ? 135.f : box.size.x - (i == 1 ? 5.f : 8.f),
                   i == 0   ? 25
                   : i == 1 ? 54
                            : 43)),
          i == 2 ? 4.f : 3.f, i == 2 ? 2.8f : 1.8f, 81 + i,
          nvgRGB(i == 1   ? 232
                 : i == 2 ? 175
                          : 205,
                 i == 1   ? 232
                 : i == 2 ? 175
                          : 205,
                 i == 1   ? 232
                 : i == 2 ? 175
                          : 205));
      nvgSave(args.vg);
      nvgTranslate(args.vg, block.box.pos.x, block.box.pos.y);
      if (i == 0) block.cornerOffsets[1] = Vec(.4f, -2.f);
      if (i == 1) {
        // Mirroring turns this corner into the input frame's upper right.
        block.cornerOffsets[0] = Vec(-.6f, -1.8f);
        nvgTranslate(args.vg, block.box.size.x, 0.f);
        nvgScale(args.vg, -1.f, 1.f);
      }
      block.draw(args);
      nvgRestore(args.vg);
    }
  }
};

// A rack-level light-layer overlay stays above cables without taking mouse
// focus.
struct BlunchShortcutOverlay : widget::TransparentWidget {
  std::weak_ptr<std::function<void(const DrawArgs&)>> drawPopup;
  void step() override {
    if (drawPopup.expired()) requestDelete();
    widget::TransparentWidget::step();
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 3)
      if (auto draw = drawPopup.lock()) (*draw)(args);
  }
};
struct BlunchLineHeightSlider : MenuParamSlider {
  ComputerscareBlunch* blunch;
  BlunchLineHeightSlider(ComputerscareBlunch* owner)
      : MenuParamSlider(owner->paramQuantities
                            [ComputerscareBlunch::EDITOR_LINE_HEIGHT_PARAM]),
        blunch(owner) {}
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    if (blunch->editorFitLines) nvgGlobalAlpha(args.vg, .35f);
    MenuParamSlider::draw(args);
    nvgRestore(args.vg);
  }
  void onDragStart(const DragStartEvent& e) override {
    if (!blunch->editorFitLines)
      MenuParamSlider::onDragStart(e);
    else
      e.consume(this);
  }
  void onDragMove(const DragMoveEvent& e) override {
    if (!blunch->editorFitLines) MenuParamSlider::onDragMove(e);
  }
  void onDoubleClick(const DoubleClickEvent& e) override {
    if (!blunch->editorFitLines)
      MenuParamSlider::onDoubleClick(e);
    else
      e.consume(this);
  }
};

struct ComputerscareBlunchWidget : ModuleWidget {
  bool controlFocusRequested = false;
  bool controlFocused() const {
    if (!inControlMode()) return false;
    auto* selected = APP->event->getSelectedWidget();
    for (auto* current = selected; current; current = current->parent)
      if (current == this) return true;
    return false;
  }
  void onButton(const ButtonEvent& e) override {
    ModuleWidget::onButton(e);
    if (inControlMode() && e.button == GLFW_MOUSE_BUTTON_LEFT &&
        e.action == GLFW_PRESS)
      controlFocusRequested = true;
  }
  bool inControlMode() const {
    auto* blunch = dynamic_cast<ComputerscareBlunch*>(module);
    return blunch && blunch->controlMode;
  }
  std::shared_ptr<std::function<void(const DrawArgs&)>> shortcutDraw;
  void drawShortcutPopup(const DrawArgs& args) {
    if (args.fb || !module || !blunchShortcutHelpHeld(inControlMode())) return;
    auto belongs = [this](widget::Widget* widget) {
      for (auto* current = widget; current; current = current->parent)
        if (current == this) return true;
      return false;
    };
    if (!belongs(APP->event->getHoveredWidget()) &&
        !belongs(APP->event->getSelectedWidget()) &&
        !belongs(APP->event->getDraggedWidget()) &&
        !APP->scene->rack->isSelected(this))
      return;
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    const char* editKeys[] = {
        "[Ctrl/Cmd+Enter]",  "[Enter]",           "[Esc]",
        "[Tab / Shift+Tab]", "[Ctrl+[ / Ctrl+]]", "[Ctrl+,]",
        "[Ctrl+/]",          "[Ctrl+.]",          "[Ctrl+Space]"};
    const char* editActions[] = {
        "Submit / start line", "All: open channel",  "Cancel / back to All",
        "All / channel view",  "Previous / next ch", "Stop channel",
        "Toggle Run",          "Stop all",           "Start all"};
    const char* controlKeys[] = {
        "[Up / Down]",      "[Shift+Up/Down]", "[Ctrl/Cmd+Enter]",
        "[Ctrl/Cmd+click]", "[Space]",         "[R]",
        "[Enter]",          "[Tab]",           "[Esc]"};
    const char* controlActions[] = {
        "Move focus",         "Select range",           "Toggle row",
        "Toggle clicked row", "Start / stop selection", "Reset selected pages",
        "Open channel",       "All / channel view",     "Back to All"};
    const char** keys = inControlMode() ? controlKeys : editKeys;
    const char** actions = inControlMode() ? controlActions : editActions;
    const int count = 9;
    float width = std::min(288.f, box.size.x - 12.f);
    float x = (box.size.x - width) * .5f, y = 43.f;
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, y, width, 16.f + count * 14.f, 5.f);
    nvgFillColor(args.vg, nvgRGBA(16, 27, 25, 245));
    nvgFill(args.vg);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontSize(args.vg, 10.f);
    for (int i = 0; i < count; ++i) {
      float row = y + 15.f + i * 14.f;
      nvgFillColor(args.vg, nvgRGB(95, 220, 181));
      nvgText(args.vg, x + 9.f, row, keys[i], nullptr);
      nvgFillColor(args.vg, nvgRGB(235, 240, 235));
      nvgText(args.vg, x + 116.f, row, actions[i], nullptr);
    }
    nvgRestore(args.vg);
  }
  BlunchPanel* panel = nullptr;
  ComputerscareSVGPanel* artwork = nullptr;
  BlunchTitle* title = nullptr;
  BlunchSpeedOfTimeLabel* speedOfTimeLabel = nullptr;
  BlunchSeedLabel* seedLabel = nullptr;
  BlunchChannelLabel* channelLabel = nullptr;
  BlunchEditControlToggle* editControlToggle = nullptr;
  BlunchChannelTransport* channelPlay = nullptr;
  BlunchChannelTransport* channelStop = nullptr;
  BlunchChannelTransport* channelCancel = nullptr;
  PolyOutputChannelsWidget* polyChannelsWidget = nullptr;
  BlunchExternalClockLabels* externalClockLabels = nullptr;
  ComputerscareTextEditor* editor = nullptr;
  PortWidget* externalClockWInput = nullptr;
  PortWidget* externalClockXInput = nullptr;
  PortWidget* externalClockYInput = nullptr;
  PortWidget* externalClockZInput = nullptr;
  PortWidget* resetBlockInput = nullptr;
  PortWidget* resetLineInput = nullptr;
  PortWidget* resetSequenceInput = nullptr;
  PortWidget* runInput = nullptr;
  PortWidget* clockOutput = nullptr;
  PortWidget* phaseOutput = nullptr;
  PortWidget* eoc1Output = nullptr;
  PortWidget* eoc2Output = nullptr;
  PortWidget* eoc3Output = nullptr;
  ComputerscareResizeHandle* leftHandle = nullptr;
  ComputerscareResizeHandle* rightHandle = nullptr;
  ComputerscareTextEditorState browserEditorState;
  int lastCursorLine = -1;
  int lastChannelsIndicatorChannel = -1;
  int channelsIndicatorChannel = 0;
  double channelsIndicatorStartTime = -10.0;
  bool wasShowingChannelsView = false;
  int lastChannelsEditorCursor = -1;
  std::string lastChannelsEditorText;

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

    // Match Phlooper’s cached/oversampled rendering for small SVG lettering.
    artwork = new ComputerscareSVGPanel;
    artwork->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareBlunchPanel.svg")));
    addChild(artwork);
    title = new BlunchTitle();
    title->box.pos = Vec(0.f, 1.f);
    title->box.size = Vec(box.size.x, 18.f);
    title->visible = false;
    addChild(title);

    speedOfTimeLabel = new BlunchSpeedOfTimeLabel();
    speedOfTimeLabel->box.pos = Vec(4.f, 4.f);
    speedOfTimeLabel->box.size = Vec(28.f, 9.f);
    speedOfTimeLabel->visible = false;
    addChild(speedOfTimeLabel);

    seedLabel = new BlunchSeedLabel();
    seedLabel->box.pos = Vec(101.f, 4.f);
    seedLabel->box.size = Vec(24.f, 9.f);
    seedLabel->visible = false;
    addChild(seedLabel);

    editControlToggle = new BlunchEditControlToggle;
    editControlToggle->blunch = module;
    editControlToggle->box = Rect(Vec(box.size.x - 93.f, 1.f), Vec(28.f, 12.f));
    addChild(editControlToggle);
    channelPlay = new BlunchChannelTransport;
    channelStop = new BlunchChannelTransport;
    channelCancel = new BlunchChannelTransport;
    channelPlay->blunch = channelStop->blunch = channelCancel->blunch = module;
    channelCancel->cancel = true;
    channelCancel->box.size = Vec(11.f, 13.f);
    channelStop->stop = true;
    channelPlay->box.size = channelStop->box.size = Vec(14.f, 13.f);
    addChild(channelPlay);
    addChild(channelStop);
    addChild(channelCancel);
    channelLabel = new BlunchChannelLabel();
    channelLabel->module = module;
    channelLabel->box.pos = Vec(box.size.x - 57.f, 3.f);
    channelLabel->box.size = Vec(54.f, 25.f);
    addChild(channelLabel);

    addParam(createParam<SmallKnob>(Vec(46.f, 283.f), module,
                                    ComputerscareBlunch::SPEED_OF_TIME_PARAM));
    addParam(createParam<SmallKnob>(Vec(76.f, 283.f), module,
                                    ComputerscareBlunch::SEED_PARAM));
    addInput(createInput<JackSmallPointingDown>(
        Vec(47.f, 312.f), module, ComputerscareBlunch::TIME_CV_INPUT));
    addInput(createInput<JackSmallPointingDown>(
        Vec(77.f, 312.f), module, ComputerscareBlunch::SEED_CV_INPUT));

    externalClockLabels = new BlunchExternalClockLabels();
    externalClockLabels->box.pos = Vec(0.f, 282.f);
    externalClockLabels->box.size = Vec(box.size.x, 12.f);
    externalClockLabels->visible = false;
    addChild(externalClockLabels);

    auto* blunchEditor = createWidget<BlunchTextEditor>(Vec(7.f, 30.f));
    blunchEditor->module = module;
    editor = blunchEditor;
    editControlToggle->changed = [this]() {
      APP->event->setSelectedWidget(editor);
    };
    editor->box.size = Vec(box.size.x - 10.f, 243.f);
    editor->placeholder = "type a sequence...";
    editor->style.cornerRadius = 0.f;
    editor->style.rowTopPadding = 0.f;
    editor->style.fullWidthRows = true;
    editor->style.backgroundColor = nvgRGB(0x05, 0x06, 0x08);
    editor->style.drawBorder = false;
    editor->style.textColor = nvgRGB(0xee, 0xee, 0xea);
    editor->style.selectionColor = nvgRGBA(0x24, 0xc9, 0xa6, 0x85);
    editor->style.placeholderColor = nvgRGBA(0xee, 0xee, 0xea, 0x66);
    editor->submitOnEnter = true;
    if (module) {
      editor->setState(&module->sequenceEditorState());
    } else {
      browserEditorState.text = "120bpm\n[300 400]ms";
      editor->setState(&browserEditorState);
    }
    addChild(editor);

    BlunchAdvanceModeButton* runButton = createParam<BlunchAdvanceModeButton>(
        Vec(5.5f, 283.f), module, ComputerscareBlunch::RUN_PARAM);
    runButton->box.size = Vec(32, 18);
    runButton->label = "Run";
    addParam(runButton);

    polyChannelsWidget = new PolyOutputChannelsWidget(
        Vec(198.f, 340.f), module, ComputerscareBlunch::POLY_CHANNELS_PARAM);
    addChild(polyChannelsWidget);

    externalClockWInput = createInput<JackSmallPointingUp>(
        Vec(8.f, 290.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_W_INPUT);
    externalClockXInput = createInput<JackSmallPointingUp>(
        Vec(38.f, 290.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_X_INPUT);
    externalClockYInput = createInput<JackSmallPointingUp>(
        Vec(68.f, 290.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_Y_INPUT);
    externalClockZInput = createInput<JackSmallPointingUp>(
        Vec(98.f, 290.f), module, ComputerscareBlunch::EXTERNAL_CLOCK_Z_INPUT);
    addInput(externalClockWInput);
    addInput(externalClockXInput);
    addInput(externalClockYInput);
    addInput(externalClockZInput);

    resetBlockInput = createInput<JackLargePointingDown>(
        Vec(38.f, 335.f), module, ComputerscareBlunch::RESET_BLOCK_INPUT);
    resetLineInput = createInput<JackLargePointingDown>(
        Vec(68.f, 335.f), module, ComputerscareBlunch::RESET_LINE_INPUT);
    resetSequenceInput = createInput<JackLargePointingDown>(
        Vec(98.f, 335.f), module, ComputerscareBlunch::RESET_SEQUENCE_INPUT);
    runInput = createInput<JackLargePointingDown>(
        Vec(8.f, 335.f), module, ComputerscareBlunch::RUN_INPUT);
    addInput(runInput);
    addInput(resetBlockInput);
    addInput(resetLineInput);
    addInput(resetSequenceInput);

    clockOutput = createOutput<JackLargePointingUp>(
        Vec(8.f, 356.f), module, ComputerscareBlunch::CLOCK_OUTPUT);
    eoc1Output = createOutput<JackLargePointingUp>(
        Vec(38.f, 356.f), module, ComputerscareBlunch::EOC1_OUTPUT);
    eoc2Output = createOutput<JackLargePointingUp>(
        Vec(68.f, 356.f), module, ComputerscareBlunch::EOC2_OUTPUT);
    eoc3Output = createOutput<JackLargePointingUp>(
        Vec(98.f, 356.f), module, ComputerscareBlunch::EOC3_OUTPUT);
    phaseOutput = createOutput<JackLargePointingUp>(
        Vec(38, 335), module, ComputerscareBlunch::PHASE_OUTPUT);
    addOutput(phaseOutput);
    addOutput(clockOutput);
    addOutput(eoc1Output);
    addOutput(eoc2Output);
    addOutput(eoc3Output);
    for (int i = 0; i < 3; ++i) {
      auto* reset = new BlunchManualReset;
      reset->module = module;
      reset->mask = 1u << i;
      reset->box = Rect(Vec(103.5f + i * 30.f, 283), Vec(28, 18));
      reset->face.configure(28, 18, 3.f, 2.f, .7f, 91 + i);
      reset->face.caption = i == 0 ? "Block" : i == 1 ? "Line" : "Page";
      reset->face.textSize = 10;
      reset->face.faceColor = nvgRGB(36, 201, 166);
      addChild(reset);
    }
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
    if (seedLabel) {
      seedLabel->box.pos = Vec(101.f, 4.f);
    }
    if (editControlToggle)
      editControlToggle->box.pos = Vec(box.size.x - 93.f, 1.f);
    if (channelPlay) channelPlay->box.pos = Vec(box.size.x - 100.f, 15.f);
    if (channelStop) channelStop->box.pos = Vec(box.size.x - 85.f, 15.f);
    if (channelCancel) channelCancel->box.pos = Vec(box.size.x - 70.f, 15.f);
    if (channelLabel) {
      channelLabel->box.pos = Vec(box.size.x - 57.f, 3.f);
      channelLabel->box.size = Vec(54.f, 25.f);
    }
    if (externalClockLabels) {
      externalClockLabels->box.size.x = box.size.x;
    }
    if (editor) {
      editor->box.pos = Vec(7, 30);
      editor->box.size = Vec(box.size.x - 10.f, 243);
    }
    if (polyChannelsWidget) {
      polyChannelsWidget->box.pos = Vec(0.f, 0.f);
      if (polyChannelsWidget->channelCountDisplay) {
        polyChannelsWidget->channelCountDisplay->box.pos = Vec(198.f, 340.f);
      }
      if (polyChannelsWidget->channelsKnob) {
        polyChannelsWidget->channelsKnob->box.pos = Vec(205.f, 343.f);
      }
    }
    if (rightHandle) {
      rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    }

    const float resetInputY = 298.f;
    const float outputY = 338.f;
    PortWidget* externalInputs[] = {externalClockWInput, externalClockXInput,
                                    externalClockYInput, externalClockZInput};
    PortWidget* resetInputs[] = {resetBlockInput, resetLineInput,
                                 resetSequenceInput};
    PortWidget* outputs[] = {clockOutput, phaseOutput, eoc1Output, eoc2Output,
                             eoc3Output};
    const float jackXs[] = {8.f, 38.f, 104.f, 134.f, 164.f};
    for (int i = 0; i < 4; i++) {
      if (externalInputs[i]) {
        externalInputs[i]->box.pos =
            Vec(196.f + (i % 2) * 18.f, 284.f + (i / 2) * 18.f);
      }
    }
    for (int i = 0; i < 5; ++i) outputs[i]->box.pos = Vec(jackXs[i], outputY);
    for (int i = 0; i < 3; i++) {
      if (resetInputs[i]) {
        resetInputs[i]->box.pos = Vec(
            117.5f + i * 30.f - resetInputs[i]->box.size.x * .5f, resetInputY);
      }
    }
    if (runInput) {
      runInput->box.pos = Vec(21.5f - runInput->box.size.x * .5f, resetInputY);
    }
  }

  void step() override {
    if (module && parent && !shortcutDraw) {
      shortcutDraw = std::make_shared<std::function<void(const DrawArgs&)>>(
          [this](const DrawArgs& args) {
            nvgSave(args.vg);
            Vec offset = getRelativeOffset(Vec(), APP->scene->rack);
            nvgTranslate(args.vg, offset.x, offset.y);
            drawShortcutPopup(args);
            nvgRestore(args.vg);
          });
      auto* overlay = new BlunchShortcutOverlay;
      overlay->drawPopup = shortcutDraw;
      APP->scene->rack->addChild(overlay);
    }
    ModuleWidget::step();
    if (controlFocusRequested) {
      bool menuOpen = false;
      for (auto* child : APP->scene->children)
        if (auto* overlay = dynamic_cast<ui::MenuOverlay*>(child))
          menuOpen |= !overlay->requestedDelete;
      // Rack assigns focus to the clicked child after its Button event.
      // Transfer it on the next step, after normal knob/button/cable handling.
      // Menus retain their own keyboard focus until they close.
      if (!menuOpen) {
        if (inControlMode() && editor) APP->event->setSelectedWidget(editor);
        controlFocusRequested = false;
      }
    }
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
        if (blunch->controlMode && blunch->showingSequenceView() &&
            blunch->controlSelection.focus != blunch->focusedChannel)
          blunch->controlSelection.moveTo(blunch->focusedChannel, false);
        bool editorHasChannelsState =
            editor->state == &blunch->channelsEditorState;
        bool channelsCursorMovedInSameText =
            blunch->showingChannelsView() && editorHasChannelsState &&
            lastChannelsEditorCursor >= 0 &&
            editor->cursor != lastChannelsEditorCursor &&
            editor->text == lastChannelsEditorText;

        editor->style.fontSize =
            blunch->params[ComputerscareBlunch::EDITOR_FONT_SIZE_PARAM]
                .getValue();
        editor->style.fontWidthOffset =
            blunch->params[ComputerscareBlunch::EDITOR_FONT_WIDTH_PARAM]
                .getValue();
        editor->style.fontHeightOffset =
            blunch->params[ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM]
                .getValue() +
            BLUNCH_FONT_HEIGHT_BASE;
        editor->style.letterSpacing =
            blunch->params[ComputerscareBlunch::EDITOR_LETTER_SPACING_PARAM]
                .getValue();
        editor->style.lineHeight =
            blunch->editorFitLines ? editor->box.size.y / 16.f : 0.f;
        editor->style.lineHeightScale =
            blunch->editorFitLines
                ? 1.f
                : 1.f + blunch
                            ->params
                                [ComputerscareBlunch::EDITOR_LINE_HEIGHT_PARAM]
                            .getValue();
        editor->style.lineWrapping =
            blunch->showingSequenceView() && blunch->editorLineWrapping;
        editor->openOnEnter = blunch->showingChannelsView();
        editor->stopShortcutEnabled = true;
        editor->readOnly =
            blunch->controlMode ||
            (blunch->showingChannelsView() &&
             !blunch->channelsEditorEditingEnabled(editor->getCursorLine()));
        editor->hideCursor = editor->readOnly;
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
          int forwardDelta = editor->commands.navigateChannelForwardCount -
                             blunch->lastNavigateChannelForwardCount;
          int backwardDelta = editor->commands.navigateChannelBackwardCount -
                              blunch->lastNavigateChannelBackwardCount;
          blunch->lastNavigateChannelCount =
              editor->commands.navigateChannelCount();
          blunch->lastNavigateChannelForwardCount =
              editor->commands.navigateChannelForwardCount;
          blunch->lastNavigateChannelBackwardCount =
              editor->commands.navigateChannelBackwardCount;
          if (blunch->showingSequenceView()) {
            int direction = forwardDelta > backwardDelta ? 1 : -1;
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
        if (editor->commands.runToggleCount != blunch->lastRunToggleCount) {
          blunch->lastRunToggleCount = editor->commands.runToggleCount;
          blunch->toggleRunParam();
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
            blunch->stopSequencer(blunch->channelsCursorChannel, true);
            blunch->refreshChannelsEditorState();
          } else {
            blunch->stopSequencer(blunch->focusedChannel, true);
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
          bool committed = blunch->executeLine(
              blunch->channelOverviewLine(blunch->focusedChannel), true);
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
          } else if (editorHasChannelsState) {
            blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
            if (channelsCursorMovedInSameText) {
              blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                  editor->getCursorLine(),
                  ComputerscareBlunch::MAX_POLY_CHANNELS);
            }
            blunch->syncChannelsEditorEdit(blunch->channelsCursorChannel);
          }
          if (editorHasChannelsState) {
            blunch->channelsEditorState.cursor = editor->cursor;
            blunch->channelsEditorState.selection = editor->selection;
          }
          blunch->refreshChannelsEditorState(true);
        }
        if (blunch->showingChannelsView()) {
          if (!wasShowingChannelsView ||
              blunch->channelsCursorChannel != lastChannelsIndicatorChannel) {
            channelsIndicatorChannel = blunch->channelsCursorChannel;
            channelsIndicatorStartTime = rack::system::getTime();
          }
          lastChannelsIndicatorChannel = blunch->channelsCursorChannel;
          wasShowingChannelsView = true;
        } else {
          wasShowingChannelsView = false;
          lastChannelsIndicatorChannel = -1;
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
            bool committed = blunch->executeLine(blunch->selectedLine, true);
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
          blunch->lastRunToggleCount = editor->commands.runToggleCount;
          if (channelsCursorMovedInSameText) {
            blunch->channelsCursorChannel = blunchChannelForChannelsViewLine(
                editor->getCursorLine(),
                ComputerscareBlunch::MAX_POLY_CHANNELS);
          }
        }
        const BlunchSequencerRuntime& visibleSequencer =
            blunch->showingChannelsView()
                ? blunch->sequencerForChannel(blunch->channelsCursorChannel)
                : blunch->activeSequencer();
        int visibleChannel = blunch->showingChannelsView()
                                 ? blunch->channelsCursorChannel
                                 : blunch->focusedChannel;
        bool playing = blunch->channelPlaying(visibleChannel);
        blinkHigh = playing && visibleSequencer.activeClockOutputHigh;
        activeProgress = playing ? visibleSequencer.activeClockRamp : 0.f;
        editor->readOnly =
            blunch->controlMode ||
            (blunch->showingChannelsView() &&
             !blunch->channelsEditorEditingEnabled(editor->getCursorLine()));
        editor->hideCursor = editor->readOnly;
        if (blunch->showingChannelsView() &&
            editor->state == &blunch->channelsEditorState) {
          lastChannelsEditorCursor = editor->cursor;
          lastChannelsEditorText = editor->text;
        } else {
          lastChannelsEditorCursor = -1;
          lastChannelsEditorText.clear();
        }
      } else {
        editor->openOnEnter = false;
        editor->stopShortcutEnabled = false;
        editor->readOnly = false;
        editor->hideCursor = false;
        editor->style.fontSize = 14.f;
        editor->style.fontHeightOffset = BLUNCH_FONT_HEIGHT_BASE;
        editor->style.lineHeight = editor->box.size.y / 16.f;
        editor->style.lineHeightScale = 1.f;
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
      if (blunch && blunch->showingChannelsView()) {
        int activeCount = blunch->selectedOutputChannelCount();
        for (int channel = activeCount;
             channel < ComputerscareBlunch::MAX_POLY_CHANNELS; ++channel) {
          BlunchLineInfo row = getLineInfo(state->text, channel);
          ComputerscareTextHighlight inactive;
          inactive.begin = row.begin;
          inactive.end = row.end;
          inactive.fullLine = true;
          inactive.italic = true;
          inactive.textOpacity = .65f;
          inactive.background =
              channel % 2 ? nvgRGB(0x33, 0x35, 0x36) : nvgRGB(0x2b, 0x2d, 0x2e);
          state->highlights.push_back(inactive);
        }
      }
      if (blunch && blunch->controlMode && blunch->showingChannelsView()) {
        for (int ch = 0; ch < ComputerscareBlunch::MAX_POLY_CHANNELS; ++ch) {
          if (!(blunch->controlSelection.targets() & (1u << ch))) continue;
          BlunchLineInfo row = getLineInfo(state->text, ch);
          ComputerscareTextHighlight selected;
          selected.begin = row.begin;
          selected.end = row.end;
          selected.fullLine = true;
          selected.background = nvgRGBA(36, 201, 166, 58);
          state->highlights.push_back(selected);
        }
      }
      int line = editor->getCursorLine();
      BlunchLineInfo lineInfo = getLineInfo(state->text, line);
      ComputerscareTextHighlight focusHighlight;
      focusHighlight.begin = lineInfo.begin;
      focusHighlight.end = lineInfo.end;
      focusHighlight.fullLine = true;
      focusHighlight.background = nvgRGBA(0xb8, 0xb8, 0xb8, 0x42);
      if (blunch && blunch->controlMode) {
        focusHighlight.background = nvgRGBA(255, 238, 154, 28);
        focusHighlight.hasBorder = true;
        focusHighlight.border = nvgRGBA(255, 238, 154, 170);
        focusHighlight.borderWidth = .7f;
        focusHighlight.borderDistortion = .5f;
      }
      state->highlights.push_back(focusHighlight);
      bool showingSequenceView = !blunch || blunch->showingSequenceView();
      bool showingPendingActiveLine =
          blunch && showingSequenceView &&
          blunch->pendingEdit().line == blunch->activeSequencer().activeLine &&
          blunch->pendingEdit().viewing;
      bool showingActiveVersionOfPending =
          blunch && showingSequenceView &&
          blunch->pendingEdit().line == blunch->activeSequencer().activeLine &&
          !blunch->pendingEdit().viewing;
      bool showingInvalidActiveLine =
          blunch && showingSequenceView &&
          blunch->errorLine == blunch->activeSequencer().activeLine;
      if (blunch && showingSequenceView && blunch->pendingEdit().line >= 0 &&
          blunch->pendingEdit().armed && !showingActiveVersionOfPending) {
        BlunchLineInfo pendingLineInfo =
            getLineInfo(state->text, blunch->pendingEdit().line);
        float pendingPulse =
            0.5f + 0.5f * std::sin((float)rack::system::getTime() * 2.2f);
        ComputerscareTextHighlight pendingHighlight;
        pendingHighlight.begin = pendingLineInfo.begin;
        pendingHighlight.end = pendingLineInfo.end;
        pendingHighlight.fullLine = true;
        pendingHighlight.background = nvgRGBA(
            0xb8, 0xb8, 0xb8, (unsigned char)(0x18 + pendingPulse * 0x28));
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
          if (blunch->channelPlaying(channel) &&
              channelLineInfo.text == channelSequencer.activeLineText)
            addBlunchSequencerHighlights(state->highlights, channelSequencer,
                                         sourceLineInfo, channelLineInfo, true,
                                         blunch->playbackDisplayStyle,
                                         blunch->playbackBars);
        }
      } else if (blunch) {
        BlunchLineInfo sourceLineInfo =
            getLineInfo(blunch->sequenceEditorState().text,
                        blunch->activeSequencer().activeLine);
        if (blunch->channelPlaying(blunch->focusedChannel) &&
            !showingPendingActiveLine && !showingInvalidActiveLine) {
          addBlunchSequencerHighlights(
              state->highlights, blunch->activeSequencer(), sourceLineInfo,
              sourceLineInfo, true, blunch->playbackDisplayStyle,
              blunch->playbackBars);
        }
      } else {
        ComputerscareTextHighlight activeHighlight;
        activeHighlight.begin = lineInfo.begin;
        activeHighlight.end = lineInfo.end;
        activeHighlight.hasBackground = blinkHigh;
        activeHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, 0x66);
        activeHighlight.hasBorder = false;
        activeHighlight.backgroundDistortion = 2.f;
        activeHighlight.borderWidth = .8f;
        activeHighlight.borderDistortion = .75f;
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

  void drawLayer(const DrawArgs& args, int layer) override {
    ModuleWidget::drawLayer(args, layer);
    if (layer != 1) {
      return;
    }

    if (!args.fb && controlFocused())
      ComputerscareModifierMouseControl::drawFocusStripe(
          args, Rect(Vec(0.f, 1.f), Vec(box.size.x, box.size.y - 2.f)), .6f);

    ComputerscareBlunch* blunch = dynamic_cast<ComputerscareBlunch*>(module);
    if (!blunch || !editor || !blunch->showingChannelsView()) {
      return;
    }

    double elapsed = rack::system::getTime() - channelsIndicatorStartTime;
    if (elapsed < 0.0 || elapsed > 1.0) {
      return;
    }

    std::shared_ptr<Font> font =
        APP->window->loadFont(asset::system(editor->style.fontPath));
    if (!font || font->handle < 0) {
      font = APP->window->uiFont;
    }
    if (!font || font->handle < 0) {
      return;
    }

    nvgSave(args.vg);
    nvgTranslate(args.vg, editor->box.pos.x, editor->box.pos.y);
    nvgScissor(args.vg, 0.f, 0.f, editor->box.size.x, editor->box.size.y);
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, std::max(6.f, editor->style.fontSize));
    nvgTextLetterSpacing(args.vg, editor->style.letterSpacing);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

    float fontScaleX = std::max(0.5f, 1.f + editor->style.fontWidthOffset);
    float fontScaleY = std::max(0.5f, 1.f + editor->style.fontHeightOffset);
    Vec scaledBox(editor->box.size.x / fontScaleX,
                  editor->box.size.y / fontScaleY);
    nvgScale(args.vg, fontScaleX, fontScaleY);
    ComputerscareTextEditorLayout layout = ComputerscareTextEditorLayout::build(
        args.vg, editor->text, scaledBox, editor->style);
    if (!layout.rows.empty()) {
      int rowIndex = std::max(
          0, std::min(channelsIndicatorChannel, (int)layout.rows.size() - 1));
      const ComputerscareTextEditorLayoutRow& row = layout.rows[rowIndex];
      float alpha = std::max(0.f, std::min(1.f, 1.f - (float)elapsed));
      std::string number = std::to_string(channelsIndicatorChannel + 1);

      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, std::max(14.f, editor->style.fontSize + 7.f));
      nvgTextLetterSpacing(args.vg, 0.f);
      nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      nvgFillColor(args.vg,
                   nvgRGBA(0x24, 0xc9, 0xa6, (unsigned char)(0xee * alpha)));
      nvgText(args.vg, scaledBox.x - 9.f,
              row.top + layout.metrics.lineHeight * 0.5f + .5f, number.c_str(),
              NULL);
    }

    nvgResetScissor(args.vg);
    nvgRestore(args.vg);
  }

  void onHoverKey(const HoverKeyEvent& e) override {
    if ((module && blunchShortcutHelpHeld(inControlMode())) ||
        computerscare::blunch::isShortcutHelpKey(e.key, e.keyName, e.mods)) {
      e.consume(this);
      return;
    }
    auto* blunchControl = dynamic_cast<ComputerscareBlunch*>(module);
    if (!e.isConsumed() && blunchControl && controlFocused() &&
        blunchControl->handleControlKey(e.key, e.mods, e.action)) {
      e.consume(this);
      return;
    }
    if (!e.isConsumed() && editor &&
        APP->event->getSelectedWidget() == editor && !editor->readOnly &&
        (e.mods & RACK_MOD_CTRL) &&
        (e.key == GLFW_KEY_Z || e.key == GLFW_KEY_Y)) {
      if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
        if (e.key == GLFW_KEY_Y || (e.mods & GLFW_MOD_SHIFT))
          editor->redo();
        else
          editor->undo();
      }
      e.consume(this);
      return;
    }
    if (computerscare::blunch::isRunToggleShortcut(e.key, e.keyName, e.mods)) {
      if (e.action == GLFW_PRESS) {
        ComputerscareBlunch* blunch =
            dynamic_cast<ComputerscareBlunch*>(module);
        if (blunch) {
          blunch->toggleRunParam();
        }
      }
      e.consume(this);
      return;
    }
    if (computerscare::blunch::isHardStopShortcut(e.key, e.keyName, e.mods)) {
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

    if (!blunch->randomTemplateError.empty()) {
      menu->addChild(
          createMenuLabel("Random templates: " + blunch->randomTemplateError));
    }
    menu->addChild(new MenuSeparator());
    const int modeParams[] = {ComputerscareBlunch::AUTO_ADVANCE_PARAM,
                              ComputerscareBlunch::AUTO_BLOCK_ADVANCE_PARAM,
                              ComputerscareBlunch::WAIT_MODE_PARAM};
    const char* modeTitles[] = {"Line advance", "Block advance", "Wait"};
    for (int section = 0; section < 3; ++section) {
      int param = modeParams[section];
      bool wait = section == 2;
      int current = int(blunch->params[param].getValue());
      menu->addChild(createSubmenuItem(
          modeTitles[section],
          wait ? BLUNCH_WAIT_MODE_NAMES[current]
               : BLUNCH_ADVANCE_MODE_NAMES[current],
          [blunch, param, wait](Menu* submenu) {
            const int order[] = {1, 2, 3, 4, 5, 0};
            for (int i = 0; i < (wait ? BLUNCH_WAIT_MODE_COUNT
                                      : BLUNCH_ADVANCE_MODE_COUNT);
                 ++i) {
              int mode = wait ? i : order[i];
              auto* item = new BlunchModeMenuItem;
              item->text = wait ? BLUNCH_WAIT_MODE_NAMES[mode]
                                : BLUNCH_ADVANCE_MODE_NAMES[mode];
              item->module = blunch;
              item->paramId = param;
              item->mode = mode;
              submenu->addChild(item);
            }
          }));
    }

    menu->addChild(createSubmenuItem(
        "Run input", BLUNCH_RUN_INPUT_MODE_NAMES[blunch->runInputMode],
        [=](Menu* submenu) {
          for (int i = 0; i < BLUNCH_RUN_INPUT_MODE_COUNT; i++) {
            submenu->addChild(construct<BlunchRunInputModeMenuItem>(
                &MenuItem::text, BLUNCH_RUN_INPUT_MODE_NAMES[i],
                &BlunchRunInputModeMenuItem::blunch, blunch,
                &BlunchRunInputModeMenuItem::mode, i));
          }
        }));
    menu->addChild(createSubmenuItem(
        "Default number unit",
        blunch::language::clockUnitName(blunch->defaultNumberUnit),
        [=](Menu* submenu) {
          const char* units[] = {"bpm", "s", "ms", "hz", "min", "mhz", "khz"};
          for (const char* name : units) {
            auto unit = blunch::language::parseClockUnit(name);
            submenu->addChild(createCheckMenuItem(
                name, "", [=]() { return blunch->defaultNumberUnit == unit; },
                [=]() { blunch->defaultNumberUnit = unit; }));
          }
        }));
    menu->addChild(new MenuSeparator());
    menu->addChild(createSubmenuItem("Visual", "", [=](Menu* visual) {
      const char* styles[] = {"Full", "Minimal", "Text", "Off", "Border"};
      visual->addChild(createSubmenuItem(
          "Blinkers", styles[blunch->playbackDisplayStyle], [=](Menu* submenu) {
            const char* names[] = {"Full", "Minimal", "Text", "Off", "Border"};
            for (int i : {0, 1, 2, 4, 3})
              submenu->addChild(createCheckMenuItem(
                  names[i], "",
                  [=]() { return blunch->playbackDisplayStyle == i; },
                  [=]() { blunch->playbackDisplayStyle = i; }));
          }));
      visual->addChild(
          createBoolPtrMenuItem("Bars", "", &blunch->playbackBars));
    }));
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
      submenu->addChild(new MenuSeparator());
      submenu->addChild(createBoolPtrMenuItem("Steady line height", "",
                                              &blunch->editorFitLines));
      submenu->addChild(new BlunchLineHeightSlider(blunch));
      submenu->addChild(new MenuSeparator());
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
