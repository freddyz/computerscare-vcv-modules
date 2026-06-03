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
    "Sum the processed main input with the processed channel input.",
    "Pass the main input unless the channel input is patched, then replace it.",
    "Crossfade between main and channel input using the attenuverter knob.",
    ("Use main input as bipolar signal and processed channel input as "
     "amplitude."),
    "Use main input as signal and processed channel input floored at zero.",
};

static const char* MollysPorridgeNormalizationNames[] = {
    "No normalization",
    "Normalize polyphonic",
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

  enum NormalizationMode {
    NORMALIZATION_NONE,
    NORMALIZATION_POLY,
    NUM_NORMALIZATION_MODES
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
  int normalizationMode = NORMALIZATION_NONE;
  int numInputChannels = 0;
  int insertSourceBlocks[MOLLYS_PORRIDGE_BLOCKS] = {};
  int insertSourceChannels[MOLLYS_PORRIDGE_BLOCKS] = {};
  bool insertActive[MOLLYS_PORRIDGE_BLOCKS] = {};

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
      insertSourceBlocks[i] = -1;
      insertSourceChannels[i] = -1;
      configParam(BLOCK_ATTEN + i, -1.f, 1.f, 1.f,
                  "Channel " + std::to_string(i + 1) + " Attenuverter");
      configParam(BLOCK_OFFSET + i, -10.f, 10.f, 0.f,
                  "Channel " + std::to_string(i + 1) + " Offset", " volts");
      configInput(BLOCK_INPUT + i, "Channel " + std::to_string(i + 1));
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_t* modesJ = json_array();
    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      json_array_append_new(modesJ, json_integer(blockModes[i]));
    }
    json_object_set_new(rootJ, "blockModes", modesJ);
    json_object_set_new(rootJ, "normalizationMode",
                        json_integer(normalizationMode));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* normalizationJ = json_object_get(rootJ, "normalizationMode");
    if (normalizationJ) {
      normalizationMode = math::clamp((int)json_integer_value(normalizationJ),
                                      0, (int)NUM_NORMALIZATION_MODES - 1);
    }

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
    updateInsertNormalizationCache();
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

  int getNormalizedSourceBlock(int blockIndex) {
    if (normalizationMode != NORMALIZATION_POLY ||
        inputs[BLOCK_INPUT + blockIndex].isConnected()) {
      return -1;
    }

    int columnStart = blockIndex < 8 ? 0 : 8;
    for (int sourceIndex = blockIndex - 1; sourceIndex >= columnStart;
         sourceIndex--) {
      Input& sourceInput = inputs[BLOCK_INPUT + sourceIndex];
      int sourceChannels = sourceInput.getChannels();
      if (sourceChannels <= 0) {
        continue;
      }

      if (blockIndex - sourceIndex < sourceChannels) {
        return sourceIndex;
      }
      return -1;
    }

    if (blockIndex >= 8) {
      for (int sourceIndex = 7; sourceIndex >= 0; sourceIndex--) {
        Input& sourceInput = inputs[BLOCK_INPUT + sourceIndex];
        int sourceChannels = sourceInput.getChannels();
        if (sourceChannels <= 0) {
          continue;
        }

        if (blockIndex - sourceIndex < sourceChannels) {
          return sourceIndex;
        }
      }
    }

    return -1;
  }

  int getNormalizationSegmentEnd(int sourceIndex, int segmentStartIndex) {
    if (normalizationMode != NORMALIZATION_POLY || sourceIndex < 0 ||
        sourceIndex >= MOLLYS_PORRIDGE_BLOCKS || segmentStartIndex < 0 ||
        segmentStartIndex >= MOLLYS_PORRIDGE_BLOCKS) {
      return segmentStartIndex;
    }

    Input& sourceInput = inputs[BLOCK_INPUT + sourceIndex];
    int sourceChannels = sourceInput.getChannels();
    if (sourceChannels <= 0) {
      return segmentStartIndex;
    }

    if (segmentStartIndex != sourceIndex &&
        inputs[BLOCK_INPUT + segmentStartIndex].isConnected()) {
      return segmentStartIndex;
    }

    int columnEnd = segmentStartIndex < 8 ? 8 : 16;
    int endIndex = std::min(columnEnd, sourceIndex + sourceChannels);
    int firstBlockToCheck =
        segmentStartIndex == sourceIndex ? sourceIndex + 1 : segmentStartIndex;
    for (int blockIndex = firstBlockToCheck; blockIndex < endIndex;
         blockIndex++) {
      if (inputs[BLOCK_INPUT + blockIndex].isConnected()) {
        return blockIndex;
      }
    }

    return endIndex;
  }

  bool isBlockNormalized(int blockIndex) {
    return getNormalizedSourceBlock(blockIndex) >= 0;
  }

  void updateInsertNormalizationCache() {
    for (int blockIndex = 0; blockIndex < MOLLYS_PORRIDGE_BLOCKS;
         blockIndex++) {
      insertSourceBlocks[blockIndex] = -1;
      insertSourceChannels[blockIndex] = -1;
      insertActive[blockIndex] = false;

      Input& input = inputs[BLOCK_INPUT + blockIndex];
      if (input.isConnected()) {
        insertSourceBlocks[blockIndex] = blockIndex;
        insertActive[blockIndex] = true;
        continue;
      }

      int sourceIndex = getNormalizedSourceBlock(blockIndex);
      if (sourceIndex >= 0) {
        insertSourceBlocks[blockIndex] = sourceIndex;
        insertSourceChannels[blockIndex] = blockIndex - sourceIndex;
        insertActive[blockIndex] = true;
      }
    }
  }

  float getEffectiveBlockPortVoltage(int blockIndex, int channel) {
    int sourceIndex = insertSourceBlocks[blockIndex];
    if (sourceIndex < 0) {
      return 0.f;
    }

    Input& input = inputs[BLOCK_INPUT + sourceIndex];
    int channels = input.getChannels();
    if (channels <= 0) {
      return 0.f;
    }

    int sourceChannel = insertSourceChannels[blockIndex];
    if (sourceChannel >= 0) {
      return sourceChannel < channels ? input.getPolyVoltage(sourceChannel)
                                      : 0.f;
    }

    return input.getPolyVoltage(channels == 1 ? 0 : channel % channels);
  }

  float getInsertVoltage(int blockIndex, int channel) {
    return getEffectiveBlockPortVoltage(blockIndex, channel) *
               params[BLOCK_ATTEN + blockIndex].getValue() +
           params[BLOCK_OFFSET + blockIndex].getValue();
  }

  bool isInsertActive(int blockIndex) { return insertActive[blockIndex]; }

  float processBlock(int blockIndex, int channel) {
    float main = getMainVoltage(channel);
    int mode = math::clamp(blockModes[blockIndex], 0, (int)NUM_MODES - 1);

    switch (mode) {
      case MODE_ADD:
        return main + getInsertVoltage(blockIndex, channel);
      case MODE_INSERT:
        if (isInsertActive(blockIndex)) {
          return getEffectiveBlockPortVoltage(blockIndex, channel) *
                     params[BLOCK_ATTEN + blockIndex].getValue() +
                 params[GLOBAL_OFFSET].getValue() +
                 params[BLOCK_OFFSET + blockIndex].getValue();
        }
        return main + params[BLOCK_OFFSET + blockIndex].getValue();
      case MODE_XFADE: {
        float fade = math::rescale(params[BLOCK_ATTEN + blockIndex].getValue(),
                                   -1.f, 1.f, 0.f, 1.f);
        float portWithOffset =
            getEffectiveBlockPortVoltage(blockIndex, channel) +
            params[BLOCK_OFFSET + blockIndex].getValue();
        return crossfade(main, portWithOffset, fade);
      }
      case MODE_VCA_BIPOLAR:
        return main * getInsertVoltage(blockIndex, channel);
      case MODE_VCA_UNIPOLAR:
        return main * std::max(0.f, getInsertVoltage(blockIndex, channel));
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

struct MollysPorridgeNormalizationOverlay : Widget {
  ComputerscareMollysPorridge* module = nullptr;

  static constexpr float startY = 52.f;
  static constexpr float rowSpacing = 41.f;
  static constexpr float leftX = 4.f;
  static constexpr float rightX = 94.f;
  static constexpr float blockW = 76.f;
  static constexpr float blockH = 36.f;

  float runNoise(int sourceIndex, int salt) {
    uint32_t seed = (sourceIndex + 1) * 73856093u ^ (salt + 1) * 83492791u;
    seed ^= seed >> 13;
    seed *= 1274126177u;
    seed ^= seed >> 16;
    return (seed & 0xffff) / 32767.5f - 1.f;
  }

  void drawRun(const DrawArgs& args, int sourceIndex, int segmentStartIndex,
               int runEndIndex, int runIndex) {
    int row = segmentStartIndex % 8;
    int lastRow = (runEndIndex - 1) % 8;
    bool rightColumn = segmentStartIndex >= 8;
    float x = (rightColumn ? rightX : leftX) - 12.f;
    float y = startY + row * rowSpacing + 5.f;
    float bottom = startY + lastRow * rowSpacing + 40.f;
    float height = bottom - y;
    float width = 40.f;
    float wiggle = 4.0f;

    Vec points[12] = {
        Vec(x + 2.f + runNoise(segmentStartIndex, 0) * wiggle,
            y + 1.f + runNoise(segmentStartIndex, 1) * wiggle),
        Vec(x + width * 0.34f + runNoise(segmentStartIndex, 2) * wiggle,
            y - 1.f + runNoise(segmentStartIndex, 3) * wiggle),
        Vec(x + width - 3.f + runNoise(segmentStartIndex, 4) * wiggle,
            y + 3.f + runNoise(segmentStartIndex, 5) * wiggle),
        Vec(x + width + runNoise(segmentStartIndex, 6) * wiggle,
            y + height * 0.20f + runNoise(segmentStartIndex, 7) * wiggle),
        Vec(x + width - 2.f + runNoise(segmentStartIndex, 8) * wiggle,
            y + height * 0.42f + runNoise(segmentStartIndex, 9) * wiggle),
        Vec(x + width - 5.f + runNoise(segmentStartIndex, 10) * wiggle,
            y + height * 0.72f + runNoise(segmentStartIndex, 11) * wiggle),
        Vec(x + width - 1.f + runNoise(segmentStartIndex, 12) * wiggle,
            y + height + runNoise(segmentStartIndex, 13) * wiggle),
        Vec(x + width * 0.58f + runNoise(segmentStartIndex, 14) * wiggle,
            y + height + 1.f + runNoise(segmentStartIndex, 15) * wiggle),
        Vec(x + 2.f + runNoise(segmentStartIndex, 16) * wiggle,
            y + height + runNoise(segmentStartIndex, 17) * wiggle),
        Vec(x - 2.f + runNoise(segmentStartIndex, 18) * wiggle,
            y + height * 0.70f + runNoise(segmentStartIndex, 19) * wiggle),
        Vec(x + runNoise(segmentStartIndex, 20) * wiggle,
            y + height * 0.45f + runNoise(segmentStartIndex, 21) * wiggle),
        Vec(x - 1.f + runNoise(segmentStartIndex, 22) * wiggle,
            y + height * 0.18f + runNoise(segmentStartIndex, 23) * wiggle),
    };

    bool dark = runIndex % 2 == 1;
    int baseRed = 0x24;
    int baseGreen = dark ? 0x86 : 0xc9;
    int baseBlue = dark ? 0x73 : 0xa6;
    float randomDarkness = (runNoise(segmentStartIndex, 24) + 1.f) * 12.f;
    int red = clamp((int)(baseRed - randomDarkness), 0, 255);
    int green = clamp((int)(baseGreen - randomDarkness), 0, 255);
    int blue = clamp((int)(baseBlue - randomDarkness), 0, 255);
    NVGcolor fill = nvgRGBA(red, green, blue, 0x90);
    NVGcolor stroke =
        nvgRGBA(clamp(red - 18, 0, 255), clamp(green - 18, 0, 255),
                clamp(blue - 18, 0, 255), 0xd0);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, points[0].x, points[0].y);
    for (int i = 1; i < 12; i++) {
      nvgLineTo(args.vg, points[i].x, points[i].y);
    }
    nvgClosePath(args.vg);
    nvgFillColor(args.vg, fill);
    nvgFill(args.vg);
    nvgStrokeWidth(args.vg, 1.2f);
    nvgStrokeColor(args.vg, stroke);
    nvgStroke(args.vg);
  }

  void draw(const DrawArgs& args) override {
    if (!module || module->normalizationMode !=
                       ComputerscareMollysPorridge::NORMALIZATION_POLY) {
      return;
    }

    int runIndex = 0;
    for (int sourceIndex = 0; sourceIndex < MOLLYS_PORRIDGE_BLOCKS;
         sourceIndex++) {
      if (!module
               ->inputs[ComputerscareMollysPorridge::BLOCK_INPUT + sourceIndex]
               .isConnected()) {
        continue;
      }

      int runEndIndex =
          module->getNormalizationSegmentEnd(sourceIndex, sourceIndex);
      if (runEndIndex <= sourceIndex) {
        continue;
      }

      int currentRunIndex = runIndex;
      drawRun(args, sourceIndex, sourceIndex, runEndIndex, currentRunIndex);

      if (sourceIndex < 8) {
        int sourceChannels =
            module
                ->inputs[ComputerscareMollysPorridge::BLOCK_INPUT + sourceIndex]
                .getChannels();
        if (sourceIndex + sourceChannels > 8) {
          int rightRunEndIndex =
              module->getNormalizationSegmentEnd(sourceIndex, 8);
          if (rightRunEndIndex > 8) {
            drawRun(args, sourceIndex, 8, rightRunEndIndex, currentRunIndex);
          }
        }
      }

      runIndex++;
    }
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

struct MollysPorridgeSimpleModeItem : MenuItem {
  ComputerscareMollysPorridge* module = nullptr;
  int blockIndex = 0;
  int mode = 0;

  void onAction(const event::Action& e) override {
    if (module) {
      module->blockModes[blockIndex] = mode;
    }
  }

  void step() override {
    rightText = CHECKMARK(module && module->blockModes[blockIndex] == mode);
    MenuItem::step();
  }
};

struct MollysPorridgeNormalizationItem : MenuItem {
  ComputerscareMollysPorridge* module = nullptr;
  int normalizationMode = 0;

  void onAction(const event::Action& e) override {
    if (module) {
      module->normalizationMode = normalizationMode;
    }
  }

  void step() override {
    rightText =
        CHECKMARK(module && module->normalizationMode == normalizationMode);
    MenuItem::step();
  }
};

static void addMollysPorridgeNormalizationItems(
    Menu* menu, ComputerscareMollysPorridge* module) {
  for (int mode = 0;
       mode < ComputerscareMollysPorridge::NUM_NORMALIZATION_MODES; mode++) {
    MollysPorridgeNormalizationItem* item =
        new MollysPorridgeNormalizationItem();
    item->text = MollysPorridgeNormalizationNames[mode];
    item->module = module;
    item->normalizationMode = mode;
    menu->addChild(item);
  }
}

static void addMollysPorridgeModeItems(Menu* menu,
                                       ComputerscareMollysPorridge* module,
                                       int blockIndex, bool setAll,
                                       bool showDescriptions) {
  for (int mode = 0; mode < ComputerscareMollysPorridge::NUM_MODES; mode++) {
    if (showDescriptions) {
      MollysPorridgeModeItem* modeItem = new MollysPorridgeModeItem();
      modeItem->text = MollysPorridgeModeNames[mode];
      modeItem->module = module;
      modeItem->blockIndex = blockIndex;
      modeItem->mode = mode;
      modeItem->setAll = setAll;
      menu->addChild(modeItem);
    } else {
      MollysPorridgeSimpleModeItem* simpleItem =
          new MollysPorridgeSimpleModeItem();
      simpleItem->text = MollysPorridgeModeNames[mode];
      simpleItem->module = module;
      simpleItem->blockIndex = blockIndex;
      simpleItem->mode = mode;
      menu->addChild(simpleItem);
    }
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
    menu->addChild(
        createMenuLabel("Channel " + std::to_string(blockIndex + 1) + " mode"));
    addMollysPorridgeModeItems(menu, module, blockIndex, false, false);
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
      addBlockBackground(x, y, 76.f, 36.f);
    }

    MollysPorridgeNormalizationOverlay* normalizationOverlay =
        createWidget<MollysPorridgeNormalizationOverlay>(Vec(0.f, 0.f));
    normalizationOverlay->box.size = box.size;
    normalizationOverlay->module = module;
    addChild(normalizationOverlay);

    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      int row = i % 8;
      bool rightColumn = i >= 8;
      float x = rightColumn ? rightX : leftX;
      float y = startY + row * rowSpacing;
      addBlockControls(module, i, x, y);
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

  void addBlockControls(ComputerscareMollysPorridge* module, int blockIndex,
                        float x, float y) {
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
      addMollysPorridgeModeItems(submenu, molly, 0, true, true);
    }));
    menu->addChild(
        createSubmenuItem("Insert normalization", "", [=](Menu* submenu) {
          addMollysPorridgeNormalizationItems(submenu, molly);
        }));
  }
};

Model* modelComputerscareMollysPorridge =
    createModel<ComputerscareMollysPorridge, ComputerscareMollysPorridgeWidget>(
        "computerscare-mollys-porridge");
