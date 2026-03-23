#pragma once

#include "ComputerscareMetarbo.hpp"

// Controls + I/O view: controls on left, inputs, then outputs
struct MetarboControlsIOView : Widget {
  ComputerscareMetarbo* module;

  MetarboControlsIOView(ComputerscareMetarbo* mod) {
    module = mod;
  }

  static void addControls(ModuleWidget* mw, ComputerscareMetarbo* module) {
    float controlX = 4.f;
    float yStart = 48.f;
    float ySpacing = 38.f;

    for (int i = 0; i < METARBO_NUM_CONTROLS; i++) {
      mw->addParam(createParam<SmoothKnob>(
          Vec(controlX, yStart + ySpacing * i), module,
          ComputerscareMetarbo::CONTROL_KNOB + i));
    }
  }

  static void addPorts(ModuleWidget* mw, ComputerscareMetarbo* module) {
    float controlsWidth = 30.f;
    float inputX = controlsWidth + 8.f;
    float outputX = controlsWidth + 52.f;
    float yStartInput = 46.f;
    float yStartOutput = 44.f;
    float ySpacing = 38.f;

    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      mw->addInput(createInput<InPort>(
          Vec(inputX, yStartInput + ySpacing * i), module,
          ComputerscareMetarbo::SIGNAL_INPUT + i));
    }
    for (int i = 0; i < METARBO_NUM_OUTPUTS; i++) {
      mw->addOutput(createOutput<OutPort>(
          Vec(outputX, yStartOutput + ySpacing * i), module,
          ComputerscareMetarbo::SIGNAL_OUTPUT + i));
    }
  }

  static float getWidth() {
    return metarboViewWidths[VIEW_CONTROLS_IO] * RACK_GRID_WIDTH;
  }
};
