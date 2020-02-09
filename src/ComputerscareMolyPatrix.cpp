#include "Computerscare.hpp"

struct ComputerscareMolyPatrix;

const int numKnobs = 256;
const int numRows =16;
const int numColumns=16;

const int numOutputs = 1;

struct ComputerscareMolyPatrix : Module {
  int counter = 0;
  ComputerscareSVGPanel* panelRef;
  enum ParamIds {
    KNOB,
    NUM_PARAMS = KNOB + numKnobs

  };
  enum InputIds {
    CHANNEL_INPUT,
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
    for(int n = 0; n < numKnobs; n++) {
      configParam(KNOB+n,-1.f,1.f,0.f);
    }
   /* for (int i = 0; i < numRows; i++) {
      for(int j = 0; j < numColumns; j++) {
      configParam(KNOB + i*16+j, -10.f, 10.f, 0.f);
    }
    }*/

  }
  void process(const ProcessArgs &args) override {
    counter++;
    if (counter > 5012) {
      //printf("%f \n",random::uniform());
      counter = 0;
      //rect4032
      //south facing high wall
    }
    outputs[POLY_OUTPUT].setChannels(16);
    for (int i = 0; i < 16; i++) {
      outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue(), i);
    }
  }

};

struct ComputerscareMolyPatrixWidget : ModuleWidget {
  ComputerscareMolyPatrixWidget(ComputerscareMolyPatrix *module) {

    setModule(module);
    //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareMolyPatrixPanel.svg")));
    box.size = Vec(12 * 15, 380);
    {
      ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));

      //module->panelRef = panel;

      addChild(panel);

    }
    float xx;
    float yy;
    for (int i = 0; i < numRows; i++) {
      for(int j = 0; j < numColumns; j++) {
      xx = 10+j*10;
      yy = 10+i*10;
      addLabeledKnob(std::to_string(i + 1), xx, yy, module, i*16+j, 0, 0);
    }
    }



    addOutput(createOutput<PointingUpPentagonPort>(Vec(28, 24), module, ComputerscareMolyPatrix::POLY_OUTPUT));

  }
  void addLabeledKnob(std::string label, int x, int y, ComputerscareMolyPatrix *module, int index, float labelDx, float labelDy) {

    smallLetterDisplay = new SmallLetterDisplay();
    smallLetterDisplay->box.size = Vec(5, 10);
    smallLetterDisplay->fontSize = 16;
    smallLetterDisplay->value = label;
    smallLetterDisplay->textAlign = 1;

    addParam(createParam<ComputerscareDotKnob>(Vec(x, y), module, ComputerscareMolyPatrix::KNOB + index));
    smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


    addChild(smallLetterDisplay);

  }
  SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareMolyPatrix = createModel<ComputerscareMolyPatrix, ComputerscareMolyPatrixWidget>("computerscare-moly-patrix");
