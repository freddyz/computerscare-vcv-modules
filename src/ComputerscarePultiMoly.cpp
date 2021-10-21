#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

struct ComputerscarePultiMoly;

const int numKnobs = 16;
const int numToggles = 16;

const int numInputs = 16;
const int numOutputs = 16;
const int numMultiphony = 16;
const int numInputCities = 16;




struct ComputerscarePultiMoly : ComputerscarePolyModule {
  ComputerscareSVGPanel* panelRef;
  enum ParamIds {
    KNOB,
    TOGGLES = KNOB + numKnobs * numMultiphony,
    POLY_CHANNELS = TOGGLES + numToggles,
    GLOBAL_SCALE,
    GLOBAL_OFFSET,
    EDITING_CITY,
    EDITING_INPUT,
    EDITING_A,
    EDITING_B,
    NUM_PARAMS

  };
  enum InputIds {
    CITY_INPUT,
    NUM_INPUTS = CITY_INPUT + numInputs
  };
  enum OutputIds {
    CITY_OUTPUT,
    NUM_OUTPUTS = CITY_OUTPUT + numOutputs
  };
  enum LightIds {
    NUM_LIGHTS
  };


  ComputerscarePultiMoly()  {

    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    for (int i = 0; i < numKnobs * numMultiphony; i++) {
      configParam(KNOB + i, 0.f, 10.f, 0.f, "City " + std::to_string(((i - i % numKnobs)) / numKnobs + 1) + ", ch " + std::to_string((i % numKnobs) + 1));
    }
    for (int i = 0; i < numInputCities; i++) {
      configInput(CITY_INPUT + i, "City " + std::to_string(i + 1) + " Input");
      configOutput(CITY_OUTPUT + i, "City " + std::to_string(i + 1) + " Output");

    }
    configParam(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels");
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");

    configParam(EDITING_CITY, 1.f, 16.f, 1.f, "Editing City");

    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;

  }
  void process(const ProcessArgs &args) override {
    ComputerscarePolyModule::checkCounter();
    float trim = params[GLOBAL_SCALE].getValue();
    float offset = params[GLOBAL_OFFSET].getValue();
    for (int i = 0; i < polyChannels; i++) {
      outputs[CITY_OUTPUT].setVoltage(params[KNOB + i].getValue()*trim + offset, i);
    }
  }
  void checkPoly() override {
    polyChannels = params[POLY_CHANNELS].getValue();
    if (polyChannels == 0) {
      polyChannels = 16;
      params[CITY_OUTPUT].setValue(16);
    }
    outputs[CITY_OUTPUT].setChannels(polyChannels);
  }
};

struct NoRandomSmallKnob : SmallKnob {
  NoRandomSmallKnob() {
    SmallKnob();
  };
};
struct NoRandomMediumSmallKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));

  NoRandomMediumSmallKnob() {
    setSvg(enabledSvg);
    RoundKnob();
  };
};

struct DisableableSmoothKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  ComputerscarePolyModule *module;

  DisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }
  void step() override {
    if (module) {
      bool candidate = channel > module->polyChannels - 1;
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        onChange(*(new event::Change()));
        fb->dirty = true;
        disabled = candidate;
      }
    }
    else {
    }
    RoundKnob::step();
  }
};

struct Block : OpaqueWidget {
  PolyOutputChannelsWidget* channelWidget;
  PolyChannelsDisplay* channelDisplay;
  DisableableSmoothKnob* fader;
  SmallLetterDisplay* smallLetterDisplay;

  Block(ComputerscarePultiMoly *module, int startParamNum) {

  }
};

struct ComputerscarePultiMolyWidget : ModuleWidget {
  ComputerscarePultiMolyWidget(ComputerscarePultiMoly *module) {
    setModule(module);
    //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePultiMolyPanel.svg")));
    box.size = Vec(4 * 4 * 15, 380);
    {
      ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePultiMolyPanel.svg")));
      addChild(panel);
    }



    channelWidget = new PolyOutputChannelsWidget(Vec(1, 24), module, ComputerscarePultiMoly::POLY_CHANNELS);
    // addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module, ComputerscarePultiMoly::CITY_OUTPUT));

    addChild(channelWidget);



    addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module, ComputerscarePultiMoly::GLOBAL_SCALE));
    addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscarePultiMoly::GLOBAL_OFFSET));


    addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscarePultiMoly::EDITING_CITY));



    float xx;
    float yy;

    int numKnobBlocks = 2;

    float inputXStart = 0;
    float inputXSpacing = 50;
    float inputYStart = 20;

    float yInitial = 86;
    float ySpacing =  34;

    float yRightColumnOffset = 14.3 / 8;

    float xInitial = 10 + inputXSpacing;
    float citySpacing = 60;

    float outputXStart = inputXStart + inputXSpacing + numKnobBlocks * citySpacing + 20;
    float outputYStart = 20;




    for (int cityInput = 0; cityInput < numInputCities; cityInput++) {
      xx = inputXStart + 1.4f + 24.3 * (cityInput - cityInput % 8) / 8;
      yy = inputYStart + ySpacing * (cityInput % 8) +  yRightColumnOffset * (cityInput - cityInput % 8) ;
      //addLabeledKnob(std::to_string(i + 1), xx, yy, module, i + block * numKnobs, 0, 0);
      addInput(createInput<InPort>(Vec(xx, yy), module, ComputerscarePultiMoly::CITY_INPUT + cityInput));
    }

    for (int block = 0; block < numKnobBlocks; block++) {
      for (int i = 0; i < numKnobs; i++) {
        xx = xInitial + block * citySpacing + 1.4f + 24.3 * (i - i % 8) / 8;
        yy = yInitial + ySpacing * (i % 8) +  yRightColumnOffset * (i - i % 8) ;
        addLabeledKnob(std::to_string(i + 1), xx, yy, module, i + block * numKnobs, 0, 0);
      }
    }

    for (int cityInput = 0; cityInput < numInputCities; cityInput++) {
      xx = outputXStart + 1.4f + 24.3 * (cityInput - cityInput % 8) / 8;
      yy = outputYStart + ySpacing * (cityInput % 8) +  yRightColumnOffset * (cityInput - cityInput % 8) ;
      //addLabeledKnob(std::to_string(i + 1), xx, yy, module, i + block * numKnobs, 0, 0);
      addOutput(createOutput<PointingUpPentagonPort>(Vec(xx, yy), module, ComputerscarePultiMoly::CITY_OUTPUT + cityInput));
    }


  }
  void addLabeledKnob(std::string label, int x, int y, ComputerscarePultiMoly *module, int index, float labelDx, float labelDy) {

    smallLetterDisplay = new SmallLetterDisplay();
    smallLetterDisplay->box.size = Vec(5, 10);
    smallLetterDisplay->fontSize = 18;
    smallLetterDisplay->value = label;
    smallLetterDisplay->textAlign = 1;

    ParamWidget* pob =  createParam<DisableableSmoothKnob>(Vec(x, y), module, ComputerscarePultiMoly::KNOB + index);

    DisableableSmoothKnob* fader = dynamic_cast<DisableableSmoothKnob*>(pob);
    fader->module = module;
    fader->channel = index;

    addParam(fader);
    smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);
    addChild(smallLetterDisplay);
  }
  PolyOutputChannelsWidget* channelWidget;
  PolyChannelsDisplay* channelDisplay;
  DisableableSmoothKnob* fader;
  SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscarePultiMoly = createModel<ComputerscarePultiMoly, ComputerscarePultiMolyWidget>("computerscare-pulti-moly");
