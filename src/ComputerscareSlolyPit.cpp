#include <array>
#include <vector>

#include "Computerscare.hpp"

const std::string SlolyPitRoutingModeNames[3] = {"Single", "Dynamic Below",
                                                 "Dynamic Above"};

struct ComputerscareSlolyPit : Module {
  enum RoutingMode {
    ROUTING_SINGLE,
    ROUTING_DYNAMIC_BELOW,
    ROUTING_DYNAMIC_ABOVE,
  };

  std::array<std::vector<int>, 16> routing;
  int routingMode = ROUTING_SINGLE;

  enum ParamIds { NUM_PARAMS };
  enum InputIds { POLY_INPUT, NUM_INPUTS };
  enum OutputIds {
    CHANNEL_OUTPUT,
    NUM_OUTPUTS = CHANNEL_OUTPUT + 16,
  };
  enum LightIds { NUM_LIGHTS };

  ComputerscareSlolyPit() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configInput(POLY_INPUT, "Poly In");
    for (int i = 0; i < 16; i++) {
      configOutput(CHANNEL_OUTPUT + i, "Output " + std::to_string(i + 1));
    }

    resetRouting();
  }

  void resetRouting() {
    for (int i = 0; i < 16; i++) {
      routing[i].clear();
      routing[i].push_back(i);
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_t* routingJ = json_array();
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      json_t* outputRoutingJ = json_array();
      for (int inputChannel : routing[outputIndex]) {
        json_array_append_new(outputRoutingJ, json_integer(inputChannel));
      }
      json_array_append_new(routingJ, outputRoutingJ);
    }
    json_object_set_new(rootJ, "routing", routingJ);
    json_object_set_new(rootJ, "routingMode", json_integer(routingMode));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    resetRouting();

    json_t* routingModeJ = json_object_get(rootJ, "routingMode");
    if (routingModeJ) {
      int loadedRoutingMode = json_integer_value(routingModeJ);
      if (loadedRoutingMode >= ROUTING_SINGLE &&
          loadedRoutingMode <= ROUTING_DYNAMIC_ABOVE) {
        routingMode = loadedRoutingMode;
      }
    }

    json_t* routingJ = json_object_get(rootJ, "routing");
    if (!json_is_array(routingJ)) {
      return;
    }

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      json_t* outputRoutingJ = json_array_get(routingJ, outputIndex);
      if (!json_is_array(outputRoutingJ)) {
        continue;
      }

      routing[outputIndex].clear();
      size_t inputIndex;
      json_t* inputChannelJ;
      json_array_foreach(outputRoutingJ, inputIndex, inputChannelJ) {
        if (!json_is_integer(inputChannelJ)) {
          continue;
        }

        int inputChannel = json_integer_value(inputChannelJ);
        if (inputChannel >= 0 && inputChannel < 16) {
          routing[outputIndex].push_back(inputChannel);
        }
      }
    }
  }

  void process(const ProcessArgs& args) override {
    int inputChannels = inputs[POLY_INPUT].getChannels();
    if (routingMode == ROUTING_DYNAMIC_BELOW) {
      processDynamicBelow(inputChannels);
      return;
    }
    if (routingMode == ROUTING_DYNAMIC_ABOVE) {
      processDynamicAbove(inputChannels);
      return;
    }

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      setOutputFromRouting(outputIndex, routing[outputIndex], inputChannels);
    }
  }

  void processDynamicBelow(int inputChannels) {
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (!outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
        outputs[CHANNEL_OUTPUT + outputIndex].setChannels(0);
        continue;
      }

      int nextConnectedOutput = inputChannels;
      for (int nextOutputIndex = outputIndex + 1; nextOutputIndex < 16;
           nextOutputIndex++) {
        if (outputs[CHANNEL_OUTPUT + nextOutputIndex].isConnected()) {
          nextConnectedOutput = std::min(nextOutputIndex, inputChannels);
          break;
        }
      }
      setOutputFromRange(outputIndex, outputIndex, nextConnectedOutput,
                         inputChannels);
    }
  }

  void processDynamicAbove(int inputChannels) {
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (!outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
        outputs[CHANNEL_OUTPUT + outputIndex].setChannels(0);
        continue;
      }

      int previousConnectedOutput = -1;
      for (int previousOutputIndex = outputIndex - 1; previousOutputIndex >= 0;
           previousOutputIndex--) {
        if (outputs[CHANNEL_OUTPUT + previousOutputIndex].isConnected()) {
          previousConnectedOutput = previousOutputIndex;
          break;
        }
      }
      int firstInputChannel = previousConnectedOutput + 1;
      int lastInputChannel = outputIndex;
      setOutputFromRange(outputIndex, firstInputChannel, lastInputChannel + 1,
                         inputChannels);
    }
  }

  void setOutputFromRouting(int outputIndex, const std::vector<int>& inputRoute,
                            int inputChannels) {
    outputs[CHANNEL_OUTPUT + outputIndex].setChannels(inputRoute.size());
    for (size_t polyChannel = 0; polyChannel < inputRoute.size();
         polyChannel++) {
      int inputChannel = inputRoute[polyChannel];
      outputs[CHANNEL_OUTPUT + outputIndex].setVoltage(
          inputChannel < inputChannels
              ? inputs[POLY_INPUT].getVoltage(inputChannel)
              : 0.f,
          polyChannel);
    }
  }

  void setOutputFromRange(int outputIndex, int startInputChannel,
                          int endInputChannel, int inputChannels) {
    startInputChannel = clamp(startInputChannel, 0, inputChannels);
    endInputChannel = clamp(endInputChannel, startInputChannel, inputChannels);
    int outputChannels = endInputChannel - startInputChannel;

    outputs[CHANNEL_OUTPUT + outputIndex].setChannels(outputChannels);
    for (int polyChannel = 0; polyChannel < outputChannels; polyChannel++) {
      outputs[CHANNEL_OUTPUT + outputIndex].setVoltage(
          inputs[POLY_INPUT].getVoltage(startInputChannel + polyChannel),
          polyChannel);
    }
  }
};

