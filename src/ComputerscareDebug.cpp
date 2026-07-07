#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "Computerscare.hpp"

const int NUM_LINES = 16;

static const char* DebugModeNames[3] = {"Single-Channel", "Internal",
                                        "Polyphonic"};
static const char* DebugClockModeDescriptions[3] = {
    "Use one selected clock input channel", "Update continuously at audio rate",
    "Use each matching clock input channel"};
static const char* DebugInputModeDescriptions[3] = {
    "Read one selected value input channel", "Generate random values",
    "Read each matching value input channel"};
static const char* DebugOutputRangeLabels[8] = {
    "  0v ... +10v", " -5v ...  +5v", "  0v ...  +5v", "  0v ...  +1v",
    " -1v ...  +1v", "-10v ... +10v", " -2v ...  +2v", "  0v ...  +2v"};
static const char* DebugVisualModeNames[3] = {"Text", "Bars", "Text + Bars"};
static const char* DebugBarModeNames[2] = {"Unipolar", "Bipolar"};
static const char* DebugBarModeCodes[2] = {"UNI", "BI"};
static const char* DebugBipolarColorModeNames[3] = {"Different", "Color 1",
                                                    "Color 2"};
static const char* DebugTextModeNames[4] = {"Voltage", "Notes Sharps",
                                            "Notes Flats", "MIDI Note Number"};

static const float DEBUG_DISPLAY_X = 15.f;
static const float DEBUG_DISPLAY_Y = 34.f;
static const float DEBUG_ROW_PITCH = 15.18f;
static const float DEBUG_TEXT_BASELINE_Y = 12.f;
static const float DEBUG_BAR_CENTER_Y = DEBUG_TEXT_BASELINE_Y - 4.5f;
static const float DEBUG_LABEL_X = -4.f;
static const float DEBUG_LABEL_Y =
    DEBUG_DISPLAY_Y + DEBUG_TEXT_BASELINE_Y - 12.f;
static const float DEBUG_LABEL_MENU_X = -1.f;
static const float DEBUG_LABEL_MENU_Y =
    DEBUG_DISPLAY_Y + DEBUG_TEXT_BASELINE_Y - 10.8f;

struct DebugTheme {
  const char* name;
  NVGcolor background;
  NVGcolor text;
  NVGcolor positive;
  NVGcolor negative;
};

static const DebugTheme DebugThemes[] = {
    {"Boldy", nvgRGB(0x10, 0x00, 0x10), nvgRGB(0xC0, 0xE7, 0xDE),
     nvgRGB(0x24, 0x8A, 0x64), nvgRGB(0xC8, 0x24, 0x3A)},
    {"Poloneck", nvgRGB(0x00, 0x06, 0x0d), nvgRGB(0xB9, 0xF1, 0xFF),
     nvgRGB(0xA6, 0xED, 0xFF), nvgRGB(0x24, 0x54, 0xB8)},
    {"Hut", nvgRGB(0x10, 0x0b, 0x05), nvgRGB(0xFF, 0xDF, 0xB5),
     nvgRGB(0xF6, 0xEE, 0xD8), nvgRGB(0xD9, 0x4F, 0x2E)},
    {"Typhus", nvgRGB(0x00, 0x00, 0x00), nvgRGB(0xD0, 0xEA, 0xD7),
     nvgRGB(0x9B, 0xE5, 0x6F), nvgRGB(0x30, 0x8E, 0x73)},
    {"Moss", nvgRGB(0x00, 0x00, 0x00), nvgRGB(0xE6, 0xE6, 0xE6),
     nvgRGB(0x4F, 0x26, 0x83), nvgRGB(0xFF, 0xC6, 0x2F)},
    {"Check", nvgRGB(0x0d, 0x0d, 0x0d), nvgRGB(0xB8, 0xB8, 0xB8),
     nvgRGB(0xFF, 0xFF, 0xFF), nvgRGB(0x67, 0x67, 0x67)},
    {"Gratings", nvgRGB(0x00, 0x00, 0x00), nvgRGB(0xD8, 0xDE, 0xE4),
     nvgRGB(0xA9, 0xB5, 0xC2), nvgRGB(0x58, 0x64, 0x70)},
    {"Ant", nvgRGB(0x00, 0x00, 0x00), nvgRGB(0x78, 0xBE, 0x20),
     nvgRGB(0x82, 0x86, 0x86), nvgRGB(0x23, 0x61, 0x92)},
    {"Santana", nvgRGB(0x00, 0x00, 0x00), nvgRGB(0xD7, 0xBC, 0x7C),
     nvgRGB(0x00, 0x44, 0x86), nvgRGB(0xD3, 0x11, 0x45)}};

static const int DEBUG_THEME_COUNT =
    sizeof(DebugThemes) / sizeof(DebugThemes[0]);

struct ComputerscareDebug;

std::string noModuleStringValue =
    "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
    "000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
    "000000\n+0.000000\n+0.000000\n+0.000000\n";

static std::string formatDebugVoltage(float value) {
  if (std::fabs(value) < 0.0000005f) {
    value = 0.f;
  }

  std::ostringstream stream;
  float magnitude = std::fabs(value);
  stream << (value < 0.f ? "-" : "+") << std::fixed
         << std::setprecision(magnitude >= 10.f ? 5 : 6) << magnitude;
  return stream.str();
}

struct DebugPitchDisplay {
  int nearestSemitone = 0;
  int cents = 0;
  int midiNote = 60;
};

static DebugPitchDisplay getDebugPitchDisplay(float value) {
  float exactSemitone = value * 12.f;
  DebugPitchDisplay pitchDisplay;
  pitchDisplay.nearestSemitone = (int)std::round(exactSemitone);
  pitchDisplay.cents =
      (int)std::round((exactSemitone - pitchDisplay.nearestSemitone) * 100.f);
  if (std::abs(pitchDisplay.cents) <= 1) {
    pitchDisplay.cents = 0;
  }
  pitchDisplay.midiNote = pitchDisplay.nearestSemitone + 60;
  return pitchDisplay;
}

static void appendDebugCents(std::string& text, int cents) {
  if (cents == 0) {
    return;
  }

  int centsPadding = std::max(1, 4 - (int)text.size());
  text += std::string(centsPadding, ' ') +
          string::f("%s %i", cents > 0 ? "+" : "-", std::abs(cents));
}

static std::string formatDebugNote(float value, bool preferFlats) {
  static const char* sharpNoteNames[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                           "F#", "G",  "G#", "A",  "A#", "B"};
  static const char* flatNoteNames[12] = {"C",  "Db", "D",  "Eb", "E",  "F",
                                          "Gb", "G",  "Ab", "A",  "Bb", "B"};

  DebugPitchDisplay pitchDisplay = getDebugPitchDisplay(value);
  int midiNote = pitchDisplay.midiNote;
  int noteIndex = midiNote % 12;
  if (noteIndex < 0) {
    noteIndex += 12;
  }
  int octave = (midiNote - noteIndex) / 12 - 1;

  const char** noteNames = preferFlats ? flatNoteNames : sharpNoteNames;
  std::string note = string::f("%s%i", noteNames[noteIndex], octave);
  appendDebugCents(note, pitchDisplay.cents);
  return note;
}

static std::string formatDebugMidiNote(float value) {
  DebugPitchDisplay pitchDisplay = getDebugPitchDisplay(value);
  std::string note = string::f("%i", pitchDisplay.midiNote);
  appendDebugCents(note, pitchDisplay.cents);
  return note;
}

