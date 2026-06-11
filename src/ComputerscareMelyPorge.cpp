#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"
#include "complex/PolyphonicMapping.hpp"

struct ComputerscareMelyPorge;

static const int MOLLYS_PORRIDGE_BLOCKS = 16;

static const char* MelyPorgeModeNames[] = {
    "Add",          "Insert", "Crossfade", "VCA bipolar",
    "VCA unipolar", "Min",    "Max",       "Compare",
};

static const char* MelyPorgeModeCodes[] = {"ADD", "INS", "XFD", "VCAB",
                                           "VCA", "MIN", "MAX", "COMP"};

static const char* MelyPorgeModeDescriptions[] = {
    "Sum the main input with the channel input.",
    "Pass the main input unless the channel input is patched, then replace it.",
    "Crossfade between main and channel input using the attenuverter knob.",
    "Multiplication: Main input is the signal, channel input is bipolar gain",
    "Multiplication: Main input is the signal, channel input is unipolar gain",
    "Output the lesser of the main input and the channel input.",
    "Output the greater of the main input and the channel input.",
    "Output 10V when the main input exceeds the channel input, otherwise 0V.",
};

static const char* MelyPorgeNormalizationNames[] = {
    "No normalization",
    "Normalize polyphonic",
};

static const char* MelyPorgeMainInputMappingNames[] = {
    "Standard",
    "Cycle",
    "Zero pad",
    "Stall",
};

static const char* MelyPorgeMainInputMappingDescriptions[] = {
    "Mono main input is copied to all output channels; poly input uses "
    "matching channels.",
    "Main input channels repeat until the output channel count is filled.",
    "Channels past the main input channel count are padded with 0V.",
    "Channels past the main input channel count use the final main input "
    "channel.",
};

struct ComputerscareMelyPorge : ComputerscarePolyModule {
  enum Mode {
    MODE_ADD,
    MODE_INSERT,
    MODE_XFADE,
    MODE_VCA_BIPOLAR,
    MODE_VCA_UNIPOLAR,
    MODE_MIN,
    MODE_MAX,
    MODE_COMPARE,
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
  int normalizationMode = NORMALIZATION_POLY;
  int mainInputMappingMode = cpx::polyphonic::defaultMappingModeValue;
  bool multipleInsertsBreakNormalization = false;
  int numInputChannels = 0;
  int insertSourceBlocks[MOLLYS_PORRIDGE_BLOCKS] = {};
  int insertSourceChannels[MOLLYS_PORRIDGE_BLOCKS] = {};
  bool insertActive[MOLLYS_PORRIDGE_BLOCKS] = {};

  struct BlockAttenParamQuantity : ParamQuantity {
    std::string getLabel() override;
  };

  struct BlockOffsetParamQuantity : ParamQuantity {
    std::string getLabel() override;
  };

