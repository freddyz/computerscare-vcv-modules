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
      for (int j = 0; j < numColumns; j++) {
        configParam(KNOB + i * 16 + j, -2.f, 2.f, i == j ? 1.f : 0.f, "Input ch." + std::to_string(i + 1) + " → Output ch." + std::to_string(j + 1));
      }
      configParam(OUTPUT_TRIM, -2.f, 2.f, 1.f, "Output Attenuation");
      configParam(OUTPUT_OFFSET, -10.f, 10.f, 0.f, "Output Offset");
      configParam(INPUT_TRIM, -2.f, 2.f, 1.f, "Input Attenuation");

      configParam(INPUT_OFFSET, -10.f, 10.f, 0.f, "Input Offset");
      configParam<AutoParamQuantity>(POLY_CHANNELS, 0.f, 16.f, 0.f, "Poly Channels");
    }

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
    for (int j = 0; j < numRows; j++) {
      float out = 0.f;
      for (int i = 0; i < numColumns; i++) {
        out += params[KNOB + i * 16 + j].getValue() * inputs[POLY_INPUT].getVoltage(i) * params[INPUT_ROW_TRIM + i].getValue() * params[INPUT_TRIM].getValue();
      }
      outputs[POLY_OUTPUT].setVoltage(out, j);
    }
  }

};
struct DisableableSmallKnob : RoundKnob {

  std::vector<std::shared_ptr<Svg>> enabledThemes = {APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg")),APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-dark.svg"))};

  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed-disabled.svg"));

  int inputChannel = 0;
  int outputChannel = 0;
  bool disabled = false;
  ComputerscareMolyPatrix *module;

  DisableableSmallKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  void draw(const DrawArgs& args) override {
    if (module) {
      bool candidateDisabled = (module->numInputChannels !=0 && inputChannel > module->numInputChannels-1 || outputChannel > module->polyChannels-1) ;
      if (disabled != candidateDisabled) {
        setSvg(candidateDisabled ? disabledSvg : enabledSvg);
        dirtyValue = -20.f;
        disabled = candidateDisabled;
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
    box.size = Vec(26 * 15, 380);
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
    float y0 = 41;
    float dy = 21;

    addInput(createInput<PointingUpPentagonPort>(Vec(9, 12), module, ComputerscareMolyPatrix::POLY_INPUT));
    addKnob(36, 16, module, ComputerscareMolyPatrix::INPUT_TRIM, 0, 0);

    for (int i = 0; i < numRows; i++) {
      for (int j = 0; j < numColumns; j++) {
        xx = x0 + j * dx;
        yy = y0 + i * dy;
        addKnob( xx, yy, module, i * 16 + j, i,j);
      }
      addKnob( x0 - 25, y0 + i * dy, module, ComputerscareMolyPatrix::INPUT_ROW_TRIM + i,i,0);
    }

    channelWidget = new PolyOutputChannelsWidget(Vec(290, 1), module, ComputerscareMolyPatrix::POLY_CHANNELS);
    addChild(channelWidget);
    addOutput(createOutput<InPort>(Vec(318, 1), module, ComputerscareMolyPatrix::POLY_OUTPUT));

  }
  void addKnob(int x, int y, ComputerscareMolyPatrix *module, int index, int row,int column) {

    /* smallLetterDisplay = new SmallLetterDisplay();
     smallLetterDisplay->box.size = Vec(5, 10);
     smallLetterDisplay->fontSize = 16;
     smallLetterDisplay->value = label;
     smallLetterDisplay->textAlign = 1;*/

    knob = createParam<DisableableSmallKnob>(Vec(x, y), module, ComputerscareMolyPatrix::KNOB + index);
    knob->module = module;
    knob->inputChannel = row;
    knob->outputChannel = column;
    addParam(knob);
    //smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


//    addChild(smallLetterDisplay);

  }
  DisableableSmallKnob* knob;
  PolyOutputChannelsWidget* channelWidget;
  SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareMolyPatrix = createModel<ComputerscareMolyPatrix, ComputerscareMolyPatrixWidget>("computerscare-moly-patrix");
