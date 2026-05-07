#include "Computerscare.hpp"
#include "ColorableSmallKnob.hpp"

struct ComputerscareMolyPatrix;

const int numKnobs = 256;
const int numRows = 16;
const int numColumns = 16;

const int numOutputs = 1;

struct ComputerscareMolyPatrix : ComputerscarePolyModule {
  int counter = 0;
  int numInputChannels = 0;
  ComputerscareSVGPanel* panelRef;
  enum ParamIds {
    KNOB,
    INPUT_ROW_TRIM = KNOB + numKnobs,
    OUTPUT_COLUMN_TRIM = INPUT_ROW_TRIM + numRows,
    OUTPUT_TRIM = OUTPUT_COLUMN_TRIM + numColumns,
    POLY_CHANNELS,
    INPUT_TRIM,
    INPUT_OFFSET,
    OUTPUT_OFFSET,
    COLOR_MODE,
    COLOR_STRENGTH,
    COLOR_HUE,
    COLOR_FADE,
    NUM_PARAMS
  };
  enum InputIds {
    POLY_INPUT,
    ZERO_INPUT,
    ONE_INPUT,
    INPUT_ATTENUATION_CV,
    INPUT_OFFSET_CV,
    OUTPUT_ATTENUATION_CV,
    OUTPUT_OFFSET_CV,
    NUM_INPUTS
  };
  enum OutputIds { POLY_OUTPUT, NUM_OUTPUTS = POLY_OUTPUT + numOutputs };
  enum LightIds { NUM_LIGHTS };

  ComputerscareMolyPatrix() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    /*for(int n = 0; n < numKnobs; n++) {
      configParam(KNOB+n,-1.f,1.f,0.f);
    }*/
    for (int i = 0; i < numRows; i++) {
      configParam(INPUT_ROW_TRIM + i, -2.f, 2.f, 1.f,
                  "Input Channel " + std::to_string(i + 1) + " Attenuation");
      configParam(OUTPUT_COLUMN_TRIM + i, -2.f, 2.f, 1.f,
                  "Output Channel " + std::to_string(i + 1) + " Attenuation");

      getParamQuantity(INPUT_ROW_TRIM + i)->randomizeEnabled = false;
      getParamQuantity(OUTPUT_COLUMN_TRIM + i)->randomizeEnabled = false;

      for (int j = 0; j < numColumns; j++) {
        configParam(KNOB + i * 16 + j, -2.f, 2.f, i == j ? 1.f : 0.f,
                    "Input ch." + std::to_string(i + 1) + " → Output ch." +
                        std::to_string(j + 1));
      }
    }
    configParam(OUTPUT_TRIM, -2.f, 2.f, 1.f, "Output Attenuation");
    configParam(OUTPUT_OFFSET, -10.f, 10.f, 0.f, "Output Offset");
    configParam(INPUT_TRIM, -2.f, 2.f, 1.f, "Input Attenuation");
    configParam(INPUT_OFFSET, -10.f, 10.f, 0.f, "Input Offset");
    configSwitch(COLOR_MODE, 0.f, 1.f, 1.f, "Knob Color Mode",
                 {"Legacy", "Value colors"});
    configParam(COLOR_STRENGTH, 0.f, 2.f, 1.f, "Knob Color Strength");
    configParam(COLOR_HUE, 0.f, 9.f, 0.f, "Knob Color Hue");
    configParam(COLOR_FADE, 0.f, 2.f, 1.f, "Knob Fade", "%", 0.f, 100.f);
    getParamQuantity(OUTPUT_TRIM)->randomizeEnabled = false;
    getParamQuantity(OUTPUT_OFFSET)->randomizeEnabled = false;
    getParamQuantity(INPUT_TRIM)->randomizeEnabled = false;
    getParamQuantity(INPUT_OFFSET)->randomizeEnabled = false;
    getParamQuantity(COLOR_MODE)->randomizeEnabled = false;
    getParamQuantity(COLOR_STRENGTH)->randomizeEnabled = false;
    getParamQuantity(COLOR_HUE)->randomizeEnabled = false;
    getParamQuantity(COLOR_FADE)->randomizeEnabled = false;