  ComputerscareMelyPorge() {
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
      configParam<BlockAttenParamQuantity>(
          BLOCK_ATTEN + i, -1.f, 1.f, 1.f,
          "Channel " + std::to_string(i + 1) + " Attenuversion");
      configParam<BlockOffsetParamQuantity>(
          BLOCK_OFFSET + i, -10.f, 10.f, 0.f,
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
    json_object_set_new(rootJ, "mainInputMappingMode",
                        json_integer(mainInputMappingMode));
    json_object_set_new(rootJ, "multipleInsertsBreakNormalization",
                        json_boolean(multipleInsertsBreakNormalization));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* normalizationJ = json_object_get(rootJ, "normalizationMode");
    if (normalizationJ) {
      normalizationMode = math::clamp((int)json_integer_value(normalizationJ),
                                      0, (int)NUM_NORMALIZATION_MODES - 1);
    }

    json_t* mainInputMappingJ = json_object_get(rootJ, "mainInputMappingMode");
    if (mainInputMappingJ) {
      mainInputMappingMode =
          math::clamp((int)json_integer_value(mainInputMappingJ),
                      cpx::polyphonic::firstMappingModeValue,
                      cpx::polyphonic::lastMappingModeValue);
    }

    json_t* multipleInsertsBreakJ =
        json_object_get(rootJ, "multipleInsertsBreakNormalization");
    if (multipleInsertsBreakJ) {
      multipleInsertsBreakNormalization = json_is_true(multipleInsertsBreakJ);
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
    updateInsertNormalizationCache();
    polyChannels = knobSetting == 0 ? getAutoPolyChannels() : knobSetting;
    outputs[POLY_OUTPUT].setChannels(polyChannels);
  }

  int getAutoPolyChannels() {
    int channels = numInputChannels;
    for (int blockIndex = 0; blockIndex < MOLLYS_PORRIDGE_BLOCKS;
         blockIndex++) {
      if (insertActive[blockIndex]) {
        channels = std::max(channels, blockIndex + 1);
      }
    }

    if (channels <= 0) {
      return 16;
    }
    return math::clamp(channels, 1, 16);
  }

  float getMainVoltage(int channel) {
    float input = getMainInputVoltage(channel);
    return input * params[GLOBAL_ATTEN].getValue() +
           params[GLOBAL_OFFSET].getValue();
  }

  float getMainInputVoltage(int channel) {
    if (numInputChannels <= 0) {
      return 0.f;
    }

    int knobSetting = (int)params[POLY_CHANNELS].getValue();
    if (knobSetting == 0 && channel < numInputChannels) {
      return inputs[POLY_INPUT].getPolyVoltage(channel);
    }

    int inputChannel = cpx::polyphonic::inputChannelForOutputChannel(
        cpx::polyphonic::OutputChannel(channel),
        static_cast<cpx::polyphonic::MappingMode>(mainInputMappingMode),
        cpx::polyphonic::ChannelCount(numInputChannels));
    return inputChannel < numInputChannels
               ? inputs[POLY_INPUT].getPolyVoltage(inputChannel)
               : 0.f;
  }

  float getBlockPortVoltage(int blockIndex, int channel) {
    Input& input = inputs[BLOCK_INPUT + blockIndex];
    int channels = input.getChannels();
    if (channels <= 0) {
      return 0.f;
    }
    return channel < channels ? input.getPolyVoltage(channel) : 0.f;
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
      if (multipleInsertsBreakNormalization) {
        return -1;
      }
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
        if (multipleInsertsBreakNormalization) {
          return -1;
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
        insertSourceChannels[blockIndex] = 0;
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

    return channel < channels ? input.getPolyVoltage(channel) : 0.f;
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
          return getInsertVoltage(blockIndex, channel);
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
        return main * getInsertVoltage(blockIndex, channel) / 10.f;
      case MODE_VCA_UNIPOLAR:
        return main * std::max(0.f, getInsertVoltage(blockIndex, channel)) /
               10.f;
      case MODE_MIN:
        return std::min(main, getInsertVoltage(blockIndex, channel));
      case MODE_MAX:
        return std::max(main, getInsertVoltage(blockIndex, channel));
      case MODE_COMPARE:
        return main > getInsertVoltage(blockIndex, channel) ? 10.f : 0.f;
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

std::string ComputerscareMelyPorge::BlockAttenParamQuantity::getLabel() {
  auto* mely = dynamic_cast<ComputerscareMelyPorge*>(module);
  int blockIndex = paramId - BLOCK_ATTEN;
  if (!mely || blockIndex < 0 || blockIndex >= MOLLYS_PORRIDGE_BLOCKS) {
    return ParamQuantity::getLabel();
  }

  int mode = math::clamp(mely->blockModes[blockIndex], 0, (int)NUM_MODES - 1);
  std::string prefix = "Channel " + std::to_string(blockIndex + 1) + " ";
  if (mode == MODE_XFADE) {
    return prefix + "Crossfade Amount";
  }
  if (mode == MODE_COMPARE) {
    return prefix + "Threshold Attenuversion";
  }
  return prefix + "Attenuversion";
}

std::string ComputerscareMelyPorge::BlockOffsetParamQuantity::getLabel() {
  auto* mely = dynamic_cast<ComputerscareMelyPorge*>(module);
  int blockIndex = paramId - BLOCK_OFFSET;
  if (!mely || blockIndex < 0 || blockIndex >= MOLLYS_PORRIDGE_BLOCKS) {
    return ParamQuantity::getLabel();
  }

  int mode = math::clamp(mely->blockModes[blockIndex], 0, (int)NUM_MODES - 1);
  std::string prefix = "Channel " + std::to_string(blockIndex + 1) + " ";
  if (mode == MODE_VCA_BIPOLAR || mode == MODE_VCA_UNIPOLAR) {
    return prefix + "VCA Amplitude Offset";
  }
  if (mode == MODE_COMPARE) {
    return prefix + "Threshold Offset";
  }
  return prefix + "Offset";
}

struct MelyPorgeBlockNumber : Widget {
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

struct MelyPorgeDisableableSmallKnob : ComputerscareRoundKnob {
  ComputerscareMelyPorge* module = nullptr;
  int channel = 0;
  bool disabled = false;
  bool initialized = false;
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-small-knob-effed.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-small-knob-effed-disabled.svg"));

  MelyPorgeDisableableSmallKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  void draw(const DrawArgs& args) override {
    if (disabled) {
      nvgSave(args.vg);
      nvgGlobalAlpha(args.vg, 0.55f);
      ComputerscareRoundKnob::draw(args);
      nvgRestore(args.vg);
      return;
    }

    ComputerscareRoundKnob::draw(args);
  }

  void step() override {
    bool candidateDisabled =
        module ? channel > module->polyChannels - 1 : false;
    if (!initialized || disabled != candidateDisabled) {
      setSvg(candidateDisabled ? disabledSvg : enabledSvg);
      event::Change changeEvent;
      onChange(changeEvent);
      fb->dirty = true;
      disabled = candidateDisabled;
      initialized = true;
    }
    ComputerscareRoundKnob::step();
  }
};

struct MelyPorgeDisableableSmoothKnob : ComputerscareRoundKnob {
  ComputerscareMelyPorge* module = nullptr;
  int channel = 0;
  bool disabled = false;
  bool initialized = false;
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-knob-effed.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-knob-disabled.svg"));

  MelyPorgeDisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  void step() override {
    bool candidateDisabled =
        module ? channel > module->polyChannels - 1 : false;
    if (!initialized || disabled != candidateDisabled) {
      setSvg(candidateDisabled ? disabledSvg : enabledSvg);
      event::Change changeEvent;
      onChange(changeEvent);
      fb->dirty = true;
      disabled = candidateDisabled;
      initialized = true;
    }
    ComputerscareRoundKnob::step();
  }
};

struct MelyPorgeInsertPort : PointingUpPentagonPort {
  int blockIndex = 0;

  void step() override {
    ComputerscareMelyPorge* mely =
        dynamic_cast<ComputerscareMelyPorge*>(module);
    engine::PortInfo* portInfo = mely ? getPortInfo() : nullptr;
    if (portInfo) {
      int sourceIndex = mely->getNormalizedSourceBlock(blockIndex);
      if (sourceIndex >= 0) {
        int sourceChannel = blockIndex - sourceIndex;
        portInfo->description = "Normaled to Insert " +
                                std::to_string(sourceIndex + 1) + ", ch" +
                                std::to_string(sourceChannel + 1);
      } else {
        portInfo->description = "";
      }
    }

    PointingUpPentagonPort::step();
  }
};

struct MelyPorgeNormalizationOverlay : Widget {
  ComputerscareMelyPorge* module = nullptr;

  static constexpr float startY = 52.f;
  static constexpr float rowSpacing = 39.f;
  static constexpr float leftX = 4.f;
  static constexpr float rightX = 91.f;
  static constexpr float leftColumnYOffset = 11.f;
  static constexpr float rightColumnYOffset = 0.f;
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
               int runEndIndex) {
    int row = segmentStartIndex % 8;
    int lastRow = (runEndIndex - 1) % 8;
    bool rightColumn = segmentStartIndex >= 8;
    float x = (rightColumn ? rightX : leftX) - 12.f;
    float columnYOffset = rightColumn ? rightColumnYOffset : leftColumnYOffset;
    float y = startY + columnYOffset + row * rowSpacing + 5.f;
    float bottom = startY + columnYOffset + lastRow * rowSpacing + 40.f;
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

    bool dark = sourceIndex % 2 == 1;
    int baseRed = 0x24;
    int baseGreen = dark ? 0x86 : 0xc9;
    int baseBlue = dark ? 0x73 : 0xa6;
    float randomDarkness = (runNoise(sourceIndex, 24) + 1.f) * 12.f;
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
                       ComputerscareMelyPorge::NORMALIZATION_POLY) {
      return;
    }

    for (int columnStart = 0; columnStart < MOLLYS_PORRIDGE_BLOCKS;
         columnStart += 8) {
      int columnEnd = columnStart + 8;
      int blockIndex = columnStart;
      while (blockIndex < columnEnd) {
        int sourceIndex = sourceForBlock(blockIndex);
        if (sourceIndex < 0) {
          blockIndex++;
          continue;
        }

        int runStartIndex = blockIndex;
        blockIndex++;
        while (blockIndex < columnEnd &&
               sourceForBlock(blockIndex) == sourceIndex) {
          blockIndex++;
        }

        drawRun(args, sourceIndex, runStartIndex, blockIndex);
      }
    }
  }

  int sourceForBlock(int blockIndex) {
    if (blockIndex < 0 || blockIndex >= MOLLYS_PORRIDGE_BLOCKS) {
      return -1;
    }

    Input& input =
        module->inputs[ComputerscareMelyPorge::BLOCK_INPUT + blockIndex];
    if (input.isConnected() && input.getChannels() > 0) {
      return blockIndex;
    }
    return module->getNormalizedSourceBlock(blockIndex);
  }
};

struct MelyPorgeModeItem : MenuItem {
  ComputerscareMelyPorge* module = nullptr;
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
    const char* description = MelyPorgeModeDescriptions[mode];

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
                                        MelyPorgeModeDescriptions[mode])) +
                 34.f;
    box.size.y = 42.f;
    Widget::step();
  }
};

struct MelyPorgeSimpleModeItem : MenuItem {
  ComputerscareMelyPorge* module = nullptr;
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

struct MelyPorgeNormalizationItem : MenuItem {
  ComputerscareMelyPorge* module = nullptr;
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

struct MelyPorgeMainInputMappingItem : MenuItem {
  ComputerscareMelyPorge* module = nullptr;
  int mappingMode = 0;

