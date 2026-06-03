#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

struct ComputerscareMollysPorridge;

static const int MOLLYS_PORRIDGE_BLOCKS = 16;

static const char* MollysPorridgeModeNames[] = {
    "Add", "Insert", "Xfade", "VCA bipolar", "VCA unipolar",
};

static const char* MollysPorridgeModeCodes[] = {"ADD", "INS", "XFD", "VCB",
                                                "VCU"};

static const char* MollysPorridgeModeDescriptions[] = {
    "Sum the processed main input with the processed block input.",
    "Pass the main input unless the block input is patched, then replace it.",
    "Crossfade between main and block input using the attenuverter knob.",
    "Use main input as bipolar signal and processed block input as amplitude.",
    "Use main input as signal and processed block input floored at zero.",
};

struct ComputerscareMollysPorridge : ComputerscarePolyModule {
  enum Mode {
    MODE_ADD,
    MODE_INSERT,
    MODE_XFADE,
    MODE_VCA_BIPOLAR,
    MODE_VCA_UNIPOLAR,
    NUM_MODES
  };

  enum ParamIds {
    GLOBAL_ATTEN,
    GLOBAL_OFFSET,
    POLY_CHANNELS,
    BLOCK_ATTEN,
    BLOCK_OFFSET = BLOCK_ATTEN + MOLLYS_PORRIDGE_BLOCKS,
    NUM_PARAMS = BLOCK_OFFSET + MOLLYS_PORRIDGE_BLOCKS
  };
  enum InputIds {
    POLY_INPUT,
    BLOCK_INPUT,
    NUM_INPUTS = BLOCK_INPUT + MOLLYS_PORRIDGE_BLOCKS
  };
  enum OutputIds { POLY_OUTPUT, NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  int blockModes[MOLLYS_PORRIDGE_BLOCKS] = {};
  int numInputChannels = 0;

  ComputerscareMollysPorridge() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configParam(GLOBAL_ATTEN, -2.f, 2.f, 1.f, "Input Attenuverter");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Input Offset", " volts");
    configSwitch(POLY_CHANNELS, 0.f, 16.f, 0.f, "Poly Channels",
                 polyChannelLabels(true));

    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS)->resetEnabled = false;
    getParamQuantity(GLOBAL_ATTEN)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;

    configInput(POLY_INPUT, "Main");
    configOutput(POLY_OUTPUT, "Main");

    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      blockModes[i] = MODE_INSERT;
      configParam(BLOCK_ATTEN + i, -1.f, 1.f, 1.f,
                  "Block " + std::to_string(i + 1) + " Attenuverter");
      configParam(BLOCK_OFFSET + i, -10.f, 10.f, 0.f,
                  "Block " + std::to_string(i + 1) + " Offset", " volts");
      configInput(BLOCK_INPUT + i, "Block " + std::to_string(i + 1));
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_t* modesJ = json_array();
    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      json_array_append_new(modesJ, json_integer(blockModes[i]));
    }
    json_object_set_new(rootJ, "blockModes", modesJ);
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* modesJ = json_object_get(rootJ, "blockModes");
    if (!modesJ) {
      return;
    }

    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      json_t* modeJ = json_array_get(modesJ, i);
      if (!modeJ) {
        continue;
      }
      blockModes[i] =
          math::clamp((int)json_integer_value(modeJ), 0, (int)NUM_MODES - 1);
    }
  }

  void checkPoly() override {
    numInputChannels = inputs[POLY_INPUT].getChannels();
    int knobSetting = (int)params[POLY_CHANNELS].getValue();
    if (numInputChannels > 0) {
      polyChannels = knobSetting == 0 ? numInputChannels : knobSetting;
    } else {
      polyChannels = knobSetting == 0 ? 16 : knobSetting;
    }
    outputs[POLY_OUTPUT].setChannels(polyChannels);
  }

  float getMainVoltage(int channel) {
    float input =
        numInputChannels > 0
            ? inputs[POLY_INPUT].getPolyVoltage(channel % numInputChannels)
            : 0.f;
    return input * params[GLOBAL_ATTEN].getValue() +
           params[GLOBAL_OFFSET].getValue();
  }

  float getBlockPortVoltage(int blockIndex, int channel) {
    Input& input = inputs[BLOCK_INPUT + blockIndex];
    int channels = input.getChannels();
    if (channels <= 0) {
      return 0.f;
    }
    return input.getPolyVoltage(channels == 1 ? 0 : channel % channels);
  }

  float getInsertVoltage(int blockIndex, int channel) {
    return getBlockPortVoltage(blockIndex, channel) *
               params[BLOCK_ATTEN + blockIndex].getValue() +
           params[BLOCK_OFFSET + blockIndex].getValue();
  }

  bool isBlockPatched(int blockIndex) {
    return inputs[BLOCK_INPUT + blockIndex].isConnected();
  }

  float processBlock(int blockIndex, int channel) {
    float main = getMainVoltage(channel);
    float insert = getInsertVoltage(blockIndex, channel);
    int mode = math::clamp(blockModes[blockIndex], 0, (int)NUM_MODES - 1);

    switch (mode) {
      case MODE_ADD:
        return main + insert;
      case MODE_INSERT:
        return isBlockPatched(blockIndex) ? insert : main;
      case MODE_XFADE: {
        float fade = math::rescale(params[BLOCK_ATTEN + blockIndex].getValue(),
                                   -1.f, 1.f, 0.f, 1.f);
        float portWithOffset = getBlockPortVoltage(blockIndex, channel) +
                               params[BLOCK_OFFSET + blockIndex].getValue();
        return crossfade(main, portWithOffset, fade);
      }
      case MODE_VCA_BIPOLAR:
        return main * insert;
      case MODE_VCA_UNIPOLAR:
        return main * std::max(0.f, insert);
      default:
        return main;
    }
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();
    for (int i = 0; i < polyChannels; i++) {
      int blockIndex = i % MOLLYS_PORRIDGE_BLOCKS;
      outputs[POLY_OUTPUT].setVoltage(processBlock(blockIndex, i), i);
    }
  }
};

