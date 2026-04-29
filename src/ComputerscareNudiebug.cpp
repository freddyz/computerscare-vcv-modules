#include <algorithm>

#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
#include "Nudiebug/NudiebugProcessing.hpp"
#include "Nudiebug/NudiebugTextDisplay.hpp"
#include "complex/ComplexWidgets.hpp"

struct ComputerscareNudiebug : ComputerscareComplexBase {
  enum ParamIds {
    Z_INPUT_MODE,
    Z_OUTPUT_MODE,
    TEXT_DISPLAY_MODE,
    BARS_DISPLAY_MODE,
    PLOT_DISPLAY_MODE,
    NUM_PARAMS
  };
  enum InputIds {
    Z_INPUT,
    NUM_INPUTS = Z_INPUT + 2
  };
  enum OutputIds {
    Z_OUTPUT,
    NUM_OUTPUTS = Z_OUTPUT + 2
  };
  enum LightIds { NUM_LIGHTS };

  float width = 12 * RACK_GRID_WIDTH;
  bool loadedJson = false;
  nudiebug::Snapshot snapshot;
  nudiebug::DisplayOptions displayOptions;

  ComputerscareNudiebug() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam<cpx::CompolyModeParam>(Z_INPUT_MODE, 0.f, 3.f, 0.f,
                                       "z Input Mode");
    configParam<cpx::CompolyModeParam>(Z_OUTPUT_MODE, 0.f, 3.f, 0.f,
                                       "z Output Mode");
    configSwitch(TEXT_DISPLAY_MODE, 0.f, 3.f, nudiebug::TEXT_POLY, "Text",
                 {"Off", "Poly", "Compoly Rectangular", "Compoly Polar"});
    configSwitch(BARS_DISPLAY_MODE, 0.f, 2.f, nudiebug::BARS_UNIPOLAR, "Bars",
                 {"Off", "Unipolar", "Bipolar"});
    configSwitch(PLOT_DISPLAY_MODE, 0.f, 0.f, nudiebug::PLOT_OFF, "Plot",
                 {"Off"});
    configInput<cpx::CompolyPortInfo<Z_INPUT_MODE, 0>>(Z_INPUT, "z");
    configInput<cpx::CompolyPortInfo<Z_INPUT_MODE, 1>>(Z_INPUT + 1, "z");
    configOutput<cpx::CompolyPortInfo<Z_OUTPUT_MODE, 0>>(Z_OUTPUT, "z");
    configOutput<cpx::CompolyPortInfo<Z_OUTPUT_MODE, 1>>(Z_OUTPUT + 1, "z");
  }

  void process(const ProcessArgs& args) override {
    displayOptions.textMode = params[TEXT_DISPLAY_MODE].getValue();
    displayOptions.textEnabled = displayOptions.textMode != nudiebug::TEXT_OFF;
    displayOptions.visualizationMode = params[BARS_DISPLAY_MODE].getValue();
    displayOptions.visualizationEnabled =
        displayOptions.visualizationMode != nudiebug::BARS_OFF;
    displayOptions.plotMode = params[PLOT_DISPLAY_MODE].getValue();
    displayOptions.plotEnabled = displayOptions.plotMode != nudiebug::PLOT_OFF;
    nudiebug::processDebugger(this);
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "width", json_real(width));
    json_object_set_new(rootJ, "textEnabled",
                        json_boolean(displayOptions.textEnabled));
    json_object_set_new(rootJ, "textMode",
                        json_integer(displayOptions.textMode));
    json_object_set_new(rootJ, "visualizationEnabled",
                        json_boolean(displayOptions.visualizationEnabled));
    json_object_set_new(rootJ, "visualizationMode",
                        json_integer(displayOptions.visualizationMode));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) {
      width =
          std::max(static_cast<float>(json_number_value(widthJ)),
                   12.f * RACK_GRID_WIDTH);
    }

    json_t* textEnabledJ = json_object_get(rootJ, "textEnabled");
    if (textEnabledJ) displayOptions.textEnabled = json_is_true(textEnabledJ);

    json_t* textModeJ = json_object_get(rootJ, "textMode");
    if (textModeJ) displayOptions.textMode = json_integer_value(textModeJ);

    json_t* visualizationEnabledJ =
        json_object_get(rootJ, "visualizationEnabled");
    if (visualizationEnabledJ) {
      displayOptions.visualizationEnabled = json_is_true(visualizationEnabledJ);
    }

    json_t* visualizationModeJ = json_object_get(rootJ, "visualizationMode");
    if (visualizationModeJ) {
      displayOptions.visualizationMode = json_integer_value(visualizationModeJ);
    }
  }
};