    configSwitch(POLY_CHANNELS, 0.f, 16.f, 0.f, "Poly Channels",
                 polyChannelLabels(true));
    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS)->resetEnabled = false;

    configInput(POLY_INPUT, "Main");

    configInput(INPUT_ATTENUATION_CV, "Input Attenuation");
    configInput(OUTPUT_ATTENUATION_CV, "Output Attenuation");

    configOutput(POLY_OUTPUT, "Main");
  }
  void fromJson(json_t* rootJ) override {
    bool hasColorMode = false;
    json_t* paramsJ = json_object_get(rootJ, "params");
    if (paramsJ) {
      size_t i;
      json_t* paramJ;
      json_array_foreach(paramsJ, i, paramJ) {
        json_t* paramIdJ = json_object_get(paramJ, "id");
        if (!paramIdJ) {
          paramIdJ = json_object_get(paramJ, "paramId");
        }
        size_t paramId = paramIdJ ? json_integer_value(paramIdJ) : i;
        if (paramId == COLOR_MODE) {
          hasColorMode = true;
          break;
        }
      }
    }
    Module::fromJson(rootJ);
    if (!hasColorMode) {
      params[COLOR_MODE].setValue(0.f);
    }
  }
  void checkPoly() override {
    numInputChannels = inputs[POLY_INPUT].getChannels();
    int knobSetting = params[POLY_CHANNELS].getValue();
    if (numInputChannels > 0) {
      if (knobSetting == 0) {
        polyChannels = numInputChannels;
      } else {
        polyChannels = knobSetting;
      }
    } else {
      polyChannels = knobSetting == 0 ? 16 : knobSetting;
    }
    outputs[POLY_OUTPUT].setChannels(polyChannels);
  }
  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();
    float outTrim = params[OUTPUT_TRIM].getValue();
    float outOffset = params[OUTPUT_OFFSET].getValue();

    float inOffset = params[INPUT_OFFSET].getValue();

    int numInputTrimChannels = inputs[INPUT_ATTENUATION_CV].getChannels();
    int numOutputTrimChannels = inputs[OUTPUT_ATTENUATION_CV].getChannels();

    float inputTrims[16] = {};
    float outputTrims[16] = {};
    float inputVals[16] = {};

    if (numInputChannels > 0) {
      inputs[POLY_INPUT].readVoltages(inputVals);
    }
    if (numInputTrimChannels > 0) {
      inputs[INPUT_ATTENUATION_CV].readVoltages(inputTrims);
    }
    if (numOutputTrimChannels > 0) {
      inputs[OUTPUT_ATTENUATION_CV].readVoltages(outputTrims);
    }

    for (int outIndex = 0; outIndex < numRows; outIndex++) {
      float outVoltage = 0.f;
      for (int i = 0; i < numColumns; i++) {
        outVoltage += params[KNOB + i * 16 + outIndex].getValue() *
                      (inputVals[i] + inOffset) *
                      params[INPUT_ROW_TRIM + i].getValue() *
                      (params[INPUT_TRIM].getValue() *
                       (numInputTrimChannels > 0
                            ? inputTrims[numInputTrimChannels == 1 ? 0 : i] / 10
                            : 1));
      }
      outputs[POLY_OUTPUT].setVoltage(
          params[OUTPUT_COLUMN_TRIM + outIndex].getValue() *
                  (outTrim *
                   (numOutputTrimChannels > 0
                        ? outputTrims[numOutputTrimChannels == 1 ? 0
                                                                 : outIndex] /
                              10
                        : 1)) *
                  outVoltage +
              outOffset,
          outIndex);
    }
  }
};

struct DisableableSmallKnob : ComputerscareRoundKnob {
  std::vector<std::shared_ptr<Svg>> enabledThemes = {
      APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/components/computerscare-small-knob-effed.svg")),
      APP->window->loadSvg(asset::plugin(
          pluginInstance,
          "res/components/computerscare-small-knob-effed-dark.svg"))};

  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-small-knob-effed.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-small-knob-effed-disabled.svg"));

  int inputChannel = 0;
  int outputChannel = 0;
  int themeIndex = 0;
  bool disabled = false;
  bool initialized = false;
  bool randomizable = true;
  ComputerscareMolyPatrix* module;
  bool previewMode = false;
  int previewNumInputChannels = 16;
  int previewPolyChannels = 16;
  float previewValue = 1.f;

