#pragma once

#include "../Computerscare.hpp"
#include "../ComputerscarePolyModule.hpp"

static const int METARBO_NUM_INPUTS = 8;
static const int METARBO_NUM_OUTPUTS = 8;
static const int METARBO_NUM_CONTROLS = 8;

enum MetarboViewMode {
  VIEW_IO_ONLY = 0,
  VIEW_CONTROLS_IO = 1,
  VIEW_FULL = 2,
  NUM_VIEW_MODES = 3
};

static const char* metarboViewModeNames[] = {
    "I/O Only", "Controls + I/O", "Full"};

// Width in HP for each view mode
static const int metarboViewWidths[] = {4, 8, 20};

struct ComputerscareMetarbo : ComputerscarePolyModule {
  enum ParamIds {
    VIEW_MODE_PARAM,
    CONTROL_KNOB,
    PROGRAM_PARAM = CONTROL_KNOB + METARBO_NUM_CONTROLS,
    NUM_PARAMS
  };
  enum InputIds {
    SIGNAL_INPUT,
    NUM_INPUTS = SIGNAL_INPUT + METARBO_NUM_INPUTS
  };
  enum OutputIds {
    SIGNAL_OUTPUT,
    NUM_OUTPUTS = SIGNAL_OUTPUT + METARBO_NUM_OUTPUTS
  };
  enum LightIds { NUM_LIGHTS };

  MetarboViewMode viewMode = VIEW_IO_ONLY;
  int currentProgram = 0;

  ComputerscareMetarbo() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configSwitch(VIEW_MODE_PARAM, 0.f, 2.f, 0.f, "View Mode",
                 {"I/O Only", "Controls + I/O", "Full"});
    getParamQuantity(VIEW_MODE_PARAM)->randomizeEnabled = false;
    getParamQuantity(VIEW_MODE_PARAM)->resetEnabled = false;

    for (int i = 0; i < METARBO_NUM_CONTROLS; i++) {
      configParam(CONTROL_KNOB + i, 0.f, 10.f, 0.f,
                  "Control " + std::to_string(i + 1));
    }

    configParam(PROGRAM_PARAM, 0.f, 3.f, 0.f, "Program");
    getParamQuantity(PROGRAM_PARAM)->randomizeEnabled = false;
    getParamQuantity(PROGRAM_PARAM)->snapEnabled = true;

    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      configInput(SIGNAL_INPUT + i, "Signal " + std::to_string(i + 1));
    }
    for (int i = 0; i < METARBO_NUM_OUTPUTS; i++) {
      configOutput(SIGNAL_OUTPUT + i, "Signal " + std::to_string(i + 1));
    }
  }

  void process(const ProcessArgs& args) override {
    // Sync view mode from param
    viewMode = (MetarboViewMode)(int)params[VIEW_MODE_PARAM].getValue();

    // Pass-through: each input passes all poly channels to corresponding output
    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      int channels = inputs[SIGNAL_INPUT + i].getChannels();
      if (channels == 0) channels = 1;
      outputs[SIGNAL_OUTPUT + i].setChannels(channels);
      for (int c = 0; c < channels; c++) {
        outputs[SIGNAL_OUTPUT + i].setVoltage(
            inputs[SIGNAL_INPUT + i].getVoltage(c), c);
      }
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "viewMode", json_integer((int)viewMode));
    json_object_set_new(rootJ, "currentProgram", json_integer(currentProgram));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* viewModeJ = json_object_get(rootJ, "viewMode");
    if (viewModeJ) {
      viewMode = (MetarboViewMode)json_integer_value(viewModeJ);
      params[VIEW_MODE_PARAM].setValue((float)viewMode);
    }
    json_t* programJ = json_object_get(rootJ, "currentProgram");
    if (programJ) {
      currentProgram = json_integer_value(programJ);
    }
  }
};
