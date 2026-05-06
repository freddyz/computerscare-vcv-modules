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

struct ComputerscareDebug;

std::string noModuleStringValue =
    "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
    "000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0."
    "000000\n+0.000000\n+0.000000\n+0.000000\n";

struct ComputerscareDebug : Module {
  enum ParamIds {
    MANUAL_TRIGGER,
    MANUAL_CLEAR_TRIGGER,
    CLOCK_CHANNEL_FOCUS,
    INPUT_CHANNEL_FOCUS,
    SWITCH_VIEW,
    WHICH_CLOCK,
    TRIGGER_BLINKERS,
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
      "0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0."
      "000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0."
      "000000\n0.000000\n";

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
    getParamQuantity(CLOCK_CHANNEL_FOCUS)->randomizeEnabled = false;
    getParamQuantity(INPUT_CHANNEL_FOCUS)->randomizeEnabled = false;

    randomizeStorage();
  }
  void process(const ProcessArgs& args) override;

  void onRandomize() override { randomizeStorage(); }

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

    json_t* sequencesJ = json_array();

    for (int i = 0; i < 16; i++) {
      json_t* sequenceJ = json_real(logLines[i]);
      json_array_append_new(sequencesJ, sequenceJ);
    }
    json_object_set_new(rootJ, "lines", sequencesJ);
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    float val;

    json_t* outputRangeEnumJ = json_object_get(rootJ, "outputRange");
    if (outputRangeEnumJ) {
      outputRangeEnum = json_integer_value(outputRangeEnumJ);
    }

    json_t* triggerBlinkersJ = json_object_get(rootJ, "triggerBlinkers");
    params[TRIGGER_BLINKERS].setValue(
        triggerBlinkersJ ? (json_boolean_value(triggerBlinkersJ) ? 1.f : 0.f)
                         : 0.f);

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
        thisLine = logLines[a] >= 0 ? "+" : "";
        thisLine += std::to_string(logLines[a]);
        thisLine = thisLine.substr(0, 9);
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
            : true) {
      Widget::draw(args);
    }
  };
};
////////////////////////////////////
struct StringDisplayWidget3 : Widget {
  std::string value;
  std::string fontPath = "res/fonts/Oswald-Regular.ttf";
  ComputerscareDebug* module;

  StringDisplayWidget3() {};

  void draw(const DrawArgs& ctx) override {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x10, 0x00, 0x10);
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
      std::shared_ptr<Font> font =
          APP->window->loadFont(asset::plugin(pluginInstance, fontPath));

      // text
      nvgFontSize(args.vg, 15);
      nvgFontFaceId(args.vg, font->handle);
      nvgTextLetterSpacing(args.vg, 2.5);

      std::string textToDraw = module ? module->strValue : noModuleStringValue;
      Vec textPos = Vec(6.0f, 12.0f);
      NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
      nvgFillColor(args.vg, textColor);
      nvgTextBox(args.vg, textPos.x, textPos.y, 80, textToDraw.c_str(), NULL);
    }
    Widget::drawLayer(args, layer);
  }
};
struct ConnectedSmallLetter : SmallLetterDisplay {
  ComputerscareDebug* module;
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
struct ComputerscareDebugWidget : ModuleWidget {
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

    addParam(createParam<DebugModeSwitch>(Vec(0, 279), module,
                                          ComputerscareDebug::WHICH_CLOCK));
    addParam(createParam<DebugModeSwitch>(Vec(58, 279), module,
                                          ComputerscareDebug::SWITCH_VIEW));

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
      sld->box.pos = Vec(-4, 33.8 + 15.08 * i);
      sld->box.size = Vec(28, 20);
      sld->module = module;
      addChild(sld);
    }

    StringDisplayWidget3* stringDisplay =
        createWidget<StringDisplayWidget3>(Vec(15, 34));
    stringDisplay->box.size = Vec(73, 245);
    stringDisplay->module = module;
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
}
Model* modelComputerscareDebug =
    createModel<ComputerscareDebug, ComputerscareDebugWidget>(
        "computerscare-debug");