  void onAction(const event::Action& e) override {
    if (module) {
      module->mainInputMappingMode = mappingMode;
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
    const char* description =
        MelyPorgeMainInputMappingDescriptions[mappingMode];

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
        CHECKMARK(module && module->mainInputMappingMode == mappingMode);
    box.size.x =
        std::max(
            bndLabelWidth(APP->window->vg, -1, text.c_str()),
            bndLabelWidth(APP->window->vg, -1,
                          MelyPorgeMainInputMappingDescriptions[mappingMode])) +
        34.f;
    box.size.y = 42.f;
    Widget::step();
  }
};

static void addMelyPorgeNormalizationItems(Menu* menu,
                                           ComputerscareMelyPorge* module) {
  for (int mode = 0; mode < ComputerscareMelyPorge::NUM_NORMALIZATION_MODES;
       mode++) {
    MelyPorgeNormalizationItem* item = new MelyPorgeNormalizationItem();
    item->text = MelyPorgeNormalizationNames[mode];
    item->module = module;
    item->normalizationMode = mode;
    menu->addChild(item);
  }
  menu->addChild(new MenuSeparator);
  menu->addChild(
      createBoolPtrMenuItem("Multiple inserts break normalization", "",
                            &module->multipleInsertsBreakNormalization));
}

static void addMelyPorgeMainInputMappingItems(Menu* menu,
                                              ComputerscareMelyPorge* module) {
  for (int mode = cpx::polyphonic::firstMappingModeValue;
       mode <= cpx::polyphonic::lastMappingModeValue; mode++) {
    MelyPorgeMainInputMappingItem* item = new MelyPorgeMainInputMappingItem();
    item->text = MelyPorgeMainInputMappingNames[mode];
    item->module = module;
    item->mappingMode = mode;
    menu->addChild(item);
  }
}

static void addMelyPorgeModeItems(Menu* menu, ComputerscareMelyPorge* module,
                                  int blockIndex, bool setAll,
                                  bool showDescriptions) {
  static const int modeOrder[] = {
      ComputerscareMelyPorge::MODE_ADD,
      ComputerscareMelyPorge::MODE_INSERT,
      ComputerscareMelyPorge::MODE_XFADE,
      ComputerscareMelyPorge::MODE_VCA_UNIPOLAR,
      ComputerscareMelyPorge::MODE_VCA_BIPOLAR,
      ComputerscareMelyPorge::MODE_MIN,
      ComputerscareMelyPorge::MODE_MAX,
      ComputerscareMelyPorge::MODE_COMPARE,
  };

  for (int i = 0; i < (int)(sizeof(modeOrder) / sizeof(modeOrder[0])); i++) {
    int mode = modeOrder[i];
    if (showDescriptions) {
      MelyPorgeModeItem* modeItem = new MelyPorgeModeItem();
      modeItem->text = MelyPorgeModeNames[mode];
      modeItem->module = module;
      modeItem->blockIndex = blockIndex;
      modeItem->mode = mode;
      modeItem->setAll = setAll;
      menu->addChild(modeItem);
    } else {
      MelyPorgeSimpleModeItem* simpleItem = new MelyPorgeSimpleModeItem();
      simpleItem->text = MelyPorgeModeNames[mode];
      simpleItem->module = module;
      simpleItem->blockIndex = blockIndex;
      simpleItem->mode = mode;
      menu->addChild(simpleItem);
    }
  }
}

struct MelyPorgeModeButton : ComputerscareBlankButton {
  ComputerscareMelyPorge* module = nullptr;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  ui::Tooltip* hoverTooltip = NULL;
  int blockIndex = 0;
  int menuFrame = -1;

  MelyPorgeModeButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
    box.size.x *= 0.82f;
  }

  ~MelyPorgeModeButton() { destroyHoverTooltip(); }

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
    hoverTooltip->text = std::string(MelyPorgeModeNames[mode]) + "\n" +
                         MelyPorgeModeDescriptions[mode];
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
      return ComputerscareMelyPorge::MODE_INSERT;
    }
    return math::clamp(module->blockModes[blockIndex], 0,
                       (int)ComputerscareMelyPorge::NUM_MODES - 1);
  }

  bool isDisabled() const {
    return module && blockIndex > module->polyChannels - 1;
  }

  void draw(const DrawArgs& args) override {
    updateMenuFrame();
    nvgSave(args.vg);
    if (isDisabled()) {
      nvgGlobalAlpha(args.vg, 0.45f);
    }
    nvgScale(args.vg, 0.82f, 1.f);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg,
                 isDisabled() ? nvgRGBA(0x00, 0x00, 0x00, 0x80) : BLACK);
    float textXOffset = isMenuOpen() ? 2.3f : -1.2f;
    float textYOffset = isMenuOpen() ? 3.6f : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + textXOffset,
            box.size.y * 0.48f + textYOffset, MelyPorgeModeCodes[currentMode()],
            NULL);
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
    addMelyPorgeModeItems(menu, module, blockIndex, false, false);
    updateMenuFrame();
  }
};

