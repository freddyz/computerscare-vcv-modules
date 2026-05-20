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

    for (int i = 0; i < 16; i++) {
      addOutputLabel(i);
      addOutput(
          createOutput<TinyJack>(Vec(39, 53 + i * 19), module,
                                 ComputerscareSlolyPit::CHANNEL_OUTPUT + i));
    }
  }

  void addOutputLabel(int index) {
    SmallLetterDisplay* label = new SmallLetterDisplay();
    label->box.pos = Vec(20, 55 + index * 19);
    label->box.size = Vec(16, 16);
    label->value = std::to_string(index + 1);
    label->fontSize = 16;
    label->letterSpacing = 0.f;
    label->textAlign = NVG_ALIGN_RIGHT;
    label->breakRowWidth = 10.f;
    addChild(label);
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