  DisableableSmallKnob() {
    setSvg(enabledThemes[themeIndex]);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  float previewAngle() {
    return math::rescale(previewValue, -2.f, 2.f, minAngle, maxAngle);
  }

  void draw(const DrawArgs& args) override {
    if (module || previewMode) {
      int activeInputChannels =
          module ? module->numInputChannels : previewNumInputChannels;
      int activePolyChannels =
          module ? module->polyChannels : previewPolyChannels;
      bool candidateDisabled =
          ((activeInputChannels != 0 && inputChannel > activeInputChannels - 1) ||
           outputChannel > activePolyChannels - 1);
      if (disabled != candidateDisabled || !initialized) {
        setSvg(candidateDisabled ? disabledSvg : enabledThemes[themeIndex]);
        disabled = candidateDisabled;
        onChange(*(new event::Change()));
        fb->dirty = true;
        initialized = true;
      }
    } else {
    }
    if (previewMode && !module) {
      math::Vec center = sw->box.getCenter();
      tw->identity();
      tw->translate(center);
      tw->rotate(previewAngle());
      tw->translate(center.neg());
    }
    ComputerscareRoundKnob::draw(args);
  }
};

struct ComputerscareMolyPatrixWidget : ModuleWidget {
  bool previewMode = false;
  float previewColorMode = 1.f;
  float previewColorStrength = 1.f;
  float previewColorHue = 0.f;
  float previewFade = 1.f;
  int previewNumInputChannels = 16;
  int previewPolyChannels = 16;

  ComputerscareMolyPatrixWidget(ComputerscareMolyPatrix* module) {
    setModule(module);
    if (!module) {
      previewMode = true;
      previewColorMode = random::uniform() < 0.3f ? 0.f : 1.f;
      previewColorStrength = random::uniform() * 2.f;
      previewColorHue = random::uniform() * 9.f;
      previewFade = random::uniform() * 2.f;
      previewNumInputChannels = 1 + (int)std::floor(random::uniform() * 16.f);
      previewPolyChannels = 1 + (int)std::floor(random::uniform() * 16.f);
    }

    // setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,
    // "res/panels/ComputerscareMolyPatrixPanel.svg")));
    box.size = Vec(28 * 15, 380);
    {
      ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/panels/ComputerscareMolyPatrixPanel.svg")));

      // module->panelRef = panel;

      addChild(panel);
    }
    float xx;
    float yy;
    float x0 = 30;
    float dx = 21.4;
    float y0 = 43;
    float dy = 21;

    float inX = 28;

    addInput(createInput<PointingUpPentagonPort>(
        Vec(inX + 0, 12), module, ComputerscareMolyPatrix::POLY_INPUT));
    addKnob(inX + 28, 12, module, ComputerscareMolyPatrix::INPUT_TRIM, 0, 0, 1,
            0);
    addInput(
        createInput<TinyJack>(Vec(inX + 41, 25), module,
                              ComputerscareMolyPatrix::INPUT_ATTENUATION_CV));

    inputOffsetKnob = createParam<SmoothKnobNoRandom>(
        Vec(inX + 58, 14), module, ComputerscareMolyPatrix::INPUT_OFFSET);
    randomizePreviewKnob(inputOffsetKnob);
    addParam(inputOffsetKnob);

    // addKnob(60, 16, module, ComputerscareMolyPatrix::INPUT_TRIM, 0, 0,1,0);

    for (int i = 0; i < numRows; i++) {
      for (int j = 0; j < numColumns; j++) {
        xx = x0 + j * dx;
        yy = y0 + i * dy;
        addKnob(xx, yy, module, i * 16 + j, i, j);
      }
      addKnob(x0 - 25, y0 + i * dy, module,
              ComputerscareMolyPatrix::INPUT_ROW_TRIM + i, i, 0, 1, 0);
      addKnob(420 - 40, y0 + i * dy, module,
              ComputerscareMolyPatrix::OUTPUT_COLUMN_TRIM + i, 0, i, 1, 0);
    }

    float outX = 302;

    addKnob(outX + 0, 1, module, ComputerscareMolyPatrix::OUTPUT_TRIM, 0, 0, 1,
            0);
    addInput(
        createInput<TinyJack>(Vec(outX + 10, 15), module,
                              ComputerscareMolyPatrix::OUTPUT_ATTENUATION_CV));

    outputOffsetKnob = createParam<SmoothKnobNoRandom>(
        Vec(outX + 28, 4), module, ComputerscareMolyPatrix::OUTPUT_OFFSET);
    randomizePreviewKnob(outputOffsetKnob);
    addParam(outputOffsetKnob);

    channelWidget = new PolyOutputChannelsWidget(
        Vec(outX + 55, 1), module, ComputerscareMolyPatrix::POLY_CHANNELS);
    addChild(channelWidget);

    addOutput(createOutput<InPort>(Vec(outX + 80, 1), module,
                                   ComputerscareMolyPatrix::POLY_OUTPUT));
  }

