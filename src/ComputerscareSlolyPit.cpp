#include <array>
#include <cctype>
#include <vector>

#include "Computerscare.hpp"

const std::string SlolyPitRoutingModeNames[4] = {"Single", "Dynamic Below",
                                                 "Dynamic Above", "Custom"};

struct ComputerscareSlolyPit : Module {
  enum RoutingMode {
    ROUTING_SINGLE,
    ROUTING_DYNAMIC_BELOW,
    ROUTING_DYNAMIC_ABOVE,
    ROUTING_CUSTOM,
  };

  std::array<std::vector<int>, 16> routing;
  int routingMode = ROUTING_DYNAMIC_BELOW;
  int editingOutputIndex = -1;

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
    json_object_set_new(rootJ, "editingOutputIndex",
                        json_integer(editingOutputIndex));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    resetRouting();

    json_t* routingModeJ = json_object_get(rootJ, "routingMode");
    if (routingModeJ) {
      int loadedRoutingMode = json_integer_value(routingModeJ);
      if (loadedRoutingMode >= ROUTING_SINGLE &&
          loadedRoutingMode <= ROUTING_CUSTOM) {
        routingMode = loadedRoutingMode;
      }
    }

    json_t* editingOutputIndexJ = json_object_get(rootJ, "editingOutputIndex");
    if (editingOutputIndexJ) {
      int loadedEditingOutputIndex = json_integer_value(editingOutputIndexJ);
      if (loadedEditingOutputIndex >= -1 && loadedEditingOutputIndex < 16) {
        editingOutputIndex = loadedEditingOutputIndex;
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

  void setRoutingMode(int newRoutingMode) { routingMode = newRoutingMode; }

  static int routeCharToInputChannel(char routeChar) {
    if (routeChar >= '1' && routeChar <= '9') {
      return routeChar - '1';
    }
    if (routeChar == '0') {
      return 9;
    }

    routeChar = std::tolower((unsigned char)routeChar);
    if (routeChar >= 'a' && routeChar <= 'f') {
      return 10 + routeChar - 'a';
    }

    return -1;
  }

  static char inputChannelToRouteChar(int inputChannel) {
    if (inputChannel >= 0 && inputChannel < 9) {
      return '1' + inputChannel;
    }
    if (inputChannel == 9) {
      return '0';
    }
    if (inputChannel >= 10 && inputChannel < 16) {
      return 'a' + inputChannel - 10;
    }
    return '?';
  }

  std::string routingToText(int outputIndex) {
    if (outputIndex < 0 || outputIndex >= 16) {
      return "";
    }

    std::string text;
    for (int inputChannel : routing[outputIndex]) {
      if ((int)text.size() >= 16) {
        break;
      }

      char routeChar = inputChannelToRouteChar(inputChannel);
      if (routeChar != '?') {
        text.push_back(routeChar);
      }
    }
    return text;
  }

  void setRoutingFromText(int outputIndex, const std::string& text) {
    if (outputIndex < 0 || outputIndex >= 16) {
      return;
    }

    routing[outputIndex].clear();
    for (char routeChar : text) {
      if ((int)routing[outputIndex].size() >= 16) {
        break;
      }

      int inputChannel = routeCharToInputChannel(routeChar);
      if (inputChannel >= 0 && inputChannel < 16) {
        routing[outputIndex].push_back(inputChannel);
      }
    }
  }

  std::array<std::vector<int>, 16> getRoutingForMode(int mode,
                                                     int inputChannels) {
    if (mode == ROUTING_DYNAMIC_BELOW) {
      return getDynamicBelowRouting(inputChannels);
    }
    if (mode == ROUTING_DYNAMIC_ABOVE) {
      return getDynamicAboveRouting(inputChannels);
    }
    if (mode == ROUTING_CUSTOM) {
      return routing;
    }

    return getSingleRouting();
  }

  std::array<std::vector<int>, 16> getSingleRouting() {
    std::array<std::vector<int>, 16> singleRouting;
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      singleRouting[outputIndex].push_back(outputIndex);
    }
    return singleRouting;
  }

  std::array<std::vector<int>, 16> getDynamicBelowRouting(int inputChannels) {
    std::array<std::vector<int>, 16> dynamicRouting;
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (!outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
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
      setRoutingFromRange(dynamicRouting[outputIndex], outputIndex,
                          nextConnectedOutput, inputChannels);
    }
    return dynamicRouting;
  }

  std::array<std::vector<int>, 16> getDynamicAboveRouting(int inputChannels) {
    std::array<std::vector<int>, 16> dynamicRouting;
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (!outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
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
      setRoutingFromRange(dynamicRouting[outputIndex],
                          previousConnectedOutput + 1, outputIndex + 1,
                          inputChannels);
    }
    return dynamicRouting;
  }

  void setRoutingFromRange(std::vector<int>& outputRouting,
                           int startInputChannel, int endInputChannel,
                           int inputChannels) {
    startInputChannel = clamp(startInputChannel, 0, inputChannels);
    endInputChannel = clamp(endInputChannel, startInputChannel, inputChannels);
    for (int inputChannel = startInputChannel; inputChannel < endInputChannel;
         inputChannel++) {
      outputRouting.push_back(inputChannel);
    }
  }

  void process(const ProcessArgs& args) override {
    int inputChannels = inputs[POLY_INPUT].getChannels();
    int routingChannels = inputChannels > 0 ? inputChannels : 16;
    std::array<std::vector<int>, 16> currentRouting =
        getRoutingForMode(routingMode, routingChannels);

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      setOutputFromRouting(outputIndex, currentRouting[outputIndex],
                           inputChannels);
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
    module->setRoutingMode(routingMode);
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
    for (int i = 0; i < 4; i++) {
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

struct SlolyPitRouteInputItem : MenuItem {
  ComputerscareSlolyPit* module;
  int outputIndex;
  int routeIndex;
  int inputChannel;
  bool addRoute;
  bool clearFromRoute;

  void onAction(const event::Action& e) override {
    if (!module || outputIndex < 0 || outputIndex >= 16) {
      return;
    }

    std::vector<int>& outputRouting = module->routing[outputIndex];
    if (clearFromRoute) {
      if (routeIndex >= 0 && routeIndex < (int)outputRouting.size()) {
        outputRouting.resize(routeIndex);
      }
      return;
    }

    if (inputChannel < 0 || inputChannel >= 16) {
      return;
    }

    if (addRoute) {
      outputRouting.push_back(inputChannel);
      return;
    }

    if (routeIndex >= 0 && routeIndex < (int)outputRouting.size()) {
      outputRouting[routeIndex] = inputChannel;
    }
  }
};

struct SlolyPitRouteTextField : ComputerscareTextField {
  ComputerscareSlolyPit* module;
  int outputIndex;

  void onSelectText(const event::SelectText& e) override {
    ComputerscareTextField::onSelectText(e);
    sanitizeAndApply();
  }

  void onSelectKey(const event::SelectKey& e) override {
    ComputerscareTextField::onSelectKey(e);
    sanitizeAndApply();
  }

  void onAction(const event::Action& e) override {
    sanitizeAndApply();
    ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
    if (overlay) {
      overlay->requestDelete();
    }
  }

  void sanitizeAndApply() {
    std::string sanitized;
    for (char routeChar : text) {
      if ((int)sanitized.size() >= 16) {
        break;
      }

      int inputChannel =
          ComputerscareSlolyPit::routeCharToInputChannel(routeChar);
      if (inputChannel >= 0) {
        sanitized.push_back(std::tolower((unsigned char)routeChar));
      }
    }

    if (sanitized != text) {
      text = sanitized;
      cursor = selection = std::min(cursor, (int)text.size());
    }

    if (module) {
      module->setRoutingFromText(outputIndex, text);
    }
  }
};

struct SlolyPitOutputLabels : Widget {
  ComputerscareSlolyPit* module;
  std::string fontPath =
      asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf");

  float rowSpacing = 19.f;
  float rowHeight = 15.f;
  float routeBackgroundX = 0.f;
  float routeBackgroundWidth = 17.f;
  float backgroundX = 21.f;
  float backgroundWidth = 17.f;
  float routeTextRightX = 13.f;
  float textRightX = 34.f;
  float textBaselineY = 12.f;
  int hoveredOutputIndex = -1;

  SlolyPitOutputLabels(ComputerscareSlolyPit* module) {
    this->module = module;
    box.pos = Vec(2, 55);
    box.size = Vec(36, rowSpacing * 15 + rowHeight);
  }

  void draw(const DrawArgs& args) override {
    drawRouteBackgrounds(args);
    drawEditSelection(args);
    drawMainLabelHover(args);
    drawEditRouteBackgrounds(args);
    drawEditRouteLabels(args);
    drawLabels(args);
  }

  void onHover(const event::Hover& e) override {
    hoveredOutputIndex = -1;
    if (module &&
        module->routingMode == ComputerscareSlolyPit::ROUTING_CUSTOM &&
        isInMainColumn(e.pos)) {
      int row = (int)std::floor(e.pos.y / rowSpacing);
      if (row >= 0 && row < 16) {
        hoveredOutputIndex = row;
      }
    }
    if (hoveredOutputIndex >= 0) {
      e.consume(this);
    }
    Widget::onHover(e);
  }

  void onLeave(const event::Leave& e) override {
    hoveredOutputIndex = -1;
    Widget::onLeave(e);
  }

  void onButton(const event::Button& e) override {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        (e.button != GLFW_MOUSE_BUTTON_LEFT &&
         e.button != GLFW_MOUSE_BUTTON_RIGHT) ||
        e.action != GLFW_PRESS) {
      Widget::onButton(e);
      return;
    }

    int row = (int)std::floor(e.pos.y / rowSpacing);
    if (row < 0 || row >= 16) {
      Widget::onButton(e);
      return;
    }

    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        isInMainColumn(e.pos)) {
      module->editingOutputIndex = row;
      openRoutingTextMenu(row);
      e.consume(this);
      return;
    }

    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
      Widget::onButton(e);
      return;
    }

    if (module->editingOutputIndex >= 0 && module->editingOutputIndex < 16 &&
        isInRouteColumn(e.pos)) {
      const std::vector<int>& inputRoute =
          module->routing[module->editingOutputIndex];
      int addRouteIndex = std::min((int)inputRoute.size(), 15);
      if (row < (int)inputRoute.size()) {
        openRouteInputMenu(row, false);
        e.consume(this);
        return;
      }
      if ((int)inputRoute.size() < 16 && row == addRouteIndex) {
        openRouteInputMenu(row, true);
        e.consume(this);
        return;
      }
    }

    module->editingOutputIndex = row;
    e.consume(this);
  }

  bool isInRouteColumn(Vec pos) {
    return pos.x >= routeBackgroundX - 4.f &&
           pos.x <= routeBackgroundX + routeBackgroundWidth + 4.f;
  }

  bool isInMainColumn(Vec pos) {
    return pos.x >= backgroundX - 4.f &&
           pos.x <= backgroundX + backgroundWidth + 4.f;
  }

  void openRoutingTextMenu(int outputIndex) {
    Menu* menu = createMenu();
    menu->addChild(createMenuLabel("Output " + std::to_string(outputIndex + 1) +
                                   " routing"));

    SlolyPitRouteTextField* textField = new SlolyPitRouteTextField();
    textField->module = module;
    textField->outputIndex = outputIndex;
    textField->box.size = Vec(180.f, 28.f);
    textField->multiline = false;
    textField->dimWithRoom = true;
    textField->fontSize = 20;
    textField->textOffset = Vec(4.f, 3.f);
    textField->color = nvgRGB(0xC0, 0xE7, 0xDE);
    textField->text = module->routingToText(outputIndex);
    textField->cursor = textField->selection = textField->text.size();
    menu->addChild(textField);

    menu->addChild(createMenuLabel("1-9: ch 1-9"));
    menu->addChild(createMenuLabel("  0: ch10"));
    menu->addChild(createMenuLabel("a-f: ch11-16"));
    APP->event->setSelectedWidget(textField);
    textField->selectAll();
  }

  void openRouteInputMenu(int routeIndex, bool addRoute) {
    int outputIndex = module->editingOutputIndex;
    int inputChannels = clamp(
        module->inputs[ComputerscareSlolyPit::POLY_INPUT].getChannels(), 0, 16);

    Menu* menu = createMenu();
    menu->addChild(
        createMenuLabel(addRoute ? "Add input channel" : "Set input channel"));

    if (!addRoute) {
      SlolyPitRouteInputItem* item = new SlolyPitRouteInputItem();
      item->text = "(none)";
      item->module = module;
      item->outputIndex = outputIndex;
      item->routeIndex = routeIndex;
      item->inputChannel = -1;
      item->addRoute = false;
      item->clearFromRoute = true;
      menu->addChild(item);
      menu->addChild(new MenuSeparator);
    }

    addRouteInputMenuItems(menu, routeIndex, addRoute, 0, inputChannels);

    if (inputChannels < 16) {
      if (inputChannels > 0) {
        menu->addChild(new MenuSeparator);
      }
      addRouteInputMenuItems(menu, routeIndex, addRoute, inputChannels, 16);
    }
  }

  void addRouteInputMenuItems(Menu* menu, int routeIndex, bool addRoute,
                              int startInputChannel, int endInputChannel) {
    int outputIndex = module->editingOutputIndex;
    for (int inputChannel = startInputChannel; inputChannel < endInputChannel;
         inputChannel++) {
      SlolyPitRouteInputItem* item = new SlolyPitRouteInputItem();
      item->text = std::to_string(inputChannel + 1);
      item->rightText =
          !addRoute && module->routing[outputIndex][routeIndex] == inputChannel
              ? CHECKMARK_STRING
              : "";
      item->module = module;
      item->outputIndex = outputIndex;
      item->routeIndex = routeIndex;
      item->inputChannel = inputChannel;
      item->addRoute = addRoute;
      item->clearFromRoute = false;
      menu->addChild(item);
    }
  }

  void drawRouteBackgrounds(const DrawArgs& args) {
    if (!module) {
      drawBrowserPreview(args);
      return;
    }

    int visualChannels = getVisualInputChannels();

    if (module->routingMode == ComputerscareSlolyPit::ROUTING_CUSTOM) {
      drawCustomRoutingPresence(args);
      return;
    }

    std::array<std::vector<int>, 16> currentRouting =
        module->getRoutingForMode(module->routingMode, visualChannels);
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      drawRoutingVector(args, currentRouting[outputIndex], visualChannels,
                        outputIndex);
    }
  }

