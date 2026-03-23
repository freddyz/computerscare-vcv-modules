#pragma once

#include "ComputerscareMetarbo.hpp"

// Full view: controls on left, inputs, square display block in middle, outputs
struct MetarboFullView : Widget {
  ComputerscareMetarbo* module;

  MetarboFullView(ComputerscareMetarbo* mod) {
    module = mod;
  }

  static void addControls(ModuleWidget* mw, ComputerscareMetarbo* module) {
    float controlX = 6.f;
    float yStart = 46.f;
    float ySpacing = 38.f;

    for (int i = 0; i < METARBO_NUM_CONTROLS; i++) {
      mw->addParam(createParam<SmallKnob>(
          Vec(controlX, yStart + ySpacing * i), module,
          ComputerscareMetarbo::CONTROL_KNOB + i));
    }
  }

  static void addPorts(ModuleWidget* mw, ComputerscareMetarbo* module) {
    float controlsWidth = 30.f;
    float inputX = controlsWidth + 4.f;
    // Square block sits between inputs and outputs
    float squareSize = 150.f;
    float outputX = controlsWidth + 34.f + squareSize + 10.f;
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
    return metarboViewWidths[VIEW_FULL] * RACK_GRID_WIDTH;
  }
};

// Square display block widget for the Full view
struct MetarboSquareDisplay : Widget {
  ComputerscareMetarbo* module;

  MetarboSquareDisplay(ComputerscareMetarbo* mod) {
    module = mod;
    box.size = Vec(150.f, 150.f);
  }

  void draw(const DrawArgs& args) override {
    // Draw a placeholder square
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
    nvgStrokeWidth(args.vg, 2.f);
    nvgStroke(args.vg);

    nvgFillColor(args.vg, nvgRGBA(0x20, 0x20, 0x20, 0x80));
    nvgFill(args.vg);

    // Label
    std::shared_ptr<Font> font =
        APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
    if (font) {
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, 14.f);
      nvgFillColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
      nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(args.vg, box.size.x / 2, box.size.y / 2, "METARBO", NULL);
    }

    Widget::draw(args);
  }
};
