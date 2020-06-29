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
        configParam(KNOB + i * 16 + j, -2.f, 2.f, i == j ? 1.f : 0.f, "Input ch." + std::to_string(i+1) + " → Output ch." + std::to_string(j+1));
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
        out += params[KNOB + i * 16 + j].getValue() * inputs[POLY_INPUT].getVoltage(i)*params[INPUT_ROW_TRIM+i].getValue()*params[INPUT_TRIM].getValue();
      }
      outputs[POLY_OUTPUT].setVoltage(out, j);
    }
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
    addLabeledKnob("",36,16,module,ComputerscareMolyPatrix::INPUT_TRIM,0,0);

    for (int i = 0; i < numRows; i++) {
      for (int j = 0; j < numColumns; j++) {
        xx = x0 + j * dx;
        yy = y0 + i * dy;
        addLabeledKnob(std::to_string(i + 1), xx, yy, module, i * 16 + j, 0, 0);
      }
      addLabeledKnob("",x0-25,y0+i*dy,module,ComputerscareMolyPatrix::INPUT_ROW_TRIM+i,0,0);
    }



    channelWidget = new PolyOutputChannelsWidget(Vec(290, 1), module, ComputerscareMolyPatrix::POLY_CHANNELS);
    addChild(channelWidget);
    addOutput(createOutput<InPort>(Vec(318, 1), module, ComputerscareMolyPatrix::POLY_OUTPUT));

  }
  void addLabeledKnob(std::string label, int x, int y, ComputerscareMolyPatrix *module, int index, float labelDx, float labelDy) {

    /* smallLetterDisplay = new SmallLetterDisplay();
     smallLetterDisplay->box.size = Vec(5, 10);
     smallLetterDisplay->fontSize = 16;
     smallLetterDisplay->value = label;
     smallLetterDisplay->textAlign = 1;*/

    addParam(createParam<ComputerscareDotKnob>(Vec(x, y), module, ComputerscareMolyPatrix::KNOB + index));
    //smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


//    addChild(smallLetterDisplay);

  }
  PolyOutputChannelsWidget* channelWidget;
  SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareMolyPatrix = createModel<ComputerscareMolyPatrix, ComputerscareMolyPatrixWidget>("computerscare-moly-patrix");