struct ComputerscareMelyPorgeWidget : ModuleWidget {
  ComputerscareMelyPorgeWidget(ComputerscareMelyPorge* module) {
    setModule(module);
    box.size = Vec(12 * 15, 380);

    ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
    panel->box.size = box.size;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareMelyPorgePanel.svg")));
    addChild(panel);

    addChild(new PolyOutputChannelsWidget(
        Vec(119.f, 4.f), module, ComputerscareMelyPorge::POLY_CHANNELS));
    addOutput(createOutput<InPort>(Vec(144.f, 3.f), module,
                                   ComputerscareMelyPorge::POLY_OUTPUT));

    addMainInputRow(module, 20.f, 23.f);

    const float startY = 52.f;
    const float rowSpacing = 39.f;
    const float leftX = 4.f;
    const float rightX = 91.f;
    const float leftColumnYOffset = 11.f;
    const float rightColumnYOffset = 0.f;

    MelyPorgeNormalizationOverlay* normalizationOverlay =
        createWidget<MelyPorgeNormalizationOverlay>(Vec(0.f, 0.f));
    normalizationOverlay->box.size = box.size;
    normalizationOverlay->module = module;
    addChild(normalizationOverlay);

    for (int i = 0; i < MOLLYS_PORRIDGE_BLOCKS; i++) {
      int row = i % 8;
      bool rightColumn = i >= 8;
      float x = rightColumn ? rightX : leftX;
      float y = startY +
                (rightColumn ? rightColumnYOffset : leftColumnYOffset) +
                row * rowSpacing;
      addBlockControls(module, i, x, y);
    }
  }