struct MollysPorridgePanel : Widget {
  std::string fontPath =
      asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf");

  void drawLabel(const DrawArgs& args, Vec pos, const char* text, float size,
                 NVGcolor color, int align = NVG_ALIGN_LEFT) {
    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
    if (!font) {
      return;
    }
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, size);
    nvgTextAlign(args.vg, align | NVG_ALIGN_BASELINE);
    nvgFillColor(args.vg, color);
    nvgText(args.vg, pos.x, pos.y, text, NULL);
  }

  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(0xf4, 0xf1, 0xe8));
    nvgFill(args.vg);

    drawLabel(args, Vec(8.f, 18.f), "mollys porridge", 14.f, BLACK);
  }
};

struct MollysPorridgeBlockBackground : Widget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
    nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x28));
    nvgFill(args.vg);
  }
};

struct MollysPorridgeBlockNumber : Widget {
  std::string value;
  std::string fontPath =
      asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf");

  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 16.f);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    nvgText(args.vg, box.size.x, box.size.y * 0.5f, value.c_str(), NULL);
  }
};

struct MollysPorridgeModeItem : MenuItem {
  ComputerscareMollysPorridge* module = nullptr;
  int blockIndex = 0;
  int mode = 0;
  bool setAll = false;

  void onAction(const event::Action& e) override {
    if (!module) {
      return;
    }

    if (setAll) {
      for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
        module->blockModes[i] = mode;
      }
    } else {
      module->blockModes[blockIndex] = mode;
    }
  }

  void draw(const DrawArgs& args) override {
    BNDwidgetState state = BND_DEFAULT;
    if (APP->event->hoveredWidget == this) {
      state = BND_HOVER;
    }

    const BNDtheme* theme = bndGetTheme();
    if (state != BND_DEFAULT) {
      bndInnerBox(args.vg, 0.0, 0.0, box.size.x, box.size.y, 0, 0, 0, 0,
                  bndOffsetColor(theme->menuItemTheme.innerSelectedColor,
                                 theme->menuItemTheme.shadeTop),
                  bndOffsetColor(theme->menuItemTheme.innerSelectedColor,
                                 theme->menuItemTheme.shadeDown));
      state = BND_ACTIVE;
    }

    NVGcolor nameColor = bndTextColor(&theme->menuItemTheme, state);
    NVGcolor descriptionColor = state == BND_DEFAULT
                                    ? theme->menuTheme.textColor
                                    : theme->menuTheme.textSelectedColor;
    const char* description = MollysPorridgeModeDescriptions[mode];

    bndIconLabelValue(args.vg, 0.f, 3.f, box.size.x, 18.f, -1, nameColor,
                      BND_LEFT, BND_LABEL_FONT_SIZE, text.c_str(), NULL);
    bndIconLabelValue(args.vg, 0.f, 21.f, box.size.x, 18.f, -1,
                      descriptionColor, BND_LEFT, BND_LABEL_FONT_SIZE,
                      description, NULL);

    if (!rightText.empty()) {
      float x = box.size.x - bndLabelWidth(args.vg, -1, rightText.c_str());
      bndIconLabelValue(args.vg, x, 0.f, box.size.x, box.size.y, -1,
                        descriptionColor, BND_LEFT, BND_LABEL_FONT_SIZE,
                        rightText.c_str(), NULL);
    }
  }

  void step() override {
    rightText =
        CHECKMARK(module && !setAll && module->blockModes[blockIndex] == mode);
    box.size.x = std::max(bndLabelWidth(APP->window->vg, -1, text.c_str()),
                          bndLabelWidth(APP->window->vg, -1,
                                        MollysPorridgeModeDescriptions[mode])) +
                 34.f;
    box.size.y = 42.f;
    Widget::step();
  }
};

