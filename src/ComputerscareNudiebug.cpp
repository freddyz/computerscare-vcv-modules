#include <algorithm>

#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
#include "Nudiebug/NudiebugProcessing.hpp"
#include "Nudiebug/NudiebugTextDisplay.hpp"
#include "complex/ComplexWidgets.hpp"

struct ComputerscareNudiebug : ComputerscareComplexBase {
  static float defaultWidth() { return 12.f * RACK_GRID_WIDTH; }
  static float minWidth() { return 8.f * RACK_GRID_WIDTH; }

  enum ParamIds {
    Z_INPUT_MODE,
    Z_OUTPUT_MODE,
    TEXT_DISPLAY_MODE,
    BARS_DISPLAY_MODE,
    PLOT_DISPLAY_MODE,
    CLEAR_PLOT_PER_FRAME,
    CHANNEL_LABELS_MODE,
    CHANNEL_LAYOUT_MODE,
    DISPLAY_ORIENTATION_MODE,
    NUM_PARAMS
  };
  enum InputIds { Z_INPUT, NUM_INPUTS = Z_INPUT + 2 };
  enum OutputIds { Z_OUTPUT, NUM_OUTPUTS = Z_OUTPUT + 2 };
  enum LightIds { NUM_LIGHTS };

  float width = defaultWidth();
  bool loadedJson = false;
  int displaySnapshotCounter = 513;
  int displaySnapshotPeriod = 512;
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
    configSwitch(PLOT_DISPLAY_MODE, 0.f, 1.f, nudiebug::PLOT_OFF, "Plot",
                 {"Off", "Dots"});
    configSwitch(CLEAR_PLOT_PER_FRAME, 0.f, 1.f, 1.f, "Clear plot per frame",
                 {"Persistent", "Clear"});
    configSwitch(CHANNEL_LABELS_MODE, 0.f, 1.f, 1.f, "Channel labels",
                 {"Off", "On"});
    configSwitch(CHANNEL_LAYOUT_MODE, 0.f, 1.f, nudiebug::CHANNEL_LAYOUT_ALL,
                 "Channel layout", {"All", "Stretch"});
    configSwitch(DISPLAY_ORIENTATION_MODE, 0.f, 1.f, nudiebug::DISPLAY_VERTICAL,
                 "Display orientation", {"Vertical", "Horizontal"});
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
    displayOptions.clearPlotPerFrame =
        params[CLEAR_PLOT_PER_FRAME].getValue() > 0.5f;
    displayOptions.channelLabelsEnabled =
        params[CHANNEL_LABELS_MODE].getValue() > 0.5f;
    displayOptions.channelLayoutMode = params[CHANNEL_LAYOUT_MODE].getValue();
    displayOptions.displayOrientation =
        params[DISPLAY_ORIENTATION_MODE].getValue();

    displaySnapshotCounter++;
    bool updateDisplay = displaySnapshotCounter > displaySnapshotPeriod;
    if (updateDisplay) {
      displaySnapshotCounter = 0;
    }