  void addMainInputRow(ComputerscareMelyPorge* module, float x, float y) {
    addInput(createInput<PointingUpPentagonPort>(
        Vec(x, y), module, ComputerscareMelyPorge::POLY_INPUT));
    addParam(createParam<SmallKnob>(Vec(x + 38.f, y + 4.f), module,
                                    ComputerscareMelyPorge::GLOBAL_ATTEN));
    addParam(createParam<SmoothKnob>(Vec(x + 62.f, y + 2.f), module,
                                     ComputerscareMelyPorge::GLOBAL_OFFSET));
  }

  void addBlockControls(ComputerscareMelyPorge* module, int blockIndex, float x,
                        float y) {
    const float jackX = x + 2.f;
    const float jackY = y + 13.f;

    MelyPorgeInsertPort* inputPort = createInput<MelyPorgeInsertPort>(
        Vec(jackX, jackY), module,
        ComputerscareMelyPorge::BLOCK_INPUT + blockIndex);
    inputPort->blockIndex = blockIndex;
    addInput(inputPort);
    MelyPorgeDisableableSmallKnob* attenKnob =
        createParam<MelyPorgeDisableableSmallKnob>(
            Vec(x + 32.f, y + 7.f), module,
            ComputerscareMelyPorge::BLOCK_ATTEN + blockIndex);
    attenKnob->module = module;
    attenKnob->channel = blockIndex;
    addParam(attenKnob);
    MelyPorgeModeButton* button =
        createWidget<MelyPorgeModeButton>(Vec(x + 27.f, y + 25.f));
    button->module = module;
    button->blockIndex = blockIndex;
    addChild(button);

    MelyPorgeDisableableSmoothKnob* offsetKnob =
        createParam<MelyPorgeDisableableSmoothKnob>(
            Vec(x + 52.f, y + 14.f), module,
            ComputerscareMelyPorge::BLOCK_OFFSET + blockIndex);
    offsetKnob->module = module;
    offsetKnob->channel = blockIndex;
    addParam(offsetKnob);

    MelyPorgeBlockNumber* label =
        createWidget<MelyPorgeBlockNumber>(Vec(jackX - 11.f, jackY - 5.f));
    label->box.size = Vec(16.f, 9.f);
    label->value = std::to_string(blockIndex + 1);
    addChild(label);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareMelyPorge* mely =
        dynamic_cast<ComputerscareMelyPorge*>(module);
    if (!mely) {
      return;
    }

    menu->addChild(new MenuSeparator);
    menu->addChild(createSubmenuItem("Set all to", "", [=](Menu* submenu) {
      addMelyPorgeModeItems(submenu, mely, 0, true, true);
    }));
    menu->addChild(createSubmenuItem(
        "Insert normalization", "",
        [=](Menu* submenu) { addMelyPorgeNormalizationItems(submenu, mely); }));
    menu->addChild(createSubmenuItem(
        "Main Input Polyphonic Wrap Mode", "", [=](Menu* submenu) {
          addMelyPorgeMainInputMappingItems(submenu, mely);
        }));
  }
};

Model* modelComputerscareMelyPorge =
    createModel<ComputerscareMelyPorge, ComputerscareMelyPorgeWidget>(
        "computerscare-mely-porge");