struct ComputerscareDebug : Module {
  enum ParamIds {
    MANUAL_TRIGGER,
    MANUAL_CLEAR_TRIGGER,
    CLOCK_CHANNEL_FOCUS,
    INPUT_CHANNEL_FOCUS,
    SWITCH_VIEW,
    WHICH_CLOCK,
    TRIGGER_BLINKERS,
    VISUAL_MODE,
    BAR_MODE,
    THEME,
    TEXT_OPACITY,
    BIPOLAR_COLOR_MODE,
    BARS_OPACITY,
    TEXT_MODE,
    NUM_PARAMS
  };
  enum InputIds { VAL_INPUT, TRG_INPUT, CLR_INPUT, NUM_INPUTS };
  enum OutputIds { POLY_OUTPUT, NUM_OUTPUTS };
  enum LightIds { BLINK_LIGHT, NUM_LIGHTS };

  std::string defaultStrValue =
      "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
      "000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
      "000000\n+0.000000\n+0.000000\n+0.000000\n";
  std::string strValue =
      "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
      "000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
      "000000\n+0.000000\n+0.000000\n+0.000000\n";

  float logLines[NUM_LINES] = {0.f};

  int lineCounter = 0;

  int clockChannel = 0;
  int inputChannel = 0;

  int clockMode = 1;
  int inputMode = 2;

  int outputRangeEnum = 0;

  float outputRanges[8][2];
  float clockLabelPulses[NUM_LINES] = {};

  int stepCounter;
  dsp::SchmittTrigger clockTriggers[NUM_LINES];
  dsp::SchmittTrigger clearTrigger;
  dsp::SchmittTrigger manualClockTrigger;
  dsp::SchmittTrigger manualClearTrigger;

  enum clockAndInputModes { SINGLE_MODE, INTERNAL_MODE, POLY_MODE };
  enum VisualMode { VISUAL_TEXT, VISUAL_BARS, VISUAL_TEXT_BARS };
  enum BarMode { BAR_UNIPOLAR, BAR_BIPOLAR };
  enum TextMode {
    TEXT_VOLTAGE,
    TEXT_NOTES_SHARPS,
    TEXT_NOTES_FLATS,
    TEXT_MIDI_NOTE
  };
  enum BipolarColorMode {
    BIPOLAR_COLORS_DIFFERENT,
    BIPOLAR_COLORS_COLOR_1,
    BIPOLAR_COLORS_COLOR_2
  };

  ComputerscareDebug() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configButton(MANUAL_TRIGGER, "Manual Trigger");
    configButton(MANUAL_CLEAR_TRIGGER, "Reset/Clear");
    configSwitch(SWITCH_VIEW, 0.0f, 2.0f, 2.0f, "Input Mode",
                 {"Single-Channel", "Internal", "Polyphonic"});
    configSwitch(WHICH_CLOCK, 0.0f, 2.0f, 1.0f, "Clock Mode",
                 {"Single-Channel", "Internal", "Polyphonic"});
    configSwitch(TRIGGER_BLINKERS, 0.0f, 1.0f, 1.0f, "Clock Blinkers",
                 {"Off", "On"});
    configSwitch(VISUAL_MODE, 0.f, 2.f, VISUAL_TEXT, "Visual Mode",
                 {"Text", "Bars", "Text + Bars"});
    configSwitch(BAR_MODE, 0.f, 1.f, BAR_UNIPOLAR, "Bar Mode",
                 {"Unipolar", "Bipolar"});
    configSwitch(THEME, 0.f, DEBUG_THEME_COUNT - 1, 0.f, "Theme");
    configParam(TEXT_OPACITY, 0.15f, 1.f, 1.f, "Text Opacity");
    configParam(BARS_OPACITY, 0.15f, 1.f, 1.f, "Bars Opacity");
    configSwitch(
        TEXT_MODE, 0.f, 3.f, TEXT_VOLTAGE, "Text",
        {"Voltage", "Notes Sharps", "Notes Flats", "MIDI Note Number"});
    configSwitch(BIPOLAR_COLOR_MODE, 0.f, 2.f, BIPOLAR_COLORS_DIFFERENT,
                 "Bipolar Colors", {"Different", "Color 1", "Color 2"});
    configParam(CLOCK_CHANNEL_FOCUS, 0.f, 15.f, 0.f, "Clock Channel Selector");
    configParam(INPUT_CHANNEL_FOCUS, 0.f, 15.f, 0.f, "Input Channel Selector");

    configInput(VAL_INPUT, "Value");
    configInput(TRG_INPUT, "Clock");
    configInput(CLR_INPUT, "Reset");
    configOutput(POLY_OUTPUT, "Main");

    outputRanges[0][0] = 0.f;
    outputRanges[0][1] = 10.f;
    outputRanges[1][0] = -5.f;
    outputRanges[1][1] = 5.f;
    outputRanges[2][0] = 0.f;
    outputRanges[2][1] = 5.f;
    outputRanges[3][0] = 0.f;
    outputRanges[3][1] = 1.f;
    outputRanges[4][0] = -1.f;
    outputRanges[4][1] = 1.f;
    outputRanges[5][0] = -10.f;
    outputRanges[5][1] = 10.f;
    outputRanges[6][0] = -2.f;
    outputRanges[6][1] = 2.f;
    outputRanges[7][0] = 0.f;
    outputRanges[7][1] = 2.f;

    stepCounter = 0;

    getParamQuantity(SWITCH_VIEW)->randomizeEnabled = false;
    getParamQuantity(WHICH_CLOCK)->randomizeEnabled = false;
    getParamQuantity(TRIGGER_BLINKERS)->randomizeEnabled = false;
    getParamQuantity(VISUAL_MODE)->randomizeEnabled = false;
    getParamQuantity(BAR_MODE)->randomizeEnabled = false;
    getParamQuantity(THEME)->randomizeEnabled = false;
    getParamQuantity(TEXT_OPACITY)->randomizeEnabled = false;
    getParamQuantity(BARS_OPACITY)->randomizeEnabled = false;
    getParamQuantity(TEXT_MODE)->randomizeEnabled = false;
    getParamQuantity(BIPOLAR_COLOR_MODE)->randomizeEnabled = false;
    getParamQuantity(VISUAL_MODE)->resetEnabled = false;
    getParamQuantity(BAR_MODE)->resetEnabled = false;
    getParamQuantity(TEXT_MODE)->resetEnabled = false;
    getParamQuantity(THEME)->resetEnabled = false;
    getParamQuantity(BIPOLAR_COLOR_MODE)->resetEnabled = false;
    getParamQuantity(CLOCK_CHANNEL_FOCUS)->randomizeEnabled = false;
    getParamQuantity(INPUT_CHANNEL_FOCUS)->randomizeEnabled = false;