  void randomizePreviewKnob(ComputerscareRoundKnob* previewKnob) {
    if (!previewMode || !previewKnob) {
      return;
    }
    float angle = math::rescale(-2.f + random::uniform() * 4.f, -2.f, 2.f,
                                previewKnob->minAngle, previewKnob->maxAngle);
    math::Vec center = previewKnob->sw->box.getCenter();
    previewKnob->tw->identity();
    previewKnob->tw->translate(center);
    previewKnob->tw->rotate(angle);
    previewKnob->tw->translate(center.neg());
  }

  void addKnob(int x, int y, ComputerscareMolyPatrix* module, int index,
               int row, int column, int theme = 0, bool randomizable = true) {
    if (index >= 0 && index < numKnobs) {
      ColorableSmallKnob* matrixKnob = createParam<ColorableSmallKnob>(
          Vec(x, y), module, ComputerscareMolyPatrix::KNOB + index);
      if (module) {
        matrixKnob->colorModeParam =
            &module->params[ComputerscareMolyPatrix::COLOR_MODE];
        matrixKnob->colorStrengthParam =
            &module->params[ComputerscareMolyPatrix::COLOR_STRENGTH];
        matrixKnob->colorHueParam =
            &module->params[ComputerscareMolyPatrix::COLOR_HUE];
        matrixKnob->fadeParam =
            &module->params[ComputerscareMolyPatrix::COLOR_FADE];
        matrixKnob->numInputChannels = &module->numInputChannels;
        matrixKnob->polyChannels = &module->polyChannels;
      } else {
        matrixKnob->useDisplayState = true;
        matrixKnob->displayValue = -2.f + random::uniform() * 4.f;
        matrixKnob->displayColorMode = previewColorMode;
        matrixKnob->displayColorStrength = previewColorStrength;
        matrixKnob->displayColorHue = previewColorHue;
        matrixKnob->displayFade = previewFade;
        matrixKnob->displayNumInputChannels = previewNumInputChannels;
        matrixKnob->displayPolyChannels = previewPolyChannels;
      }
      matrixKnob->inputChannel = row;
      matrixKnob->outputChannel = column;
      addParam(matrixKnob);
      return;
    }

    knob = createParam<DisableableSmallKnob>(
        Vec(x, y), module, ComputerscareMolyPatrix::KNOB + index);

    knob->module = module;
    if (!module) {
      knob->previewMode = true;
      knob->previewNumInputChannels = previewNumInputChannels;
      knob->previewPolyChannels = previewPolyChannels;
      knob->previewValue = -2.f + random::uniform() * 4.f;
    }
    knob->inputChannel = row;
    knob->outputChannel = column;
    knob->themeIndex = theme;
    knob->randomizable = randomizable;
    addParam(knob);
  }
  void appendContextMenu(Menu* menu) override {
    ComputerscareMolyPatrix* moly =
        dynamic_cast<ComputerscareMolyPatrix*>(this->module);
    if (!moly) {
      return;
    }

    menu->addChild(new MenuSeparator);
    menu->addChild(new MenuToggle(
        moly->paramQuantities[ComputerscareMolyPatrix::COLOR_MODE]));
    menu->addChild(new MenuParamSlider(
        moly->paramQuantities[ComputerscareMolyPatrix::COLOR_STRENGTH]));
    menu->addChild(new MenuParamSlider(
        moly->paramQuantities[ComputerscareMolyPatrix::COLOR_HUE]));
    menu->addChild(new MenuParamSlider(
        moly->paramQuantities[ComputerscareMolyPatrix::COLOR_FADE]));
  }
  DisableableSmallKnob* knob;
  SmoothKnobNoRandom* inputOffsetKnob;
  SmoothKnobNoRandom* outputOffsetKnob;
  PolyOutputChannelsWidget* channelWidget;
  SmallLetterDisplay* smallLetterDisplay;
};

Model* modelComputerscareMolyPatrix =
    createModel<ComputerscareMolyPatrix, ComputerscareMolyPatrixWidget>(
        "computerscare-moly-patrix");