  int getVisualInputChannels() {
    if (!module) {
      return 16;
    }

    int inputChannels =
        module->inputs[ComputerscareSlolyPit::POLY_INPUT].getChannels();
    return inputChannels > 0 ? inputChannels : 16;
  }

  void drawBrowserPreview(const DrawArgs& args) {
    const int fakeOutputs[] = {0, 2, 7, 9, 14};
    int blockIndex = 0;
    for (int i = 0; i < 5; i++) {
      int outputIndex = fakeOutputs[i];
      int nextOutputIndex = i < 4 ? fakeOutputs[i + 1] : 16;
      drawRouteBlock(args, outputIndex, nextOutputIndex, blockIndex);
      blockIndex++;
    }
  }

  void drawCustomRoutingPresence(const DrawArgs& args) {
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (module->routing[outputIndex].empty()) {
        continue;
      }

      drawRouteBlock(args, outputIndex, outputIndex + 1, outputIndex);
    }
  }

  void drawEditSelection(const DrawArgs& args) {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        module->editingOutputIndex < 0 || module->editingOutputIndex >= 16) {
      return;
    }

    drawRouteBlock(args, module->editingOutputIndex,
                   module->editingOutputIndex + 1, 0, true);
  }

  void drawMainLabelHover(const DrawArgs& args) {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        hoveredOutputIndex < 0 || hoveredOutputIndex >= 16) {
      return;
    }

    drawRouteOutline(args, hoveredOutputIndex, backgroundX, backgroundWidth,
                     nvgRGB(0x00, 0x00, 0x00));
  }

  void drawEditRouteBackgrounds(const DrawArgs& args) {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        module->editingOutputIndex < 0 || module->editingOutputIndex >= 16) {
      return;
    }

    const std::vector<int>& inputRoute =
        module->routing[module->editingOutputIndex];
    int inputChannels = getVisualInputChannels();
    for (size_t routeIndex = 0;
         routeIndex < inputRoute.size() && routeIndex < 16; routeIndex++) {
      bool outOfBounds = inputRoute[routeIndex] >= inputChannels;
      drawRouteBlock(args, (int)routeIndex, (int)routeIndex + 1,
                     (int)routeIndex, true, routeBackgroundX,
                     routeBackgroundWidth, outOfBounds ? 0x62 : 0xc8);
    }

    if (inputRoute.size() < 16) {
      int addRouteIndex = std::min((int)inputRoute.size(), 15);
      drawDottedRouteBlock(args, addRouteIndex, routeBackgroundX,
                           routeBackgroundWidth);
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
                      int blockIndex, bool editHighlight = false) {
    drawRouteBlock(args, startChannel, endChannel, blockIndex, editHighlight,
                   backgroundX, backgroundWidth, editHighlight ? 0xc8 : 0x9c);
  }

  void drawRouteBlock(const DrawArgs& args, int startChannel, int endChannel,
                      int blockIndex, bool editHighlight, float x, float width,
                      int alpha = 0x9c) {
    startChannel = clamp(startChannel, 0, 16);
    endChannel = clamp(endChannel, startChannel, 16);
    if (endChannel <= startChannel) {
      return;
    }

    bool useLightBase = blockIndex % 2 == 0;
    int baseRed = editHighlight ? 0xe4 : useLightBase ? 0x24 : 0x24;
    int baseGreen = editHighlight ? 0xc4 : useLightBase ? 0xc9 : 0x86;
    int baseBlue = editHighlight ? 0x21 : useLightBase ? 0xa6 : 0x73;
    float randomDarkness =
        (blockNoise(startChannel, endChannel, 4) + 1.f) * 12.f;
    int red = clamp((int)(baseRed - randomDarkness), 0, 255);
    int green = clamp((int)(baseGreen - randomDarkness), 0, 255);
    int blue = clamp((int)(baseBlue - randomDarkness), 0, 255);

    float y = startChannel * rowSpacing + 1.f;
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
    nvgFillColor(args.vg, nvgRGBA(red, green, blue, alpha));
    nvgFill(args.vg);
  }

  void drawDottedRouteBlock(const DrawArgs& args, int channel, float x,
                            float width) {
    float y = channel * rowSpacing + 1.f;
    float height = rowHeight;
    float wiggle = 2.0f;
    Vec points[4] = {
        Vec(x + blockWiggle(channel, channel + 1, 0) * wiggle,
            y + blockWiggle(channel, channel + 1, 1) * wiggle),
        Vec(x + width + blockWiggle(channel, channel + 1, 2) * wiggle,
            y + blockWiggle(channel, channel + 1, 3) * wiggle),
        Vec(x + width + blockWiggle(channel, channel + 1, 5) * wiggle,
            y + height + blockWiggle(channel, channel + 1, 6) * wiggle),
        Vec(x + blockWiggle(channel, channel + 1, 7) * wiggle,
            y + height + blockWiggle(channel, channel + 1, 8) * wiggle),
    };

    nvgFillColor(args.vg, nvgRGBA(0xe4, 0xc4, 0x21, 0xc8));
    for (int edgeIndex = 0; edgeIndex < 4; edgeIndex++) {
      Vec start = points[edgeIndex];
      Vec end = points[(edgeIndex + 1) % 4];
      float distance = start.minus(end).norm();
      int dots = std::max(2, (int)std::floor(distance / 4.f));
      for (int dotIndex = 0; dotIndex <= dots; dotIndex++) {
        float amount = (float)dotIndex / (float)dots;
        Vec dot = start.plus(end.minus(start).mult(amount));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, dot.x, dot.y, 1.1f);
        nvgFill(args.vg);
      }
    }
  }

  void drawRouteOutline(const DrawArgs& args, int channel, float x, float width,
                        NVGcolor color) {
    float y = channel * rowSpacing + 1.f;
    float height = rowHeight;
    float wiggle = 2.0f;
    Vec topLeft = Vec(x + blockWiggle(channel, channel + 1, 0) * wiggle,
                      y + blockWiggle(channel, channel + 1, 1) * wiggle);
    Vec topRight =
        Vec(x + width + blockWiggle(channel, channel + 1, 2) * wiggle,
            y + blockWiggle(channel, channel + 1, 3) * wiggle);
    Vec bottomRight =
        Vec(x + width + blockWiggle(channel, channel + 1, 5) * wiggle,
            y + height + blockWiggle(channel, channel + 1, 6) * wiggle);
    Vec bottomLeft =
        Vec(x + blockWiggle(channel, channel + 1, 7) * wiggle,
            y + height + blockWiggle(channel, channel + 1, 8) * wiggle);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, topLeft.x, topLeft.y);
    nvgLineTo(args.vg, topRight.x, topRight.y);
    nvgLineTo(args.vg, bottomRight.x, bottomRight.y);
    nvgLineTo(args.vg, bottomLeft.x, bottomLeft.y);
    nvgClosePath(args.vg);
    nvgStrokeWidth(args.vg, 1.2f);
    nvgStrokeColor(args.vg, color);
    nvgStroke(args.vg);
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

    int inputChannels = 16;
    bool fadeBeyondInputChannels =
        module && module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM;
    if (fadeBeyondInputChannels) {
      inputChannels =
          module->inputs[ComputerscareSlolyPit::POLY_INPUT].getChannels();
      fadeBeyondInputChannels = inputChannels > 0;
    }

    for (int channel = 0; channel < 16; channel++) {
      if (fadeBeyondInputChannels && channel >= inputChannels) {
        nvgFillColor(args.vg, nvgRGB(0xa8, 0xa8, 0xa0));
      } else {
        nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x00));
      }

      std::string label = std::to_string(channel + 1);
      nvgText(args.vg, textRightX, textBaselineY + channel * rowSpacing,
              label.c_str(), NULL);
    }
  }

  void drawEditRouteLabels(const DrawArgs& args) {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        module->editingOutputIndex < 0 || module->editingOutputIndex >= 16) {
      return;
    }

    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 16);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextLineHeight(args.vg, 0.7);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT);

    const std::vector<int>& inputRoute =
        module->routing[module->editingOutputIndex];
    int inputChannels = getVisualInputChannels();
    for (size_t routeIndex = 0;
         routeIndex < inputRoute.size() && routeIndex < 16; routeIndex++) {
      if (inputRoute[routeIndex] >= inputChannels) {
        nvgFillColor(args.vg, nvgRGB(0xa8, 0xa8, 0xa0));
      } else {
        nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x00));
      }

      std::string label = std::to_string(inputRoute[routeIndex] + 1);
      nvgText(args.vg, routeTextRightX, textBaselineY + routeIndex * rowSpacing,
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

    addInput(createInput<InPort>(Vec(4, 25), module,
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