    randomizeStorage();
  }
  void process(const ProcessArgs& args) override;

  void onRandomize() override {
    if (shouldPreserveStorageOnRandomize()) {
      return;
    }
    randomizeStorage();
  }

  bool shouldPreserveStorageOnRandomize() {
    int currentClockMode = floor(params[WHICH_CLOCK].getValue());
    int currentInputMode = floor(params[SWITCH_VIEW].getValue());
    return currentClockMode == INTERNAL_MODE &&
           inputs[VAL_INPUT].isConnected() &&
           (currentInputMode == INTERNAL_MODE || currentInputMode == POLY_MODE);
  }

  void randomizeStorage() {
    float min = outputRanges[outputRangeEnum][0];
    float max = outputRanges[outputRangeEnum][1];
    float spread = max - min;
    for (int i = 0; i < 16; i++) {
      logLines[i] = min + spread * random::uniform();
    }
  }

  void setClockLabelGate(int channel, bool high) {
    if (channel >= 0 && channel < NUM_LINES) {
      clockLabelPulses[channel] = high ? 1.f : clockLabelPulses[channel];
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();

    json_object_set_new(rootJ, "outputRange", json_integer(outputRangeEnum));
    json_object_set_new(
        rootJ, "triggerBlinkers",
        json_boolean(params[TRIGGER_BLINKERS].getValue() > 0.5f));
    json_object_set_new(rootJ, "visualMode",
                        json_integer((int)params[VISUAL_MODE].getValue()));
    json_object_set_new(rootJ, "barMode",
                        json_integer((int)params[BAR_MODE].getValue()));
    json_object_set_new(rootJ, "theme",
                        json_integer((int)params[THEME].getValue()));
    json_object_set_new(rootJ, "textOpacity",
                        json_real(params[TEXT_OPACITY].getValue()));
    json_object_set_new(rootJ, "barsOpacity",
                        json_real(params[BARS_OPACITY].getValue()));
    json_object_set_new(rootJ, "textMode",
                        json_integer((int)params[TEXT_MODE].getValue()));
    json_object_set_new(
        rootJ, "bipolarColorMode",
        json_integer((int)params[BIPOLAR_COLOR_MODE].getValue()));

    json_t* sequencesJ = json_array();

    for (int i = 0; i < 16; i++) {
      json_t* sequenceJ = json_real(logLines[i]);
      json_array_append_new(sequencesJ, sequenceJ);
    }
    json_object_set_new(rootJ, "lines", sequencesJ);
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    float val = 0.f;

    json_t* outputRangeEnumJ = json_object_get(rootJ, "outputRange");
    if (outputRangeEnumJ) {
      outputRangeEnum = json_integer_value(outputRangeEnumJ);
    }

    json_t* triggerBlinkersJ = json_object_get(rootJ, "triggerBlinkers");
    params[TRIGGER_BLINKERS].setValue(
        triggerBlinkersJ ? (json_boolean_value(triggerBlinkersJ) ? 1.f : 0.f)
                         : 0.f);

    json_t* visualModeJ = json_object_get(rootJ, "visualMode");
    params[VISUAL_MODE].setValue(
        visualModeJ ? math::clamp((int)json_integer_value(visualModeJ), 0, 2)
                    : VISUAL_TEXT);

    json_t* barModeJ = json_object_get(rootJ, "barMode");
    if (barModeJ) {
      params[BAR_MODE].setValue(
          math::clamp((int)json_integer_value(barModeJ), 0, 1));
    }

    json_t* themeJ = json_object_get(rootJ, "theme");
    if (themeJ) {
      params[THEME].setValue(math::clamp((int)json_integer_value(themeJ), 0,
                                         DEBUG_THEME_COUNT - 1));
    }

    json_t* textOpacityJ = json_object_get(rootJ, "textOpacity");
    if (textOpacityJ) {
      params[TEXT_OPACITY].setValue(
          math::clamp((float)json_number_value(textOpacityJ), 0.15f, 1.f));
    }

    json_t* barsOpacityJ = json_object_get(rootJ, "barsOpacity");
    if (barsOpacityJ) {
      params[BARS_OPACITY].setValue(
          math::clamp((float)json_number_value(barsOpacityJ), 0.15f, 1.f));
    }

    json_t* textModeJ = json_object_get(rootJ, "textMode");
    if (textModeJ) {
      params[TEXT_MODE].setValue(
          math::clamp((int)json_integer_value(textModeJ), 0, 3));
    }

    json_t* bipolarColorModeJ = json_object_get(rootJ, "bipolarColorMode");
    if (bipolarColorModeJ) {
      params[BIPOLAR_COLOR_MODE].setValue(
          math::clamp((int)json_integer_value(bipolarColorModeJ), 0, 2));
    }

    json_t* sequencesJ = json_object_get(rootJ, "lines");

    if (sequencesJ) {
      for (int i = 0; i < 16; i++) {
        json_t* sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ) val = json_real_value(sequenceJ);
        logLines[i] = val;
      }
    }
  }
  int setChannelCount() {
    clockMode = floor(params[WHICH_CLOCK].getValue());

    inputMode = floor(params[SWITCH_VIEW].getValue());

    int numInputChannels = inputs[VAL_INPUT].getChannels();
    int numClockChannels = inputs[TRG_INPUT].getChannels();

    bool inputConnected = inputs[VAL_INPUT].isConnected();
    bool clockConnected = inputs[TRG_INPUT].isConnected();

    bool noConnection = !inputConnected && !clockConnected;

    int numOutputChannels = 16;

    if (!noConnection) {
      if (clockMode == SINGLE_MODE) {
        if (inputMode == POLY_MODE) {
          numOutputChannels = numInputChannels;
        }
      } else if (clockMode == INTERNAL_MODE) {
        if (inputMode == POLY_MODE) {
          numOutputChannels = numInputChannels;
          for (int i = 0; i < 16; i++) {
            logLines[i] = inputs[VAL_INPUT].getVoltage(i);
          }
        }
      } else if (clockMode == POLY_MODE) {
        if (inputMode == POLY_MODE) {
          numOutputChannels = std::min(numInputChannels, numClockChannels);
        } else if (inputMode == SINGLE_MODE) {
          numOutputChannels = numClockChannels;
        } else if (inputMode == INTERNAL_MODE) {
          numOutputChannels = numClockChannels;
        }
      }
    }
    outputs[POLY_OUTPUT].setChannels(numOutputChannels);

    return numOutputChannels;
  }
};

struct DebugPreviewState {
  int themeIndex = 0;
  int visualMode = ComputerscareDebug::VISUAL_TEXT;
  int barMode = ComputerscareDebug::BAR_UNIPOLAR;
  int textMode = ComputerscareDebug::TEXT_VOLTAGE;
  int activeChannels = NUM_LINES;
  float values[NUM_LINES] = {};
  std::string text;

  DebugPreviewState() { randomize(); }

  void randomize() {
    themeIndex =
        math::clamp((int)std::floor(random::uniform() * DEBUG_THEME_COUNT), 0,
                    DEBUG_THEME_COUNT - 1);
    visualMode = math::clamp((int)std::floor(random::uniform() * 3.f), 0, 2);
    barMode = random::uniform() < 0.5f ? ComputerscareDebug::BAR_UNIPOLAR
                                       : ComputerscareDebug::BAR_BIPOLAR;
    textMode = math::clamp((int)std::floor(random::uniform() * 4.f), 0, 3);
    activeChannels = math::clamp(
        1 + (int)std::floor(random::uniform() * NUM_LINES), 1, NUM_LINES);

    text.clear();
    for (int i = 0; i < NUM_LINES; i++) {
      values[i] = -10.f + 20.f * random::uniform();
      if (i > 0) {
        text += "\n";
      }
      if (i < activeChannels) {
        if (textMode == ComputerscareDebug::TEXT_VOLTAGE) {
          text += formatDebugVoltage(values[i]);
        } else if (textMode == ComputerscareDebug::TEXT_MIDI_NOTE) {
          text += formatDebugMidiNote(values[i]);
        } else {
          text += formatDebugNote(
              values[i], textMode == ComputerscareDebug::TEXT_NOTES_FLATS);
        }
      }
    }
  }
};