static void addMollysPorridgeModeItems(Menu* menu,
                                       ComputerscareMollysPorridge* module,
                                       int blockIndex, bool setAll) {
  for (int mode = 0; mode < ComputerscareMollysPorridge::NUM_MODES; mode++) {
    MollysPorridgeModeItem* item = new MollysPorridgeModeItem();
    item->text = MollysPorridgeModeNames[mode];
    item->module = module;
    item->blockIndex = blockIndex;
    item->mode = mode;
    item->setAll = setAll;
    menu->addChild(item);
  }
}

struct MollysPorridgeModeButton : ComputerscareBlankButton {
  ComputerscareMollysPorridge* module = nullptr;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  ui::Tooltip* hoverTooltip = NULL;
  int blockIndex = 0;
  int menuFrame = -1;

  MollysPorridgeModeButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
    box.size.x *= 0.74f;
  }

  ~MollysPorridgeModeButton() { destroyHoverTooltip(); }

  void createHoverTooltip() {
    if (!settings::tooltips || hoverTooltip) {
      return;
    }

    hoverTooltip = new ui::Tooltip;
    APP->scene->addChild(hoverTooltip);
  }

  void updateHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    int mode = currentMode();
    hoverTooltip->text = std::string(MollysPorridgeModeNames[mode]) + "\n" +
                         MollysPorridgeModeDescriptions[mode];
  }

  void destroyHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    APP->scene->removeChild(hoverTooltip);
    delete hoverTooltip;
    hoverTooltip = NULL;
  }

  bool isMenuOpen() {
    ui::MenuOverlay* overlay = activeMenuOverlay.get();
    return overlay && !overlay->requestedDelete;
  }

  void updateMenuFrame() {
    int frame = isMenuOpen() ? 1 : 0;
    if (menuFrame == frame || frame >= (int)frames.size()) {
      return;
    }

    sw->setSvg(frames[frame]);
    fb->setDirty();
    menuFrame = frame;
    setIconPressed(frame == 1);
  }

  void step() override {
    ComputerscareBlankButton::step();
    updateMenuFrame();
    updateHoverTooltip();
  }

  int currentMode() const {
    if (!module) {
      return ComputerscareMollysPorridge::MODE_INSERT;
    }
    return math::clamp(module->blockModes[blockIndex], 0,
                       (int)ComputerscareMollysPorridge::NUM_MODES - 1);
  }

  void draw(const DrawArgs& args) override {
    updateMenuFrame();
    ComputerscareBlankButton::draw(args);
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    float textYOffset = isMenuOpen() ? 2.9f : 0.f;
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.48f + textYOffset,
            MollysPorridgeModeCodes[currentMode()], NULL);
  }

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && isMenuOpen()) {
      updateMenuFrame();
      return;
    }

    ComputerscareBlankButton::onDragEnd(e);
  }

  void onEnter(const event::Enter& e) override {
    createHoverTooltip();
    updateHoverTooltip();
    ComputerscareBlankButton::onEnter(e);
  }

  void onLeave(const event::Leave& e) override {
    destroyHoverTooltip();
    ComputerscareBlankButton::onLeave(e);
  }

  void onButton(const event::Button& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS) {
      ComputerscareBlankButton::onButton(e);
      return;
    }

    e.consume(this);
    destroyHoverTooltip();
    if (!module) {
      return;
    }

    Menu* menu = createMenu();
    activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
    menu->addChild(createMenuLabel("Block " + std::to_string(blockIndex + 1)));
    addMollysPorridgeModeItems(menu, module, blockIndex, false);
    updateMenuFrame();
  }
};