struct SlolyPitRoutingModeItem : MenuItem {
  ComputerscareSlolyPit* module;
  int routingMode;

  void onAction(const event::Action& e) override {
    module->routingMode = routingMode;
  }

  void step() override {
    rightText = CHECKMARK(module->routingMode == routingMode);
    MenuItem::step();
  }
};

struct SlolyPitRoutingModeMenu : MenuItem {
  ComputerscareSlolyPit* module;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    for (int i = 0; i < 3; i++) {
      SlolyPitRoutingModeItem* item = new SlolyPitRoutingModeItem();
      item->text = SlolyPitRoutingModeNames[i];
      item->module = module;
      item->routingMode = i;
      menu->addChild(item);
    }
    return menu;
  }

  void step() override {
    rightText =
        SlolyPitRoutingModeNames[module->routingMode] + " " + RIGHT_ARROW;
    MenuItem::step();
  }
};

struct SlolyPitOutputLabels : Widget {
  ComputerscareSlolyPit* module;
  std::string fontPath =
      asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf");

  float rowSpacing = 19.f;
  float rowHeight = 15.f;
  float backgroundX = 6.f;
  float backgroundWidth = 17.f;
  float textRightX = 22.f;
  float textBaselineY = 12.f;

  SlolyPitOutputLabels(ComputerscareSlolyPit* module) {
    this->module = module;
    box.pos = Vec(14, 55);
    box.size = Vec(24, rowSpacing * 15 + rowHeight);
  }

  void draw(const DrawArgs& args) override {
    drawRouteBackgrounds(args);
    drawLabels(args);
  }