void ComputerscareDebug::process(const ProcessArgs& args) {
  std::string thisVal;

  clockMode = floor(params[WHICH_CLOCK].getValue());

  inputMode = floor(params[SWITCH_VIEW].getValue());

  inputChannel = floor(params[INPUT_CHANNEL_FOCUS].getValue());
  clockChannel = floor(params[CLOCK_CHANNEL_FOCUS].getValue());

  bool inputConnected = inputs[VAL_INPUT].isConnected();

  float min = outputRanges[outputRangeEnum][0];
  float max = outputRanges[outputRangeEnum][1];
  float spread = max - min;
  bool manualClock =
      manualClockTrigger.process(params[MANUAL_TRIGGER].getValue());
  bool triggerBlinkers = params[TRIGGER_BLINKERS].getValue() > 0.5f;

  for (int i = 0; i < NUM_LINES; i++) {
    if (triggerBlinkers) {
      setClockLabelGate(i, inputs[TRG_INPUT].getVoltage(i) >= 1.f);
      clockLabelPulses[i] *= std::max(0.f, 1.f - args.sampleTime * 5.f);
    }
    if (!triggerBlinkers || clockLabelPulses[i] < 0.001f) {
      clockLabelPulses[i] = 0.f;
    }
  }

  if (clockMode == SINGLE_MODE) {
    float clockVoltage = inputs[TRG_INPUT].getVoltage(clockChannel);
    bool clocked =
        clockTriggers[clockChannel].process(clockVoltage / 2.f) || manualClock;
    if (clocked) {
      if (inputMode == POLY_MODE) {
        for (int i = 0; i < 16; i++) {
          logLines[i] = inputs[VAL_INPUT].getVoltage(i);
        }
      } else if (inputMode == SINGLE_MODE) {
        for (unsigned int a = NUM_LINES - 1; a > 0; a = a - 1) {
          logLines[a] = logLines[a - 1];
        }

        logLines[0] = inputs[VAL_INPUT].getVoltage(inputChannel);
      } else if (inputMode == INTERNAL_MODE) {
        for (int i = 0; i < 16; i++) {
          logLines[i] = min + spread * random::uniform();
        }
      }
    }
  } else if (clockMode == INTERNAL_MODE) {
    if (inputConnected) {
      if (inputMode == POLY_MODE) {
        for (int i = 0; i < 16; i++) {
          logLines[i] = inputs[VAL_INPUT].getVoltage(i);
        }
      } else if (inputMode == SINGLE_MODE) {
        logLines[inputChannel] = inputs[VAL_INPUT].getVoltage(inputChannel);
      }
    }
    if (inputMode == INTERNAL_MODE) {
      for (int i = 0; i < 16; i++) {
        logLines[i] = min + spread * random::uniform();
      }
    }
  } else if (clockMode == POLY_MODE) {
    if (inputMode == POLY_MODE) {
      for (int i = 0; i < 16; i++) {
        float clockVoltage = inputs[TRG_INPUT].getVoltage(i);
        bool clocked =
            clockTriggers[i].process(clockVoltage / 2.f) || manualClock;
        if (clocked) {
          logLines[i] = inputs[VAL_INPUT].getVoltage(i);
        }
      }
    } else if (inputMode == SINGLE_MODE) {
      for (int i = 0; i < 16; i++) {
        float clockVoltage = inputs[TRG_INPUT].getVoltage(i);
        bool clocked =
            clockTriggers[i].process(clockVoltage / 2.f) || manualClock;
        if (clocked) {
          logLines[i] = inputs[VAL_INPUT].getVoltage(inputChannel);
        }
      }
    } else if (inputMode == INTERNAL_MODE) {
      for (int i = 0; i < 16; i++) {
        float clockVoltage = inputs[TRG_INPUT].getVoltage(i);
        bool clocked =
            clockTriggers[i].process(clockVoltage / 2.f) || manualClock;
        if (clocked) {
          logLines[i] = min + spread * random::uniform();
        }
      }
    }
  }

  if (clearTrigger.process(inputs[CLR_INPUT].getVoltage() / 2.f) ||
      manualClearTrigger.process(params[MANUAL_CLEAR_TRIGGER].getValue())) {
    for (unsigned int a = 0; a < NUM_LINES; a++) {
      logLines[a] = 0;
    }
    strValue = defaultStrValue;
  }

  int numOutputChannels = setChannelCount();

  stepCounter++;

  if (stepCounter > 1025) {
    stepCounter = 0;

    thisVal = "";
    std::string thisLine = "";
    for (unsigned int a = 0; a < NUM_LINES; a = a + 1) {
      if (a < (unsigned int)numOutputChannels) {
        thisLine = formatDebugVoltage(logLines[a]);
      } else {
        thisLine = "";
      }

      thisVal += (a > 0 ? "\n" : "") + thisLine;

      outputs[POLY_OUTPUT].setVoltage(logLines[a], a);
    }
    strValue = thisVal;
  }
}

struct DebugModeSwitch : ThreeVerticalXSwitch {
  DebugModeSwitch() {
    sw->box.pos = Vec(2, 0);
    fb->box.pos = sw->box.pos;
    box.size = Vec(36, 22);
  }
};

struct HidableSmallSnapKnob : SmallSnapKnob {
  bool visible = true;
  int hackIndex = 0;
  ComputerscareDebug* module;

  HidableSmallSnapKnob() { SmallSnapKnob(); }
  void draw(const DrawArgs& args) override {
    if (module
            ? (hackIndex == 0 ? module->clockMode == 0 : module->inputMode == 0)
            : false) {
      Widget::draw(args);
    }
  };
};
////////////////////////////////////
struct StringDisplayWidget3 : Widget {
  std::string value;
  std::string fontPath = "res/fonts/Oswald-Regular.ttf";
  ComputerscareDebug* module = nullptr;
  DebugPreviewState* preview = nullptr;

  StringDisplayWidget3() {};

  int themeIndex() const {
    if (!module) {
      return preview ? preview->themeIndex : 0;
    }
    return math::clamp(
        (int)module->params[ComputerscareDebug::THEME].getValue(), 0,
        DEBUG_THEME_COUNT - 1);
  }

  const DebugTheme& theme() const { return DebugThemes[themeIndex()]; }

  NVGcolor textColor(float alpha = 1.f) const {
    NVGcolor color = theme().text;
    color.a *= alpha;
    return color;
  }

  float barsOpacity() const {
    return module ? module->params[ComputerscareDebug::BARS_OPACITY].getValue()
                  : 1.f;
  }

  NVGcolor barColor(float voltage, int barMode) const {
    NVGcolor color;
    if (module && barMode == ComputerscareDebug::BAR_BIPOLAR) {
      int bipolarColorMode = math::clamp(
          (int)module->params[ComputerscareDebug::BIPOLAR_COLOR_MODE]
              .getValue(),
          0, 2);
      if (bipolarColorMode == ComputerscareDebug::BIPOLAR_COLORS_COLOR_1) {
        color = theme().positive;
        color.a *= barsOpacity();
        return color;
      }
      if (bipolarColorMode == ComputerscareDebug::BIPOLAR_COLORS_COLOR_2) {
        color = theme().negative;
        color.a *= barsOpacity();
        return color;
      }
    }
    color = voltage >= 0.f ? theme().positive : theme().negative;
    color.a *= barsOpacity();
    return color;
  }

