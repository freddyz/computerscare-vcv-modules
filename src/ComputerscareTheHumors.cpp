#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

static const int HUMORS_COUNT = 4;

struct ComputerscareTheHumors : ComputerscarePolyModule {
  enum ParamIds {
    HUMOR_KNOB,
    HUMOR_ATTEN = HUMOR_KNOB + HUMORS_COUNT,
    POLY_CHANNELS = HUMOR_ATTEN + HUMORS_COUNT,
    PROGRAM_KNOB,
    NUM_PARAMS
  };
  enum InputIds {
    CLOCK_INPUT,
    RHYTHM_INPUT,
    CV_INPUT,
    AUDIO_INPUT,
    HUMOR_CV_INPUT,
    PROGRAM_CV_INPUT = HUMOR_CV_INPUT + HUMORS_COUNT,
    NUM_INPUTS
  };
  enum OutputIds {
    CLOCK_OUTPUT,
    RHYTHM_OUTPUT,
    CV_OUTPUT,
    AUDIO_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds { NUM_LIGHTS };

  ComputerscareTheHumors() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    const std::string humorLabels[HUMORS_COUNT] = {"Blood", "Yellow Bile",
                                                   "Black Bile", "Phlegm"};
    for (int i = 0; i < HUMORS_COUNT; i++) {
      configParam(HUMOR_KNOB + i, 0.f, 1.f, 0.f, humorLabels[i]);
      configParam(HUMOR_ATTEN + i, -1.f, 1.f, 0.f,
                  humorLabels[i] + " CV Attenuverter");
      configInput(HUMOR_CV_INPUT + i, humorLabels[i] + " CV");
    }

    configSwitch(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels",
                 polyChannelLabels(false));
    configParam(PROGRAM_KNOB, 0.f, 10.f, 0.f, "Program");
    configInput(PROGRAM_CV_INPUT, "Program CV");

    configInput(CLOCK_INPUT, "Clock");
    configInput(RHYTHM_INPUT, "Rhythm");
    configInput(CV_INPUT, "CV");
    configInput(AUDIO_INPUT, "Audio");

    configOutput(CLOCK_OUTPUT, "Clock");
    configOutput(RHYTHM_OUTPUT, "Rhythm");
    configOutput(CV_OUTPUT, "CV");
    configOutput(AUDIO_OUTPUT, "Audio");

    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS)->resetEnabled = false;
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();
    for (int i = 0; i < HUMORS_COUNT; i++) {
      const int channels = inputs[CLOCK_INPUT + i].getChannels();
      outputs[CLOCK_OUTPUT + i].setChannels(channels);
      for (int c = 0; c < channels; c++) {
        outputs[CLOCK_OUTPUT + i].setVoltage(
            inputs[CLOCK_INPUT + i].getVoltage(c), c);
      }
    }
  }

  void checkPoly() override {
    polyChannels = params[POLY_CHANNELS].getValue();
    if (polyChannels == 0) {
      polyChannels = 16;
      params[POLY_CHANNELS].setValue(16);
    }
  }
};

struct TheHumorsLabel : SmallLetterDisplay {
  TheHumorsLabel(const std::string& label, Vec pos, float size = 14.f,
                 NVGcolor color = BLACK, int align = 1, float rowWidth = 82.f) {
    box.pos = pos;
    box.size = Vec(rowWidth, 12);
    fontSize = size;
    value = label;
    textAlign = align;
    textColor = color;
    breakRowWidth = rowWidth;
  }
};

struct ComputerscareTheHumorsWidget : ModuleWidget {
  ComputerscareTheHumorsWidget(ComputerscareTheHumors* module) {
    setModule(module);
    box.size = Vec(16 * 15, 380);

    ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
    panel->box.size = box.size;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareTheHumorsPanel.svg")));
    addChild(panel);

    addChild(new PolyOutputChannelsWidget(
        Vec(202, 24), module, ComputerscareTheHumors::POLY_CHANNELS));

    addChild(new TheHumorsLabel("Program", Vec(111, 18), 15.f, BLACK, 2, 82.f));
    addParam(createParam<ScrambleKnob>(Vec(126, 34), module,
                                       ComputerscareTheHumors::PROGRAM_KNOB));
    addInput(createInput<InPort>(Vec(160, 28), module,
                                 ComputerscareTheHumors::PROGRAM_CV_INPUT));

    const std::string humorLabels[HUMORS_COUNT] = {"Blood", "Yellow Bile",
                                                   "Black Bile", "Phlegm"};
    for (int i = 0; i < HUMORS_COUNT; i++) {
      const float controlX = (i % 2 == 0) ? 20.f : 130.f;
      const float controlY = (i / 2 == 0) ? 72.f : 190.f;
      const float knobX = controlX + 26.f;
      const float knobY = controlY + 20.f;
      addChild(new TheHumorsLabel(humorLabels[i], Vec(knobX - 42, knobY - 22),
                                  19.f, BLACK, 2, 112.f));
      addParam(createParam<BigSmoothKnob>(
          Vec(knobX, knobY), module, ComputerscareTheHumors::HUMOR_KNOB + i));
      addParam(createParam<SmallKnob>(Vec(controlX + 20, controlY + 66), module,
                                      ComputerscareTheHumors::HUMOR_ATTEN + i));
      addInput(createInput<InPort>(Vec(controlX + 49, controlY + 62), module,
                                   ComputerscareTheHumors::HUMOR_CV_INPUT + i));
    }

    const std::string ioLabels[HUMORS_COUNT] = {"clock", "rhythm", "cv",
                                                "audio"};
    for (int i = 0; i < HUMORS_COUNT; i++) {
      const float inputX = 28.f + (i % 2) * 44.f;
      const float outputX = 136.f + (i % 2) * 44.f;
      const float y = (i / 2 == 0) ? 300.f : 344.f;
      addChild(new TheHumorsLabel(ioLabels[i], Vec(inputX - 29.5f, y - 14),
                                  13.f, BLACK, 2, 74.f));
      addInput(createInput<InPort>(Vec(inputX, y), module,
                                   ComputerscareTheHumors::CLOCK_INPUT + i));
      addChild(new TheHumorsLabel(ioLabels[i], Vec(outputX - 29.5f, y - 14),
                                  13.f, BLACK, 2, 74.f));
      addOutput(createOutput<InPort>(Vec(outputX, y), module,
                                     ComputerscareTheHumors::CLOCK_OUTPUT + i));
    }
  }
};

Model* modelComputerscareTheHumors =
    createModel<ComputerscareTheHumors, ComputerscareTheHumorsWidget>(
        "computerscare-the-humors");
