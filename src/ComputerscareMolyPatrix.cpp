#include "Computerscare.hpp"

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
  enum OutputIds {
    POLY_OUTPUT,
    NUM_OUTPUTS = POLY_OUTPUT + numOutputs
  };
  enum LightIds {
    NUM_LIGHTS
  };


  ComputerscareMolyPatrix()  {

    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    /*for(int n = 0; n < numKnobs; n++) {
      configParam(KNOB+n,-1.f,1.f,0.f);
    }*/
    for (int i = 0; i < numRows; i++) {
      configParam(INPUT_ROW_TRIM + i, -2.f, 2.f, 1.f, "Input Channel " + std::to_string(i + 1) + " Attenuation");
      configParam(OUTPUT_COLUMN_TRIM + i, -2.f, 2.f, 1.f, "Output Channel " + std::to_string(i + 1) + " Attenuation");

      getParamQuantity(INPUT_ROW_TRIM + i)->randomizeEnabled = false;
      getParamQuantity(OUTPUT_COLUMN_TRIM + i)->randomizeEnabled = false;

      for (int j = 0; j < numColumns; j++) {
        configParam(KNOB + i * 16 + j, -2.f, 2.f, i == j ? 1.f : 0.f, "Input ch." + std::to_string(i + 1) + " → Output ch." + std::to_string(j + 1));
      }

    }
    configParam(OUTPUT_TRIM, -2.f, 2.f, 1.f, "Output Attenuation");
    configParam(OUTPUT_OFFSET, -10.f, 10.f, 0.f, "Output Offset");
    configParam(INPUT_TRIM, -2.f, 2.f, 1.f, "Input Attenuation");
    configParam(INPUT_OFFSET, -10.f, 10.f, 0.f, "Input Offset");
    getParamQuantity(OUTPUT_TRIM)->randomizeEnabled = false;
    getParamQuantity(OUTPUT_OFFSET)->randomizeEnabled = false;
    getParamQuantity(INPUT_TRIM)->randomizeEnabled = false;
    getParamQuantity(INPUT_OFFSET)->randomizeEnabled = false;


    configParam<AutoParamQuantity>(POLY_CHANNELS, 0.f, 16.f, 0.f, "Poly Channels");
    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS)->resetEnabled = false;

    configInput(POLY_INPUT, "Main");

    configInput(INPUT_ATTENUATION_CV, "Input Attenuation");
    configInput(OUTPUT_ATTENUATION_CV, "Output Attenuation");

    configOutput(POLY_OUTPUT, "Main");

  }
  void checkPoly() override {
    numInputChannels = inputs[POLY_INPUT].getChannels();
    int knobSetting = params[POLY_CHANNELS].getValue();
    if (numInputChannels > 0) {
      if (knobSetting == 0) {
        polyChannels = numInputChannels;
      }
      else {
        polyChannels = knobSetting;
      }
    } else {
      polyChannels = knobSetting == 0 ? 16 : knobSetting;
    }
    outputs[POLY_OUTPUT].setChannels(polyChannels);
  }
  void process(const ProcessArgs &args) override {
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
        outVoltage += params[KNOB + i * 16 + outIndex].getValue() * (inputVals[i] + inOffset) * params[INPUT_ROW_TRIM + i].getValue() * (params[INPUT_TRIM].getValue() * (numInputTrimChannels > 0 ? inputTrims[numInputTrimChannels == 1 ? 0 : i] / 10 : 1));
      }
      outputs[POLY_OUTPUT].setVoltage(params[OUTPUT_COLUMN_TRIM + outIndex].getValue() * (outTrim * (numOutputTrimChannels > 0 ? outputTrims[numOutputTrimChannels == 1 ? 0 : outIndex] / 10 : 1)) * outVoltage + outOffset, outIndex);
    }
  }
};



struct DisableableSmallKnob : RoundKnob {