  int currentVisualMode() const {
    if (module) {
      return math::clamp(
          (int)module->params[ComputerscareDebug::VISUAL_MODE].getValue(), 0,
          2);
    }
    return preview ? preview->visualMode : ComputerscareDebug::VISUAL_TEXT;
  }

  int currentBarMode() const {
    if (module) {
      return math::clamp(
          (int)module->params[ComputerscareDebug::BAR_MODE].getValue(), 0, 1);
    }
    return preview ? preview->barMode : ComputerscareDebug::BAR_UNIPOLAR;
  }

  int currentTextMode() const {
    if (module) {
      return math::clamp(
          (int)module->params[ComputerscareDebug::TEXT_MODE].getValue(), 0, 3);
    }
    return preview ? preview->textMode : ComputerscareDebug::TEXT_VOLTAGE;
  }

  std::string displayText() const {
    if (!module) {
      return preview ? preview->text : noModuleStringValue;
    }
    if (currentTextMode() == ComputerscareDebug::TEXT_VOLTAGE) {
      return module->strValue;
    }

    int textMode = currentTextMode();
    bool preferFlats = textMode == ComputerscareDebug::TEXT_NOTES_FLATS;
    std::istringstream voltageLines(module->strValue);

    std::string text;
    for (int i = 0; i < NUM_LINES; i++) {
      if (i > 0) {
        text += "\n";
      }
      std::string voltageLine;
      std::getline(voltageLines, voltageLine);
      if (!voltageLine.empty()) {
        text += textMode == ComputerscareDebug::TEXT_MIDI_NOTE
                    ? formatDebugMidiNote(module->logLines[i])
                    : formatDebugNote(module->logLines[i], preferFlats);
      }
    }
    return text;
  }

  void drawBar(NVGcontext* vg, float y, float voltage) {
    const float x = 4.f;
    const float width = box.size.x - 8.f;
    const float height = 13.f;
    const float clamped = clamp(std::fabs(voltage) / 10.f, 0.f, 1.f);
    const bool zero = std::fabs(voltage) < 0.0000005f;
    const int barMode = currentBarMode();

    float barX = x;
    float barWidth = zero ? 0.f : std::max(0.8f, width * clamped);
    if (barMode == ComputerscareDebug::BAR_BIPOLAR) {
      const float centerX = x + width * 0.5f;
      barWidth = zero ? 0.f : std::max(0.8f, width * 0.5f * clamped);
      barX = voltage >= 0.f ? centerX : centerX - barWidth;
    }

    if (barWidth <= 0.f) {
      return;
    }

    nvgBeginPath(vg);
    nvgRoundedRect(vg, barX, y - height * 0.5f, barWidth, height, 2.f);
    nvgFillColor(vg, barColor(voltage, barMode));
    nvgFill(vg);
  }

  void drawBars(NVGcontext* vg) {
    if (!module && !preview) {
      return;
    }

    int visualMode = currentVisualMode();
    if (visualMode != ComputerscareDebug::VISUAL_BARS &&
        visualMode != ComputerscareDebug::VISUAL_TEXT_BARS) {
      return;
    }

    int activeChannels =
        module ? module->outputs[ComputerscareDebug::POLY_OUTPUT].getChannels()
               : preview->activeChannels;
    if (activeChannels <= 0) {
      activeChannels = NUM_LINES;
    }
    activeChannels = math::clamp(activeChannels, 0, NUM_LINES);

    nvgSave(vg);
    nvgIntersectScissor(vg, 1.f, 1.f, box.size.x - 2.f, box.size.y - 2.f);
    for (int i = 0; i < activeChannels; i++) {
      float y = DEBUG_BAR_CENTER_Y + DEBUG_ROW_PITCH * i;
      float value = module ? module->logLines[i] : preview->values[i];
      drawBar(vg, y, value);
    }
    nvgRestore(vg);
  }

  void draw(const DrawArgs& ctx) override {
    // Background
    NVGcolor backgroundColor = theme().background;
    NVGcolor StrokeColor = nvgRGB(0xC0, 0xC7, 0xDE);
    nvgBeginPath(ctx.vg);
    nvgRoundedRect(ctx.vg, -1.0, -1.0, box.size.x + 2, box.size.y + 2, 4.0);
    nvgFillColor(ctx.vg, StrokeColor);
    nvgFill(ctx.vg);
    nvgBeginPath(ctx.vg);
    nvgRoundedRect(ctx.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(ctx.vg, backgroundColor);
    nvgFill(ctx.vg);
  }
  void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
    if (layer == 1) {
      int visualMode = currentVisualMode();
      bool drawText = visualMode == ComputerscareDebug::VISUAL_TEXT ||
                      visualMode == ComputerscareDebug::VISUAL_TEXT_BARS;

      drawBars(args.vg);

      if (!drawText) {
        Widget::drawLayer(args, layer);
        return;
      }

      std::shared_ptr<Font> font =
          APP->window->loadFont(asset::plugin(pluginInstance, fontPath));

      // text
      nvgFontSize(args.vg, 15);
      nvgFontFaceId(args.vg, font->handle);
      nvgTextLetterSpacing(args.vg, 2.5);
      nvgTextLineHeight(args.vg, DEBUG_ROW_PITCH / 15.f);

      std::string textToDraw = displayText();
      Vec textPos = Vec(5.0f, DEBUG_TEXT_BASELINE_Y);
      float textOpacity =
          module ? module->params[ComputerscareDebug::TEXT_OPACITY].getValue()
                 : 1.f;
      if (visualMode == ComputerscareDebug::VISUAL_TEXT_BARS) {
        nvgFillColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, 0.72f * textOpacity));
        nvgTextBox(args.vg, textPos.x + 0.8f, textPos.y + 0.8f, 80,
                   textToDraw.c_str(), NULL);
      }
      nvgFillColor(args.vg, textColor(textOpacity));
      nvgTextBox(args.vg, textPos.x, textPos.y, 80, textToDraw.c_str(), NULL);
    }
    Widget::drawLayer(args, layer);
  }
};
struct ConnectedSmallLetter : SmallLetterDisplay {
  ComputerscareDebug* module = nullptr;
  DebugPreviewState* preview = nullptr;
  int index;
  ConnectedSmallLetter(int dex) {
    index = dex;
    value = std::to_string(dex + 1);
  }
  void draw(const DrawArgs& ctx) override {
    baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
    bool hasSelectionColor = false;
    if (module) {
      int cm = module->clockMode;
      int im = module->inputMode;
      int cc = module->clockChannel;
      int ic = module->inputChannel;

      // both:pink
      // clock: green
      // input:yellow
      if (cm == 0 && im == 0 && cc == index && ic == index) {
        baseColor = COLOR_COMPUTERSCARE_PINK;
        hasSelectionColor = true;
      } else {
        if (cm == 0 && cc == index) {
          baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
          hasSelectionColor = true;
        }
        if (im == 0 && ic == index) {
          baseColor = COLOR_COMPUTERSCARE_YELLOW;
          hasSelectionColor = true;
        }
      }

      float clockPulse = module->clockLabelPulses[index];
      if (clockPulse > 0.f) {
        if (hasSelectionColor) {
          float mix = 0.70f * clockPulse;
          baseColor.r += (0.58f - baseColor.r) * mix;
          baseColor.g += (0.74f - baseColor.g) * mix;
          baseColor.b += (0.68f - baseColor.b) * mix;
          baseColor.a = std::max(baseColor.a, 0.95f);
        } else {
          baseColor =
              nvgRGBA(0x6f, 0x8d, 0x83, (unsigned char)(clockPulse * 0xf2));
        }
      }
    }
    SmallLetterDisplay::draw(ctx);
  }
};