  void drawRouteBackgrounds(const DrawArgs& args) {
    if (!module) {
      for (int channel = 0; channel < 16; channel++) {
        drawRouteBlock(args, channel, channel + 1, channel);
      }
      return;
    }

    int inputChannels =
        module->inputs[ComputerscareSlolyPit::POLY_INPUT].getChannels();
    int visualChannels = inputChannels > 0 ? inputChannels : 16;

    if (module->routingMode == ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW) {
      drawDynamicBelow(args, visualChannels);
      return;
    }
    if (module->routingMode == ComputerscareSlolyPit::ROUTING_DYNAMIC_ABOVE) {
      drawDynamicAbove(args, visualChannels);
      return;
    }

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      drawRoutingVector(args, module->routing[outputIndex], visualChannels,
                        outputIndex);
    }
  }

  void drawDynamicBelow(const DrawArgs& args, int visualChannels) {
    int blockIndex = 0;
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (!module->outputs[ComputerscareSlolyPit::CHANNEL_OUTPUT + outputIndex]
               .isConnected()) {
        continue;
      }

      int nextConnectedOutput = visualChannels;
      for (int nextOutputIndex = outputIndex + 1; nextOutputIndex < 16;
           nextOutputIndex++) {
        if (module
                ->outputs[ComputerscareSlolyPit::CHANNEL_OUTPUT +
                          nextOutputIndex]
                .isConnected()) {
          nextConnectedOutput = std::min(nextOutputIndex, visualChannels);
          break;
        }
      }
      drawRouteBlock(args, outputIndex, nextConnectedOutput, blockIndex);
      blockIndex++;
    }
  }

  void drawDynamicAbove(const DrawArgs& args, int visualChannels) {
    int blockIndex = 0;
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (!module->outputs[ComputerscareSlolyPit::CHANNEL_OUTPUT + outputIndex]
               .isConnected()) {
        continue;
      }

      int previousConnectedOutput = -1;
      for (int previousOutputIndex = outputIndex - 1; previousOutputIndex >= 0;
           previousOutputIndex--) {
        if (module
                ->outputs[ComputerscareSlolyPit::CHANNEL_OUTPUT +
                          previousOutputIndex]
                .isConnected()) {
          previousConnectedOutput = previousOutputIndex;
          break;
        }
      }
      drawRouteBlock(args, previousConnectedOutput + 1, outputIndex + 1,
                     blockIndex);
      blockIndex++;
    }
  }

  void drawRoutingVector(const DrawArgs& args,
                         const std::vector<int>& inputRoute, int visualChannels,
                         int blockIndex) {
    int blockStart = -1;
    int previousChannel = -2;

    for (int inputChannel : inputRoute) {
      if (inputChannel < 0 || inputChannel >= visualChannels) {
        continue;
      }

      if (blockStart < 0) {
        blockStart = inputChannel;
      } else if (inputChannel != previousChannel + 1) {
        drawRouteBlock(args, blockStart, previousChannel + 1, blockIndex);
        blockIndex++;
        blockStart = inputChannel;
      }
      previousChannel = inputChannel;
    }

    if (blockStart >= 0) {
      drawRouteBlock(args, blockStart, previousChannel + 1, blockIndex);
    }
  }

  void drawRouteBlock(const DrawArgs& args, int startChannel, int endChannel,
                      int blockIndex) {
    startChannel = clamp(startChannel, 0, 16);
    endChannel = clamp(endChannel, startChannel, 16);
    if (endChannel <= startChannel) {
      return;
    }

    bool useLightBase = blockIndex % 2 == 0;
    int baseRed = useLightBase ? 0x24 : 0x24;
    int baseGreen = useLightBase ? 0xc9 : 0x86;
    int baseBlue = useLightBase ? 0xa6 : 0x73;
    float randomDarkness =
        (blockNoise(startChannel, endChannel, 4) + 1.f) * 12.f;
    int red = clamp((int)(baseRed - randomDarkness), 0, 255);
    int green = clamp((int)(baseGreen - randomDarkness), 0, 255);
    int blue = clamp((int)(baseBlue - randomDarkness), 0, 255);

    float x = backgroundX;
    float y = startChannel * rowSpacing + 1.f;
    float width = backgroundWidth;
    float height = (endChannel - startChannel - 1) * rowSpacing + rowHeight;
    float wiggle = 2.0f;
    Vec topLeft = Vec(x + blockWiggle(startChannel, endChannel, 0) * wiggle,
                      y + blockWiggle(startChannel, endChannel, 1) * wiggle);
    Vec topRight =
        Vec(x + width + blockWiggle(startChannel, endChannel, 2) * wiggle,
            y + blockWiggle(startChannel, endChannel, 3) * wiggle);
    Vec bottomRight =
        Vec(x + width + blockWiggle(startChannel, endChannel, 5) * wiggle,
            y + height + blockWiggle(startChannel, endChannel, 6) * wiggle);
    Vec bottomLeft =
        Vec(x + blockWiggle(startChannel, endChannel, 7) * wiggle,
            y + height + blockWiggle(startChannel, endChannel, 8) * wiggle);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, topLeft.x, topLeft.y);
    nvgLineTo(args.vg, topRight.x, topRight.y);
    nvgLineTo(args.vg, bottomRight.x, bottomRight.y);
    nvgLineTo(args.vg, bottomLeft.x, bottomLeft.y);
    nvgClosePath(args.vg);
    nvgFillColor(args.vg, nvgRGBA(red, green, blue, 0x9c));
    nvgFill(args.vg);
  }

  float blockNoise(int startChannel, int endChannel, int salt) {
    uint32_t seed = (startChannel + 1) * 73856093u ^
                    (endChannel + 1) * 19349663u ^ (salt + 1) * 83492791u;
    seed ^= seed >> 13;
    seed *= 1274126177u;
    seed ^= seed >> 16;
    return (seed & 0xffff) / 32767.5f - 1.f;
  }

  float blockWiggle(int startChannel, int endChannel, int salt) {
    return blockNoise(startChannel, endChannel, salt);
  }

  void drawLabels(const DrawArgs& args) {
    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 16);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextLineHeight(args.vg, 0.7);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);
    nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x00));

    for (int channel = 0; channel < 16; channel++) {
      std::string label = std::to_string(channel + 1);
      nvgText(args.vg, textRightX, textBaselineY + channel * rowSpacing,
              label.c_str(), NULL);
    }
  }
};

