#include "TheHumors.hpp"

#include "dsp/Passthrough.hpp"

ComputerscareTheHumors::ComputerscareTheHumors() {
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

void ComputerscareTheHumors::process(const ProcessArgs& args) {
  ComputerscarePolyModule::checkCounter();
  the_humors::dsp::processPassthrough(inputs, outputs, CLOCK_INPUT,
                                      CLOCK_OUTPUT, HUMORS_COUNT);
}

void ComputerscareTheHumors::checkPoly() {
  polyChannels = params[POLY_CHANNELS].getValue();
  if (polyChannels == 0) {
    polyChannels = 16;
    params[POLY_CHANNELS].setValue(16);
  }
}