struct DebugVisualParamItem : MenuItem {
  ComputerscareDebug* debug = nullptr;
  int paramId = 0;
  int value = 0;

  void onAction(const event::Action& e) override {
    if (debug) {
      debug->params[paramId].setValue(value);
    }
  }

  void step() override {
    rightText =
        CHECKMARK(debug && (int)debug->params[paramId].getValue() == value);
    MenuItem::step();
  }
};

static void addDebugVisualModeItems(Menu* menu, ComputerscareDebug* debug) {
  for (int mode = 0; mode < 3; mode++) {
    DebugVisualParamItem* item = new DebugVisualParamItem();
    item->text = DebugVisualModeNames[mode];
    item->debug = debug;
    item->paramId = ComputerscareDebug::VISUAL_MODE;
    item->value = mode;
    menu->addChild(item);
  }
}

static void addDebugBipolarColorModeItems(Menu* menu,
                                          ComputerscareDebug* debug);

static void addDebugBarModeItems(Menu* menu, ComputerscareDebug* debug) {
  for (int mode = 0; mode < 2; mode++) {
    DebugVisualParamItem* item = new DebugVisualParamItem();
    item->text = DebugBarModeNames[mode];
    item->debug = debug;
    item->paramId = ComputerscareDebug::BAR_MODE;
    item->value = mode;
    menu->addChild(item);
  }
  menu->addChild(new MenuSeparator);
  menu->addChild(createSubmenuItem(
      "Bipolar Colors",
      DebugBipolarColorModeNames
          [(int)debug->params[ComputerscareDebug::BIPOLAR_COLOR_MODE]
               .getValue()],
      [=](Menu* submenu) { addDebugBipolarColorModeItems(submenu, debug); }));
}

static void addDebugBipolarColorModeItems(Menu* menu,
                                          ComputerscareDebug* debug) {
  for (int mode = 0; mode < 3; mode++) {
    DebugVisualParamItem* item = new DebugVisualParamItem();
    item->text = DebugBipolarColorModeNames[mode];
    item->debug = debug;
    item->paramId = ComputerscareDebug::BIPOLAR_COLOR_MODE;
    item->value = mode;
    menu->addChild(item);
  }
}

static void addDebugTextModeItems(Menu* menu, ComputerscareDebug* debug) {
  for (int mode = 0; mode < 4; mode++) {
    DebugVisualParamItem* item = new DebugVisualParamItem();
    item->text = DebugTextModeNames[mode];
    item->debug = debug;
    item->paramId = ComputerscareDebug::TEXT_MODE;
    item->value = mode;
    menu->addChild(item);
  }
}

static void addDebugQuickVisualSettingsItems(Menu* menu,
                                             ComputerscareDebug* debug) {
  menu->addChild(createSubmenuItem(
      "Bar Mode",
      DebugBarModeNames[(int)debug->params[ComputerscareDebug::BAR_MODE]
                            .getValue()],
      [=](Menu* submenu) { addDebugBarModeItems(submenu, debug); }));
  menu->addChild(new MenuSeparator);
  menu->addChild(new MenuParamSlider(
      debug->paramQuantities[ComputerscareDebug::TEXT_OPACITY]));
  menu->addChild(new MenuParamSlider(
      debug->paramQuantities[ComputerscareDebug::BARS_OPACITY]));
}

struct DebugThemeMenuItem : MenuItem {
  ComputerscareDebug* debug = nullptr;
  int themeIndex = 0;
  float previewValue = 0.f;

  DebugThemeMenuItem() {
    previewValue = -10.f + 20.f * random::uniform();
    box.size.y = 46.f;
  }

  void onAction(const event::Action& e) override {
    if (debug) {
      debug->params[ComputerscareDebug::THEME].setValue(themeIndex);
    }
  }

  void draw(const DrawArgs& args) override {
    BNDwidgetState state = BND_DEFAULT;
    if (APP->event->hoveredWidget == this) state = BND_HOVER;

    const BNDtheme* rackTheme = bndGetTheme();
    if (state != BND_DEFAULT) {
      bndInnerBox(args.vg, 0.0, 0.0, box.size.x, box.size.y, 0, 0, 0, 0,
                  bndOffsetColor(rackTheme->menuItemTheme.innerSelectedColor,
                                 rackTheme->menuItemTheme.shadeTop),
                  bndOffsetColor(rackTheme->menuItemTheme.innerSelectedColor,
                                 rackTheme->menuItemTheme.shadeDown));
      state = BND_ACTIVE;
    }

    const DebugTheme& theme = DebugThemes[themeIndex];
    NVGcolor textColor = bndTextColor(&rackTheme->menuItemTheme, state);
    bndIconLabelValue(args.vg, 0.f, 3.f, box.size.x, 16.f, -1, textColor,
                      BND_LEFT, BND_LABEL_FONT_SIZE, text.c_str(), NULL);

    const float x = 8.f;
    const float y = 29.f;
    const float width = std::max(100.f, box.size.x - 34.f);
    const float height = 13.f;
    const float centerX = x + width * 0.5f;
    const float radius = 2.f;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, y - height * 0.5f, width, height, radius);
    nvgFillColor(args.vg, theme.background);
    nvgFill(args.vg);

    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, x, y - height * 0.5f, width * 0.5f, height);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, y - height * 0.5f, width, height, radius);
    nvgFillColor(args.vg, theme.negative);
    nvgFill(args.vg);
    nvgRestore(args.vg);

    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, centerX, y - height * 0.5f, width * 0.5f,
                        height);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, y - height * 0.5f, width, height, radius);
    nvgFillColor(args.vg, theme.positive);
    nvgFill(args.vg);
    nvgRestore(args.vg);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, centerX, y - 6.f);
    nvgLineTo(args.vg, centerX, y + 6.f);
    nvgStrokeColor(args.vg, theme.text);
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);

    std::string previewText = formatDebugVoltage(previewValue);
    nvgFontSize(args.vg, 10.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, theme.text);
    nvgText(args.vg, centerX, y, previewText.c_str(), NULL);

    if (!rightText.empty()) {
      float checkX = box.size.x - bndLabelWidth(args.vg, -1, rightText.c_str());
      bndIconLabelValue(args.vg, checkX, 0.f, box.size.x, box.size.y, -1,
                        textColor, BND_LEFT, BND_LABEL_FONT_SIZE,
                        rightText.c_str(), NULL);
    }
  }

  void step() override {
    rightText = CHECKMARK(
        debug &&
        (int)debug->params[ComputerscareDebug::THEME].getValue() == themeIndex);
    box.size.x = std::max(
        150.f, bndLabelWidth(APP->window->vg, -1, text.c_str()) + 34.f);
    box.size.y = 46.f;
    Widget::step();
  }
};

static void addDebugThemeItems(Menu* menu, ComputerscareDebug* debug) {
  for (int i = 0; i < DEBUG_THEME_COUNT; i++) {
    DebugThemeMenuItem* item = new DebugThemeMenuItem();
    item->text = DebugThemes[i].name;
    item->debug = debug;
    item->themeIndex = i;
    menu->addChild(item);
  }
}

