#include "ComputerscareMetarbo.hpp"

// Square display block widget for the Full view
struct MetarboSquareDisplay : Widget {
  ComputerscareMetarbo* module;

  MetarboSquareDisplay(ComputerscareMetarbo* mod) {
    module = mod;
    box.size = Vec(150.f, 150.f);
  }

  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
    nvgStrokeWidth(args.vg, 2.f);
    nvgStroke(args.vg);
    nvgFillColor(args.vg, nvgRGBA(0x20, 0x20, 0x20, 0x80));
    nvgFill(args.vg);
    Widget::draw(args);
  }
};

struct ComputerscareMetarboWidget : ModuleWidget {
  ComputerscareMetarbo* metarboModule;
  BGPanel* bgPanel;
  MetarboSquareDisplay* squareDisplay;
  MetarboViewMode lastViewMode = VIEW_IO_ONLY;

  static constexpr float INPUT_X = 8.f;
  static constexpr float OUTPUT_X = 52.f;
  static constexpr float KNOB_X = 4.f;
  static constexpr float CONTROLS_W = 30.f;
  static constexpr float Y_IN = 46.f;
  static constexpr float Y_OUT = 44.f;
  static constexpr float Y_KNOB = 48.f;
  static constexpr float Y_SPACING = 38.f;
  static constexpr float PORT_W = 27.f;

  PortWidget* inputPorts[METARBO_NUM_INPUTS] = {};
  PortWidget* outputPorts[METARBO_NUM_OUTPUTS] = {};
  ParamWidget* controlKnobs[METARBO_NUM_CONTROLS] = {};

  ComputerscareMetarboWidget(ComputerscareMetarbo* module) {
    setModule(module);
    metarboModule = module;

    box.size =
        Vec(metarboViewWidths[VIEW_IO_ONLY] * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0xf0, 0xf0, 0xf0));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      inputPorts[i] = createInput<InPort>(Vec(0, 0), module,
                                          ComputerscareMetarbo::SIGNAL_INPUT + i);
      addInput(inputPorts[i]);
    }
    for (int i = 0; i < METARBO_NUM_OUTPUTS; i++) {
      outputPorts[i] = createOutput<OutPort>(
          Vec(0, 0), module, ComputerscareMetarbo::SIGNAL_OUTPUT + i);
      addOutput(outputPorts[i]);
    }
    for (int i = 0; i < METARBO_NUM_CONTROLS; i++) {
      controlKnobs[i] = createParam<SmoothKnob>(
          Vec(0, 0), module, ComputerscareMetarbo::CONTROL_KNOB + i);
      addParam(controlKnobs[i]);
    }

    squareDisplay = new MetarboSquareDisplay(module);
    addChild(squareDisplay);

    updateLayout(module ? module->viewMode : VIEW_IO_ONLY);
  }

  void updateLayout(MetarboViewMode mode) {
    float controlsOffset = (mode == VIEW_IO_ONLY) ? 0.f : CONTROLS_W;
    bool showKnobs = (mode != VIEW_IO_ONLY);
    bool showSquare = (mode == VIEW_FULL);

    float newWidth = metarboViewWidths[mode] * RACK_GRID_WIDTH;
    box.size.x = newWidth;
    bgPanel->box.size.x = newWidth;

    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      inputPorts[i]->box.pos = Vec(INPUT_X + controlsOffset, Y_IN + Y_SPACING * i);
    }

    float outX = showSquare
                     ? (CONTROLS_W + INPUT_X + PORT_W + 20.f + 150.f)
                     : (OUTPUT_X + controlsOffset);
    for (int i = 0; i < METARBO_NUM_OUTPUTS; i++) {
      outputPorts[i]->box.pos = Vec(outX, Y_OUT + Y_SPACING * i);
    }

    for (int i = 0; i < METARBO_NUM_CONTROLS; i++) {
      controlKnobs[i]->box.pos = Vec(KNOB_X, Y_KNOB + Y_SPACING * i);
      controlKnobs[i]->visible = showKnobs;
    }

    squareDisplay->visible = showSquare;
    squareDisplay->box.pos =
        Vec(CONTROLS_W + INPUT_X + PORT_W + 10.f,
            (RACK_GRID_HEIGHT - 150.f) / 2.f);

    lastViewMode = mode;
  }

  void draw(const DrawArgs& args) override {
    ModuleWidget::draw(args);
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::system("res/fonts/ShareTechMono-Regular.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, nvgRGB(0x22, 0x22, 0x22));

    nvgFontSize(args.vg, 13.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, box.size.x / 2, 8.f, "METARBO", NULL);

    float controlsOffset = (lastViewMode == VIEW_IO_ONLY) ? 0.f : CONTROLS_W;
    float inputX = INPUT_X + controlsOffset;
    float outX = (lastViewMode == VIEW_FULL)
                     ? (CONTROLS_W + INPUT_X + PORT_W + 20.f + 150.f)
                     : (OUTPUT_X + controlsOffset);

    nvgFontSize(args.vg, 11.f);
    static const char* inputLabels[] = {"A", "B", "C", "D", "E", "F", "G", "H"};
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
    for (int i = 0; i < METARBO_NUM_INPUTS; i++) {
      nvgText(args.vg, inputX, Y_IN + Y_SPACING * i + 4.f, inputLabels[i], NULL);
    }
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
    for (int i = 0; i < METARBO_NUM_OUTPUTS; i++) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", i + 1);
      nvgText(args.vg, outX + PORT_W + 4.f, Y_OUT + Y_SPACING * i + 9.f, buf,
              NULL);
    }
  }

  void step() override {
    if (metarboModule) {
      MetarboViewMode currentMode = metarboModule->viewMode;
      if (currentMode != lastViewMode) {
        updateLayout(currentMode);
      }
    }
    ModuleWidget::step();
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareMetarbo* mod =
        dynamic_cast<ComputerscareMetarbo*>(this->module);
    if (!mod) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("View Mode"));

    for (int i = 0; i < NUM_VIEW_MODES; i++) {
      struct ViewModeItem : MenuItem {
        ComputerscareMetarbo* module;
        int mode;
        void onAction(const event::Action& e) override {
          module->viewMode = (MetarboViewMode)mode;
          module->params[ComputerscareMetarbo::VIEW_MODE_PARAM].setValue(
              (float)mode);
        }
        void step() override {
          rightText = CHECKMARK((int)module->viewMode == mode);
          MenuItem::step();
        }
      };

      ViewModeItem* item = new ViewModeItem();
      item->text = metarboViewModeNames[i];
      item->module = mod;
      item->mode = i;
      menu->addChild(item);
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Program"));

    for (int i = 0; i < 4; i++) {
      struct ProgramItem : MenuItem {
        ComputerscareMetarbo* module;
        int program;
        void onAction(const event::Action& e) override {
          module->currentProgram = program;
        }
        void step() override {
          rightText = CHECKMARK(module->currentProgram == program);
          MenuItem::step();
        }
      };

      ProgramItem* item = new ProgramItem();
      item->text = "Program " + std::to_string(i + 1) + " (pass-through)";
      item->module = mod;
      item->program = i;
      menu->addChild(item);
    }
  }
};

Model* modelComputerscareMetarbo =
    createModel<ComputerscareMetarbo, ComputerscareMetarboWidget>(
        "computerscare-metarbo");