struct NudiebugModeSwitch : app::Switch {
  std::string label;

  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 2.f);
    nvgFillColor(args.vg, nvgRGBA(0x12, 0x14, 0x16, 0x90));
    nvgFill(args.vg);
    nvgStrokeColor(args.vg, nvgRGBA(0xC8, 0xEA, 0xE2, 0xA0));
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);

    nvgFontSize(args.vg, 8.5f);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0x12, 0x14, 0x16));
    nvgText(args.vg, box.size.x * 0.5f + 0.6f, box.size.y * 0.55f + 0.6f,
            label.c_str(), nullptr);
    nvgFillColor(args.vg, nvgRGB(0xF3, 0xF1, 0xDE));
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.55f, label.c_str(),
            nullptr);
  }
};

struct NudiebugLabel : TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x10));
    nvgFontSize(args.vg, 13.f);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, "Nudiebug",
            nullptr);
  }
};

struct ComputerscareNudiebugWidget : ModuleWidget {
  BGPanel* bgPanel = nullptr;
  nudiebug::TextDisplay* textDisplay = nullptr;
  cpx::CompolyPortsWidget* zInput = nullptr;
  cpx::CompolyPortsWidget* zOutput = nullptr;
  NudiebugLabel* label = nullptr;
  ComputerscareResizeHandle* rightHandle = nullptr;
  ComputerscareNudiebug* nudiebugModule = nullptr;

  ComputerscareNudiebugWidget(ComputerscareNudiebug* module) {
    setModule(module);
    nudiebugModule = module;
    box.size =
        Vec(module ? module->width : 12 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0xE0, 0xE0, 0xD9));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    textDisplay = new nudiebug::TextDisplay();
    textDisplay->box.pos = Vec(0.f, 48.f);
    textDisplay->box.size = Vec(box.size.x, 282.f);
    if (module) {
      textDisplay->snapshot = &module->snapshot;
      textDisplay->options = &module->displayOptions;
    }
    addChild(textDisplay);

    addModeButton("Txt", Vec(5.f, 7.f), module,
                  ComputerscareNudiebug::TEXT_DISPLAY_MODE);
    addModeButton("Bars", Vec(31.f, 7.f), module,
                  ComputerscareNudiebug::BARS_DISPLAY_MODE);
    addModeButton("Plot", Vec(63.f, 7.f), module,
                  ComputerscareNudiebug::PLOT_DISPLAY_MODE);

    zOutput = new cpx::CompolyPortsWidget(
        Vec(box.size.x - 68.f, 16.f), module, ComputerscareNudiebug::Z_OUTPUT,
        ComputerscareNudiebug::Z_OUTPUT_MODE, 0.7f, true, "z");
    zOutput->compolyLabelTransform->box.pos = Vec(box.size.x - 90.f, 15.f);
    addChild(zOutput);

    zInput = new cpx::CompolyPortsWidget(
        Vec(24.f, 346.f), module, ComputerscareNudiebug::Z_INPUT,
        ComputerscareNudiebug::Z_INPUT_MODE, 0.7f, false, "z");
    zInput->compolyLabelTransform->box.pos = Vec(6.f, 350.f);
    addChild(zInput);