struct DebugLabelHoverButton : ComputerscareBlankButton {
  ComputerscareDebug* module = nullptr;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  ui::Tooltip* hoverTooltip = NULL;
  int control = 0;
  int menuFrame = -1;
  bool hovering = false;

  enum Control {
    DISPLAY_CONTROL,
    BAR_STYLE_CONTROL,
    VISUAL_SETTINGS_CONTROL,
    THEME_CONTROL,
    TEXT_CONTROL
  };

  DebugLabelHoverButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
    box.size = Vec(25.f, 13.f);
    shadow->opacity = 0.f;
  }

  ~DebugLabelHoverButton() { destroyHoverTooltip(); }

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

    hoverTooltip->text = tooltipText();
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

  bool isShowing() { return isMenuOpen() || hovering; }

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

  int currentBarMode() const {
    if (!module) {
      return ComputerscareDebug::BAR_UNIPOLAR;
    }
    return math::clamp(
        (int)module->params[ComputerscareDebug::BAR_MODE].getValue(), 0, 1);
  }

  std::string codeText() const {
    if (control == DISPLAY_CONTROL) {
      return "DSP";
    }
    if (control == BAR_STYLE_CONTROL) {
      return DebugBarModeCodes[currentBarMode()];
    }
    if (control == VISUAL_SETTINGS_CONTROL) {
      return "SVL";
    }
    if (control == TEXT_CONTROL) {
      return "Txt";
    }
    return "Thm";
  }

  std::string tooltipText() const {
    if (control == DISPLAY_CONTROL) {
      return "Display";
    }
    if (control == BAR_STYLE_CONTROL) {
      return "Bar Style";
    }
    if (control == VISUAL_SETTINGS_CONTROL) {
      return "Visual";
    }
    if (control == TEXT_CONTROL) {
      return "Text";
    }
    return "Theme";
  }

  void draw(const DrawArgs& args) override {
    updateMenuFrame();
    if (!isShowing()) {
      return;
    }
    ComputerscareBlankButton::draw(args);
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 9.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    const float textXOffset = isMenuOpen() ? 1.f : 0.f;
    const float textYOffset = isMenuOpen() ? 1.4f : 0.f;
    std::string code = codeText();
    nvgText(args.vg, box.size.x * 0.32f + textXOffset,
            box.size.y * 0.50f + textYOffset, code.c_str(), NULL);
  }

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && isMenuOpen()) {
      updateMenuFrame();
      return;
    }

    ComputerscareBlankButton::onDragEnd(e);
  }

  void onEnter(const event::Enter& e) override {
    hovering = true;
    createHoverTooltip();
    updateHoverTooltip();
    ComputerscareBlankButton::onEnter(e);
  }

  void onLeave(const event::Leave& e) override {
    hovering = false;
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
    if (!module) {
      return;
    }

    Menu* menu = createMenu();
    activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
    menu->addChild(createMenuLabel(tooltipText()));
    if (control == DISPLAY_CONTROL) {
      addDebugVisualModeItems(menu, module);
    } else if (control == BAR_STYLE_CONTROL) {
      addDebugBarModeItems(menu, module);
    } else if (control == VISUAL_SETTINGS_CONTROL) {
      addDebugQuickVisualSettingsItems(menu, module);
    } else if (control == TEXT_CONTROL) {
      addDebugTextModeItems(menu, module);
    } else {
      addDebugThemeItems(menu, module);
    }
    updateMenuFrame();
  }
};

struct ComputerscareDebugWidget : ModuleWidget {
  DebugPreviewState previewState;

  ComputerscareDebugWidget(ComputerscareDebug* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareDebugPanel.svg")));

    addInput(createInput<InPort>(Vec(2, 339), module,
                                 ComputerscareDebug::TRG_INPUT));
    addInput(createInput<InPort>(Vec(61, 339), module,
                                 ComputerscareDebug::VAL_INPUT));
    addInput(createInput<InPort>(Vec(31, 339), module,
                                 ComputerscareDebug::CLR_INPUT));

    addParam(createParam<ComputerscareClockButton>(
        Vec(2, 325), module, ComputerscareDebug::MANUAL_TRIGGER));

    addParam(createParam<ComputerscareResetButton>(
        Vec(32, 324), module, ComputerscareDebug::MANUAL_CLEAR_TRIGGER));

    DebugModeSwitch* clockModeSwitch = createParam<DebugModeSwitch>(
        Vec(0, 279), module, ComputerscareDebug::WHICH_CLOCK);
    if (!module && clockModeSwitch->getParamQuantity()) {
      clockModeSwitch->getParamQuantity()->setValue(
          ComputerscareDebug::INTERNAL_MODE);
    }
    addParam(clockModeSwitch);

    DebugModeSwitch* inputModeSwitch = createParam<DebugModeSwitch>(
        Vec(58, 279), module, ComputerscareDebug::SWITCH_VIEW);
    if (!module && inputModeSwitch->getParamQuantity()) {
      inputModeSwitch->getParamQuantity()->setValue(
          ComputerscareDebug::POLY_MODE);
    }
    addParam(inputModeSwitch);

    HidableSmallSnapKnob* clockKnob = createParam<HidableSmallSnapKnob>(
        Vec(6, 305), module, ComputerscareDebug::CLOCK_CHANNEL_FOCUS);
    clockKnob->module = module;
    clockKnob->hackIndex = 0;
    addParam(clockKnob);

    HidableSmallSnapKnob* inputKnob = createParam<HidableSmallSnapKnob>(
        Vec(66, 305), module, ComputerscareDebug::INPUT_CHANNEL_FOCUS);
    inputKnob->module = module;
    inputKnob->hackIndex = 1;
    addParam(inputKnob);

    addOutput(createOutput<OutPort>(Vec(56, 1), module,
                                    ComputerscareDebug::POLY_OUTPUT));

    for (int i = 0; i < 16; i++) {
      ConnectedSmallLetter* sld = new ConnectedSmallLetter(i);
      sld->fontSize = 15;
      sld->textAlign = 1;
      sld->box.pos = Vec(DEBUG_LABEL_X, DEBUG_LABEL_Y + DEBUG_ROW_PITCH * i);
      sld->box.size = Vec(28, 20);
      sld->module = module;
      sld->preview = module ? nullptr : &previewState;
      addChild(sld);
    }

    for (int i = 0; i < 5; i++) {
      DebugLabelHoverButton* button = createWidget<DebugLabelHoverButton>(
          Vec(DEBUG_LABEL_MENU_X, DEBUG_LABEL_MENU_Y + DEBUG_ROW_PITCH * i));
      button->module = module;
      button->control = i;
      addChild(button);
    }

    StringDisplayWidget3* stringDisplay = createWidget<StringDisplayWidget3>(
        Vec(DEBUG_DISPLAY_X, DEBUG_DISPLAY_Y));
    stringDisplay->box.size = Vec(73, 245);
    stringDisplay->module = module;
    stringDisplay->preview = module ? nullptr : &previewState;
    addChild(stringDisplay);

    debug = module;
  }
  /*json_t *toJson() override
  {
          json_t *rootJ = ModuleWidget::toJson();
          json_object_set_new(rootJ, "outputRange",
  json_integer(debug->outputRangeEnum));

          json_t *sequencesJ = json_array();

          for (int i = 0; i < 16; i++) {
                  json_t *sequenceJ = json_real(debug->logLines[i]);
                  json_array_append_new(sequencesJ, sequenceJ);
          }
          json_object_set_new(rootJ, "lines", sequencesJ);
          return rootJ;
  }*/
  /*void fromJson(json_t *rootJ) override
  {
          float val;
          ModuleWidget::fromJson(rootJ);
          // button states

          json_t *outputRangeEnumJ = json_object_get(rootJ, "outputRange");
          if (outputRangeEnumJ) { debug->outputRangeEnum =
  json_integer_value(outputRangeEnumJ); }

          json_t *sequencesJ = json_object_get(rootJ, "lines");

          if (sequencesJ) {
                  for (int i = 0; i < 16; i++) {
                          json_t *sequenceJ = json_array_get(sequencesJ, i);
                          if (sequenceJ)
                                  val = json_real_value(sequenceJ);
                          debug->logLines[i] = val;
                  }
          }


  }*/
  void appendContextMenu(Menu* menu) override;
  ComputerscareDebug* debug;
};
struct DebugOutputRangeItem : MenuItem {
  ComputerscareDebug* debug;
  int outputRangeEnum;
  void onAction(const event::Action& e) override {
    debug->outputRangeEnum = outputRangeEnum;
  }
  void step() override {
    rightText = CHECKMARK(debug->outputRangeEnum == outputRangeEnum);
    MenuItem::step();
  }
};