struct ComputerscareSlolyPitWidget : ModuleWidget {
  ComputerscareSlolyPitWidget(ComputerscareSlolyPit* module) {
    setModule(module);
    box.size = Vec(4 * 15, 380);
    {
      ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/panels/ComputerscareSlolyPitPanel.svg")));
      addChild(panel);
    }

    addInput(createInput<InPort>(Vec(4, 32), module,
                                 ComputerscareSlolyPit::POLY_INPUT));

    addChild(new SlolyPitOutputLabels(module));

    for (int i = 0; i < 16; i++) {
      addOutput(
          createOutput<TinyJack>(Vec(39, 53 + i * 19), module,
                                 ComputerscareSlolyPit::CHANNEL_OUTPUT + i));
    }
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareSlolyPit* slolyPit =
        dynamic_cast<ComputerscareSlolyPit*>(module);
    if (!slolyPit) {
      return;
    }

    menu->addChild(new MenuSeparator);

    SlolyPitRoutingModeMenu* routingModeMenu = new SlolyPitRoutingModeMenu();
    routingModeMenu->text = "Routing Mode";
    routingModeMenu->module = slolyPit;
    menu->addChild(routingModeMenu);
  }
};

Model* modelComputerscareSlolyPit =
    createModel<ComputerscareSlolyPit, ComputerscareSlolyPitWidget>(
        "computerscare-sloly-pit");
