#pragma once

#include "ComputerscareMetarbo.hpp"

// I/O Only view: narrow module showing just inputs and outputs
struct MetarboIOView : Widget {
  ComputerscareMetarbo* module;

  MetarboIOView(ComputerscareMetarbo* mod) {
    module = mod;
  }

  // Called by parent widget to add ports when this view is active
  static void addPorts(ModuleWidget* mw, ComputerscareMetarbo* module) {
    float inputX = 4.f;
    float outputX = 34.f;
    float yStart = 46.f;
    float ySpacing = 38.f;

    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      mw->addInput(createInput<InPort>(
          Vec(inputX, yStart + ySpacing * i), module,
          ComputerscareMetarbo::SIGNAL_INPUT + i));
    }
    for (int i = 0; i < METARBO_NUM_OUTPUTS; i++) {
      mw->addOutput(createOutput<OutPort>(
          Vec(outputX, yStart + ySpacing * i), module,
          ComputerscareMetarbo::SIGNAL_OUTPUT + i));
    }
  }

  static float getWidth() {
    return metarboViewWidths[VIEW_IO_ONLY] * RACK_GRID_WIDTH;
  }
};