struct DebugTriggerBlinkersItem : MenuItem {
  ComputerscareDebug* debug;

  void onAction(const event::Action& e) override {
    Param& param = debug->params[ComputerscareDebug::TRIGGER_BLINKERS];
    param.setValue(param.getValue() > 0.5f ? 0.f : 1.f);
  }

  void step() override {
    rightText = CHECKMARK(
        debug->params[ComputerscareDebug::TRIGGER_BLINKERS].getValue() > 0.5f);
    MenuItem::step();
  }
};

struct DebugModeMenuItem : MenuItem {
  ComputerscareDebug* debug;
  int paramId;
  int value;
  std::string description;

  DebugModeMenuItem(int value, const char* description) {
    this->value = value;
    text = DebugModeNames[value];
    this->description = description;
    box.size.y = 42.f;
  }

  void onAction(const event::Action& e) override {
    debug->params[paramId].setValue(value);
  }

  void draw(const DrawArgs& args) override {
    BNDwidgetState state = BND_DEFAULT;
    if (APP->event->hoveredWidget == this) state = BND_HOVER;

    const BNDtheme* theme = bndGetTheme();
    if (state != BND_DEFAULT) {
      bndInnerBox(args.vg, 0.0, 0.0, box.size.x, box.size.y, 0, 0, 0, 0,
                  bndOffsetColor(theme->menuItemTheme.innerSelectedColor,
                                 theme->menuItemTheme.shadeTop),
                  bndOffsetColor(theme->menuItemTheme.innerSelectedColor,
                                 theme->menuItemTheme.shadeDown));
      state = BND_ACTIVE;
    }

    NVGcolor nameColor = bndTextColor(&theme->menuItemTheme, state);
    NVGcolor descriptionColor = state == BND_DEFAULT
                                    ? theme->menuTheme.textColor
                                    : theme->menuTheme.textSelectedColor;

    bndIconLabelValue(args.vg, 0.f, 3.f, box.size.x, 18.f, -1, nameColor,
                      BND_LEFT, BND_LABEL_FONT_SIZE, text.c_str(), NULL);
    bndIconLabelValue(args.vg, 0.f, 21.f, box.size.x, 18.f, -1,
                      descriptionColor, BND_LEFT, BND_LABEL_FONT_SIZE,
                      description.c_str(), NULL);

    if (!rightText.empty()) {
      float x = box.size.x - bndLabelWidth(args.vg, -1, rightText.c_str());
      bndIconLabelValue(args.vg, x, 0.f, box.size.x, box.size.y, -1,
                        descriptionColor, BND_LEFT, BND_LABEL_FONT_SIZE,
                        rightText.c_str(), NULL);
    }
  }

  void step() override {
    rightText = CHECKMARK(debug->params[paramId].getValue() == value);
    box.size.x =
        std::max(bndLabelWidth(APP->window->vg, -1, text.c_str()),
                 bndLabelWidth(APP->window->vg, -1, description.c_str())) +
        34.f;
    box.size.y = 42.f;
    Widget::step();
  }
};

void ComputerscareDebugWidget::appendContextMenu(Menu* menu) {
  ComputerscareDebug* debug = dynamic_cast<ComputerscareDebug*>(this->module);
  if (!debug) return;

  MenuLabel* spacerLabel = new MenuLabel();
  menu->addChild(spacerLabel);

  menu->addChild(construct<DebugTriggerBlinkersItem>(
      &MenuItem::text, "Show Clock Blinkers", &DebugTriggerBlinkersItem::debug,
      debug));

  menu->addChild(createSubmenuItem(
      "Clock Mode",
      DebugModeNames[(int)debug->params[ComputerscareDebug::WHICH_CLOCK]
                         .getValue()],
      [=](Menu* submenu) {
        for (int i = 0; i < 3; i++) {
          DebugModeMenuItem* item =
              new DebugModeMenuItem(i, DebugClockModeDescriptions[i]);
          item->debug = debug;
          item->paramId = ComputerscareDebug::WHICH_CLOCK;
          submenu->addChild(item);
        }
      }));
  menu->addChild(createSubmenuItem(
      "Input Mode",
      DebugModeNames[(int)debug->params[ComputerscareDebug::SWITCH_VIEW]
                         .getValue()],
      [=](Menu* submenu) {
        for (int i = 0; i < 3; i++) {
          DebugModeMenuItem* item =
              new DebugModeMenuItem(i, DebugInputModeDescriptions[i]);
          item->debug = debug;
          item->paramId = ComputerscareDebug::SWITCH_VIEW;
          submenu->addChild(item);
        }
      }));
  menu->addChild(createSubmenuItem(
      "Random Generator Range", DebugOutputRangeLabels[debug->outputRangeEnum],
      [=](Menu* submenu) {
        for (int i = 0; i < 8; i++) {
          submenu->addChild(construct<DebugOutputRangeItem>(
              &MenuItem::text, DebugOutputRangeLabels[i],
              &DebugOutputRangeItem::debug, debug,
              &DebugOutputRangeItem::outputRangeEnum, i));
        }
      }));
  menu->addChild(new MenuSeparator);
  menu->addChild(createSubmenuItem("Visual", "", [=](Menu* visualMenu) {
    addDebugQuickVisualSettingsItems(visualMenu, debug);
  }));
  menu->addChild(createSubmenuItem(
      "Text",
      DebugTextModeNames[(int)debug->params[ComputerscareDebug::TEXT_MODE]
                             .getValue()],
      [=](Menu* textMenu) { addDebugTextModeItems(textMenu, debug); }));
  menu->addChild(createSubmenuItem(
      "Theme",
      DebugThemes[(int)debug->params[ComputerscareDebug::THEME].getValue()]
          .name,
      [=](Menu* submenu) { addDebugThemeItems(submenu, debug); }));
}
Model* modelComputerscareDebug =
    createModel<ComputerscareDebug, ComputerscareDebugWidget>(
        "computerscare-debug");