struct ComputerscareMollysPorridgeWidget : ModuleWidget {
  ComputerscareMollysPorridgeWidget(ComputerscareMollysPorridge* module) {
    setModule(module);
    box.size = Vec(12 * 15, 380);

    MollysPorridgePanel* panel = new MollysPorridgePanel();
    panel->box.size = box.size;
    addChild(panel);

    addChild(new PolyOutputChannelsWidget(
        Vec(118.f, 4.f), module, ComputerscareMollysPorridge::POLY_CHANNELS));
    addOutput(createOutput<InPort>(Vec(148.f, 5.f), module,
                                   ComputerscareMollysPorridge::POLY_OUTPUT));

    addMainInputRow(module, 8.f, 17.f);

    const float startY = 52.f;
    const float rowSpacing = 41.f;
    const float leftX = 4.f;
    const float rightX = 94.f;
    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      int row = i % 8;
      bool rightColumn = i >= 8;
      float x = rightColumn ? rightX : leftX;
      float y = startY + row * rowSpacing;
      addBlock(module, i, x, y);
    }
  }

  void addBlockBackground(float x, float y, float blockW, float blockH) {
    MollysPorridgeBlockBackground* background =
        createWidget<MollysPorridgeBlockBackground>(Vec(x, y));
    background->box.size = Vec(blockW, blockH);
    addChild(background);
  }

  void addMainInputRow(ComputerscareMollysPorridge* module, float x, float y) {
    addInput(createInput<PointingUpPentagonPort>(
        Vec(x, y), module, ComputerscareMollysPorridge::POLY_INPUT));
    addParam(createParam<SmallKnob>(Vec(x + 30.f, y + 3.f), module,
                                    ComputerscareMollysPorridge::GLOBAL_ATTEN));
    addParam(
        createParam<SmoothKnob>(Vec(x + 51.f, y - 2.f), module,
                                ComputerscareMollysPorridge::GLOBAL_OFFSET));
  }

  void addBlock(ComputerscareMollysPorridge* module, int blockIndex, float x,
                float y) {
    const float blockW = 76.f;
    const float blockH = 36.f;

    addBlockBackground(x, y, blockW, blockH);

    const float jackX = x + 2.f;
    const float jackY = y + 13.f;

    addInput(createInput<PointingUpPentagonPort>(
        Vec(jackX, jackY), module,
        ComputerscareMollysPorridge::BLOCK_INPUT + blockIndex));
    addParam(createParam<SmallKnob>(
        Vec(x + 32.f, y + 7.f), module,
        ComputerscareMollysPorridge::BLOCK_ATTEN + blockIndex));
    MollysPorridgeModeButton* button =
        createWidget<MollysPorridgeModeButton>(Vec(x + 27.f, y + 25.f));
    button->module = module;
    button->blockIndex = blockIndex;
    addChild(button);

    addParam(createParam<SmoothKnob>(
        Vec(x + 56.f, y + 14.f), module,
        ComputerscareMollysPorridge::BLOCK_OFFSET + blockIndex));

    MollysPorridgeBlockNumber* label =
        createWidget<MollysPorridgeBlockNumber>(Vec(jackX - 11.f, jackY - 5.f));
    label->box.size = Vec(16.f, 9.f);
    label->value = std::to_string(blockIndex + 1);
    addChild(label);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareMollysPorridge* molly =
        dynamic_cast<ComputerscareMollysPorridge*>(module);
    if (!molly) {
      return;
    }

    menu->addChild(new MenuSeparator);
    menu->addChild(createSubmenuItem("Set all to", "", [=](Menu* submenu) {
      addMollysPorridgeModeItems(submenu, molly, 0, true);
    }));
  }
};

Model* modelComputerscareMollysPorridge =
    createModel<ComputerscareMollysPorridge, ComputerscareMollysPorridgeWidget>(
        "computerscare-mollys-porridge");