    label = new NudiebugLabel();
    label->box.pos = Vec(0.f, RACK_GRID_HEIGHT - 17.f);
    label->box.size = Vec(box.size.x, 14.f);
    addChild(label);

    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->minWidth = 12 * RACK_GRID_WIDTH;
    rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    addChild(rightHandle);
  }

  void addModeButton(std::string label, Vec pos, ComputerscareNudiebug* module,
                     int paramId) {
    NudiebugModeSwitch* button =
        createParam<NudiebugModeSwitch>(pos, module, paramId);
    button->box.size = Vec(label == "Bars" ? 29.f : 24.f, 12.f);
    button->label = label;
    addParam(button);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareNudiebug* m = dynamic_cast<ComputerscareNudiebug*>(module);
    if (!m) return;

    menu->addChild(new MenuSeparator);
    menu->addChild(createSubmenuItem("Text", "", [=](Menu* submenu) {
      addParamMenuItem(submenu, "Off", nudiebug::TEXT_OFF,
                       ComputerscareNudiebug::TEXT_DISPLAY_MODE);
      addParamMenuItem(submenu, "Poly", nudiebug::TEXT_POLY,
                       ComputerscareNudiebug::TEXT_DISPLAY_MODE);
      addParamMenuItem(submenu, "Compoly Rectangular",
                       nudiebug::TEXT_COMPOLY_RECT,
                       ComputerscareNudiebug::TEXT_DISPLAY_MODE);
      addParamMenuItem(submenu, "Compoly Polar", nudiebug::TEXT_COMPOLY_POLAR,
                       ComputerscareNudiebug::TEXT_DISPLAY_MODE);
    }));
    menu->addChild(createSubmenuItem("Bars", "", [=](Menu* submenu) {
      addParamMenuItem(submenu, "Off", nudiebug::BARS_OFF,
                       ComputerscareNudiebug::BARS_DISPLAY_MODE);
      addParamMenuItem(submenu, "Unipolar", nudiebug::BARS_UNIPOLAR,
                       ComputerscareNudiebug::BARS_DISPLAY_MODE);
      addParamMenuItem(submenu, "Bipolar", nudiebug::BARS_BIPOLAR,
                       ComputerscareNudiebug::BARS_DISPLAY_MODE);
    }));
    menu->addChild(createSubmenuItem("Plot", "", [=](Menu* submenu) {
      addParamMenuItem(submenu, "Off", nudiebug::PLOT_OFF,
                       ComputerscareNudiebug::PLOT_DISPLAY_MODE);
    }));
  }

  void addParamMenuItem(Menu* menu, std::string text, int value, int paramId) {
    ComputerscareNudiebug* m = dynamic_cast<ComputerscareNudiebug*>(module);
    ParamSettingItem* item = new ParamSettingItem(value, &m->params[paramId]);
    item->text = text;
    menu->addChild(item);
  }

  void step() override {
    if (nudiebugModule && !nudiebugModule->loadedJson) {
      box.size.x = nudiebugModule->width;
      nudiebugModule->loadedJson = true;
    } else if (nudiebugModule && box.size.x != nudiebugModule->width) {
      nudiebugModule->width = box.size.x;
    }

    bgPanel->box.size = box.size;
    textDisplay->box.size.x = box.size.x;
    syncCompolyWidget(zOutput, box.size.x - 68.f, 16.f);
    zOutput->compolyLabelTransform->box.pos = Vec(box.size.x - 90.f, 15.f);
    syncCompolyWidget(zInput, 24.f, 346.f);
    zInput->compolyLabelTransform->box.pos = Vec(6.f, 350.f);
    label->box.size.x = box.size.x;
    rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;

    ModuleWidget::step();
  }

  void syncCompolyWidget(cpx::CompolyPortsWidget* widget, float x, float y) {
    widget->leftLabel->box.pos = Vec(x, y - 10.f);
    widget->rightLabel->box.pos = Vec(x + 50.f, y - 10.f);
    for (int i = 0; i < widget->numPorts; i++) {
      widget->ports[i]->box.pos = Vec(x + 30.f * i, y);
    }
  }
};

Model* modelComputerscareNudiebug =
    createModel<ComputerscareNudiebug, ComputerscareNudiebugWidget>(
        "computerscare-nudiebug");