  std::vector<std::shared_ptr<Svg>> enabledThemes = {APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg")), APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed-dark.svg"))};


  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed-disabled.svg"));

  int inputChannel = 0;
  int outputChannel = 0;
  int themeIndex = 0;
  bool disabled = false;
  bool initialized = false;
  bool randomizable = true;
  ComputerscareMolyPatrix *module;

  DisableableSmallKnob() {
    setSvg(enabledThemes[themeIndex]);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  void draw(const DrawArgs& args) override {
    if (module) {
      bool candidateDisabled = (module->numInputChannels != 0 && inputChannel > module->numInputChannels - 1 || outputChannel > module->polyChannels - 1) ;
      if (disabled != candidateDisabled || !initialized) {
        setSvg(candidateDisabled ? disabledSvg : enabledThemes[themeIndex]);
        disabled = candidateDisabled;
        onChange(*(new event::Change()));
        fb->dirty = true;
        initialized = true;
      }
    }
    else {
    }
    RoundKnob::draw(args);
  }
};

struct ComputerscareMolyPatrixWidget : ModuleWidget {

  ComputerscareMolyPatrixWidget(ComputerscareMolyPatrix *module) {

    setModule(module);

    //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareMolyPatrixPanel.svg")));
    box.size = Vec(28 * 15, 380);
    {
      ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareMolyPatrixPanel.svg")));

      //module->panelRef = panel;

      addChild(panel);

    }
    float xx;
    float yy;
    float x0 = 30;
    float dx = 21.4;
    float y0 = 43;
    float dy = 21;

    float inX = 28;

    addInput(createInput<PointingUpPentagonPort>(Vec(inX + 0, 12), module, ComputerscareMolyPatrix::POLY_INPUT));
    addKnob(inX + 28, 12, module, ComputerscareMolyPatrix::INPUT_TRIM, 0, 0, 1, 0);
    addInput(createInput<TinyJack>(Vec(inX + 41, 25), module, ComputerscareMolyPatrix::INPUT_ATTENUATION_CV));

    addParam(createParam<SmoothKnobNoRandom>(Vec(inX + 58, 14), module, ComputerscareMolyPatrix::INPUT_OFFSET));


    //addKnob(60, 16, module, ComputerscareMolyPatrix::INPUT_TRIM, 0, 0,1,0);


    for (int i = 0; i < numRows; i++) {
      for (int j = 0; j < numColumns; j++) {
        xx = x0 + j * dx;
        yy = y0 + i * dy;
        addKnob( xx, yy, module, i * 16 + j, i, j);
      }
      addKnob( x0 - 25, y0 + i * dy, module, ComputerscareMolyPatrix::INPUT_ROW_TRIM + i, i, 0, 1, 0);
      addKnob( 420 - 40, y0 + i * dy, module, ComputerscareMolyPatrix::OUTPUT_COLUMN_TRIM + i, 0, i, 1, 0);

    }


    float outX = 302;


    addKnob(outX + 0, 1, module, ComputerscareMolyPatrix::OUTPUT_TRIM, 0, 0, 1, 0);
    addInput(createInput<TinyJack>(Vec(outX + 10, 15), module, ComputerscareMolyPatrix::OUTPUT_ATTENUATION_CV));

    addParam(createParam<SmoothKnobNoRandom>(Vec(outX + 28, 4), module, ComputerscareMolyPatrix::OUTPUT_OFFSET));

    channelWidget = new PolyOutputChannelsWidget(Vec(outX + 55, 1), module, ComputerscareMolyPatrix::POLY_CHANNELS);
    addChild(channelWidget);

    addOutput(createOutput<InPort>(Vec(outX + 80, 1), module, ComputerscareMolyPatrix::POLY_OUTPUT));


  }
  void addKnob(int x, int y, ComputerscareMolyPatrix *module, int index, int row, int column, int theme = 0, bool randomizable = true) {

    knob = createParam<DisableableSmallKnob>(Vec(x, y), module, ComputerscareMolyPatrix::KNOB + index);

    knob->module = module;
    knob->inputChannel = row;
    knob->outputChannel = column;
    knob->themeIndex = theme;
    knob->randomizable = randomizable;
    addParam(knob);

  }
  DisableableSmallKnob* knob;
  PolyOutputChannelsWidget* channelWidget;
  SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareMolyPatrix = createModel<ComputerscareMolyPatrix, ComputerscareMolyPatrixWidget>("computerscare-moly-patrix");