    nudiebug::processDebugger(this, updateDisplay);
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
    json_object_set_new(rootJ, "plotMode",
                        json_integer(displayOptions.plotMode));
    json_object_set_new(rootJ, "clearPlotPerFrame",
                        json_boolean(displayOptions.clearPlotPerFrame));
    json_object_set_new(rootJ, "channelLabelsEnabled",
                        json_boolean(displayOptions.channelLabelsEnabled));
    json_object_set_new(rootJ, "channelLayoutMode",
                        json_integer(displayOptions.channelLayoutMode));
    json_object_set_new(rootJ, "displayOrientation",
                        json_integer(displayOptions.displayOrientation));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) {
      width =
          std::max(static_cast<float>(json_number_value(widthJ)), minWidth());
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

    json_t* plotModeJ = json_object_get(rootJ, "plotMode");
    if (plotModeJ) displayOptions.plotMode = json_integer_value(plotModeJ);

    json_t* clearPlotPerFrameJ = json_object_get(rootJ, "clearPlotPerFrame");
    if (clearPlotPerFrameJ) {
      displayOptions.clearPlotPerFrame = json_is_true(clearPlotPerFrameJ);
    }

    json_t* channelLabelsEnabledJ =
        json_object_get(rootJ, "channelLabelsEnabled");
    if (channelLabelsEnabledJ) {
      displayOptions.channelLabelsEnabled = json_is_true(channelLabelsEnabledJ);
    }

    json_t* channelLayoutModeJ = json_object_get(rootJ, "channelLayoutMode");
    if (channelLayoutModeJ) {
      displayOptions.channelLayoutMode = json_integer_value(channelLayoutModeJ);
    }

    json_t* displayOrientationJ = json_object_get(rootJ, "displayOrientation");
    if (displayOrientationJ) {
      displayOptions.displayOrientation =
          json_integer_value(displayOrientationJ);
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
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, "Nudiebug", nullptr);
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
        Vec(module ? module->width : ComputerscareNudiebug::defaultWidth(),
            RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0xE0, 0xE0, 0xD9));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    textDisplay = new nudiebug::TextDisplay();
    textDisplay->box.pos = Vec(0.f, 64.f);
    textDisplay->box.size = Vec(box.size.x, 266.f);
    if (module) {
      textDisplay->snapshot = &module->snapshot;
      textDisplay->options = &module->displayOptions;
    }
    addChild(textDisplay);

    addModeButton("Txt", Vec(5.f, 7.f), module,
                  ComputerscareNudiebug::TEXT_DISPLAY_MODE);
    addModeButton("Bars", Vec(33.f, 7.f), module,
                  ComputerscareNudiebug::BARS_DISPLAY_MODE);
    addModeButton("Plot", Vec(5.f, 21.f), module,
                  ComputerscareNudiebug::PLOT_DISPLAY_MODE);
    addModeButton("Clr", Vec(33.f, 21.f), module,
                  ComputerscareNudiebug::CLEAR_PLOT_PER_FRAME);
    addModeButton("Lbl", Vec(5.f, 35.f), module,
                  ComputerscareNudiebug::CHANNEL_LABELS_MODE);
    addModeButton("All", Vec(33.f, 35.f), module,
                  ComputerscareNudiebug::CHANNEL_LAYOUT_MODE);
    addModeButton("Hor", Vec(5.f, 49.f), module,
                  ComputerscareNudiebug::DISPLAY_ORIENTATION_MODE);

    zOutput = new cpx::CompolyPortsWidget(
        Vec(box.size.x - 68.f, 16.f), module, ComputerscareNudiebug::Z_OUTPUT,
        ComputerscareNudiebug::Z_OUTPUT_MODE, 0.7f, true, "z");
    zOutput->compolyLabelTransform->box.pos = Vec(box.size.x - 82.f, 20.f);
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
    rightHandle->minWidth = ComputerscareNudiebug::minWidth();
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
      addParamMenuItem(submenu, "Dots", nudiebug::PLOT_DOTS,
                       ComputerscareNudiebug::PLOT_DISPLAY_MODE);
    }));
    menu->addChild(createSubmenuItem("Plot Clear", "", [=](Menu* submenu) {
      addParamMenuItem(submenu, "Persistent", 0,
                       ComputerscareNudiebug::CLEAR_PLOT_PER_FRAME);
      addParamMenuItem(submenu, "Clear per frame", 1,
                       ComputerscareNudiebug::CLEAR_PLOT_PER_FRAME);
    }));
    menu->addChild(createSubmenuItem("Channel Labels", "", [=](Menu* submenu) {
      addParamMenuItem(submenu, "Off", 0,
                       ComputerscareNudiebug::CHANNEL_LABELS_MODE);
      addParamMenuItem(submenu, "On", 1,
                       ComputerscareNudiebug::CHANNEL_LABELS_MODE);
    }));
    menu->addChild(createSubmenuItem("Channel Layout", "", [=](Menu* submenu) {
      addParamMenuItem(submenu, "All", nudiebug::CHANNEL_LAYOUT_ALL,
                       ComputerscareNudiebug::CHANNEL_LAYOUT_MODE);
      addParamMenuItem(submenu, "Stretch", nudiebug::CHANNEL_LAYOUT_STRETCH,
                       ComputerscareNudiebug::CHANNEL_LAYOUT_MODE);
    }));
    menu->addChild(
        createSubmenuItem("Display Orientation", "", [=](Menu* submenu) {
          addParamMenuItem(submenu, "Vertical", nudiebug::DISPLAY_VERTICAL,
                           ComputerscareNudiebug::DISPLAY_ORIENTATION_MODE);
          addParamMenuItem(submenu, "Horizontal", nudiebug::DISPLAY_HORIZONTAL,
                           ComputerscareNudiebug::DISPLAY_ORIENTATION_MODE);
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
    zOutput->compolyLabelTransform->box.pos = Vec(box.size.x - 82.f, 20.f);
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
