#pragma once

#include "../Computerscare.hpp"
#include "../ComputerscarePolyModule.hpp"

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

  ComputerscareTheHumors();
  void process(const ProcessArgs& args) override;
  void checkPoly() override;
};
