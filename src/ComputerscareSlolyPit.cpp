#include <array>
#include <cctype>
#include <cmath>
#include <utility>
#include <vector>

#include "Computerscare.hpp"

const std::string SlolyPitRoutingModeNames[4] = {"Single", "Dynamic Below",
                                                 "Dynamic Above", "Custom"};
const std::string SlolyPitRoutingModeDescriptions[4] = {
    "Split input into its polyphonic channels",
    "Split input where the output cables are patched, taking all channels "
    "including and BELOW the patched cable",
    "Split input where the output cables are patched, taking all channels "
    "including and ABOVE the patched cable",
    "16 fully independent routings"};
const std::string SlolyPitInputPoolNames[2] = {"Only active input channels",
                                               "All input channels"};
const std::string SlolyPitReplacementNames[3] = {
    "With replacement", "Without replacement", "Shuffle existing"};
const std::string SlolyPitChannelCountNames[3] = {
    "Same channels as current", "Random channels", "Same channels as input"};
const std::string SlolyPitOutputTargetNames[2] = {"Only connected outputs",
                                                  "All outputs"};
const int SLOLY_PIT_BLANK_INPUT = -1;

std::string slolyPitOrdinal(int number) {
  if (number % 100 >= 11 && number % 100 <= 13) {
    return std::to_string(number) + "th";
  }

  switch (number % 10) {
    case 1:
      return std::to_string(number) + "st";
    case 2:
      return std::to_string(number) + "nd";
    case 3:
      return std::to_string(number) + "rd";
    default:
      return std::to_string(number) + "th";
  }
}

struct ComputerscareSlolyPit : Module {
  enum RoutingMode {
    ROUTING_SINGLE,
    ROUTING_DYNAMIC_BELOW,
    ROUTING_DYNAMIC_ABOVE,
    ROUTING_CUSTOM,
  };

  enum RandomizeInputPool {
    RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS,
    RANDOMIZE_ALL_INPUT_CHANNELS,
  };

  enum RandomizeReplacement {
    RANDOMIZE_WITH_REPLACEMENT,
    RANDOMIZE_WITHOUT_REPLACEMENT,
    RANDOMIZE_SHUFFLE_EXISTING,
  };

  enum RandomizeChannelCount {
    RANDOMIZE_SAME_CHANNEL_COUNT,
    RANDOMIZE_RANDOM_CHANNEL_COUNT,
    RANDOMIZE_FIXED_CHANNEL_COUNT,
    RANDOMIZE_SAME_INPUT_CHANNEL_COUNT = RANDOMIZE_FIXED_CHANNEL_COUNT + 16,
  };

  std::array<std::vector<int>, 16> routing;
  std::array<std::vector<int>, 16> cachedRouting;
  std::array<int, 16> outputChannelCounts;
  int routingMode = ROUTING_DYNAMIC_BELOW;
  int editingOutputIndex = -1;
  bool customRoutingInitialized = false;
  int randomizeInputPool = RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS;
  int randomizeReplacement = RANDOMIZE_WITH_REPLACEMENT;
  int randomizeChannelCount = RANDOMIZE_SAME_INPUT_CHANNEL_COUNT;
  bool randomizeOnlyConnectedOutputs = false;
  int cachedRoutingMode = -1;
  int cachedRoutingChannels = -1;
  uint16_t cachedOutputConnectionMask = 0;
  bool cachedRoutingValid = false;
  rack::dsp::SchmittTrigger randomizeTriggers[16];

  enum ParamIds { MODE_BUTTON_PARAM, NUM_PARAMS };
  enum InputIds { POLY_INPUT, RANDOMIZE_INPUT, NUM_INPUTS };
  enum OutputIds {
    CHANNEL_OUTPUT,
    NUM_OUTPUTS = CHANNEL_OUTPUT + 16,
  };
  enum LightIds { NUM_LIGHTS };

  struct RoutingModeParamQuantity : SwitchQuantity {
    std::string getString() override;
    std::string getDescription() override;
  };

  ComputerscareSlolyPit() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configButton<RoutingModeParamQuantity>(MODE_BUTTON_PARAM, "Routing Mode");
    configInput(POLY_INPUT, "Poly");
    configInput(RANDOMIZE_INPUT, "Randomize Trigger");
    for (int i = 0; i < 16; i++) {
      configOutput(CHANNEL_OUTPUT + i, slolyPitOrdinal(i + 1));
    }

    getParamQuantity(MODE_BUTTON_PARAM)->randomizeEnabled = false;

    resetRouting();
    outputChannelCounts.fill(-1);
  }

  void resetRouting() {
    for (int i = 0; i < 16; i++) {
      routing[i].clear();
      routing[i].push_back(i);
    }
  }

  void onReset() override {
    if (routingMode != ROUTING_CUSTOM) {
      return;
    }

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      routing[outputIndex].clear();
    }
    customRoutingInitialized = true;
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
    json_object_set_new(rootJ, "customRoutingInitialized",
                        json_boolean(customRoutingInitialized));
    json_object_set_new(rootJ, "randomizeInputPool",
                        json_integer(randomizeInputPool));
    json_object_set_new(rootJ, "randomizeReplacement",
                        json_integer(randomizeReplacement));
    json_object_set_new(rootJ, "randomizeChannelCount",
                        json_integer(randomizeChannelCount));
    json_object_set_new(rootJ, "randomizeOnlyConnectedOutputs",
                        json_boolean(randomizeOnlyConnectedOutputs));
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

    json_t* customRoutingInitializedJ =
        json_object_get(rootJ, "customRoutingInitialized");
    if (customRoutingInitializedJ) {
      customRoutingInitialized = json_boolean_value(customRoutingInitializedJ);
    } else {
      customRoutingInitialized = true;
    }

    json_t* randomizeInputPoolJ = json_object_get(rootJ, "randomizeInputPool");
    if (randomizeInputPoolJ) {
      int loadedInputPool = json_integer_value(randomizeInputPoolJ);
      if (loadedInputPool >= RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS &&
          loadedInputPool <= RANDOMIZE_ALL_INPUT_CHANNELS) {
        randomizeInputPool = loadedInputPool;
      } else if (loadedInputPool == RANDOMIZE_SHUFFLE_EXISTING) {
        randomizeReplacement = RANDOMIZE_SHUFFLE_EXISTING;
      }
    } else {
      json_t* randomizeOnlyActiveInputsJ =
          json_object_get(rootJ, "randomizeOnlyActiveInputs");
      if (randomizeOnlyActiveInputsJ) {
        randomizeInputPool = json_boolean_value(randomizeOnlyActiveInputsJ)
                                 ? RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS
                                 : RANDOMIZE_ALL_INPUT_CHANNELS;
      }
    }

    json_t* randomizeReplacementJ =
        json_object_get(rootJ, "randomizeReplacement");
    if (randomizeReplacementJ) {
      int loadedReplacement = json_integer_value(randomizeReplacementJ);
      if (loadedReplacement >= RANDOMIZE_WITH_REPLACEMENT &&
          loadedReplacement <= RANDOMIZE_SHUFFLE_EXISTING) {
        randomizeReplacement = loadedReplacement;
      }
    } else {
      json_t* randomizeWithReplacementJ =
          json_object_get(rootJ, "randomizeWithReplacement");
      if (randomizeWithReplacementJ) {
        randomizeReplacement = json_boolean_value(randomizeWithReplacementJ)
                                   ? RANDOMIZE_WITH_REPLACEMENT
                                   : RANDOMIZE_WITHOUT_REPLACEMENT;
      }
    }

    json_t* randomizeChannelCountJ =
        json_object_get(rootJ, "randomizeChannelCount");
    if (randomizeChannelCountJ) {
      int loadedChannelCount = json_integer_value(randomizeChannelCountJ);
      if (loadedChannelCount >= RANDOMIZE_SAME_CHANNEL_COUNT &&
          loadedChannelCount <= RANDOMIZE_SAME_INPUT_CHANNEL_COUNT) {
        randomizeChannelCount = loadedChannelCount;
      }
    } else {
      json_t* randomizeSameChannelCountJ =
          json_object_get(rootJ, "randomizeSameChannelCount");
      if (randomizeSameChannelCountJ) {
        randomizeChannelCount = json_boolean_value(randomizeSameChannelCountJ)
                                    ? RANDOMIZE_SAME_CHANNEL_COUNT
                                    : RANDOMIZE_RANDOM_CHANNEL_COUNT;
      } else {
        randomizeChannelCount = RANDOMIZE_SAME_CHANNEL_COUNT;
      }
    }

    json_t* randomizeOnlyConnectedOutputsJ =
        json_object_get(rootJ, "randomizeOnlyConnectedOutputs");
    if (randomizeOnlyConnectedOutputsJ) {
      randomizeOnlyConnectedOutputs =
          json_boolean_value(randomizeOnlyConnectedOutputsJ);
    } else {
      randomizeOnlyConnectedOutputs = true;
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
        if (inputChannel == SLOLY_PIT_BLANK_INPUT ||
            (inputChannel >= 0 && inputChannel < 16)) {
          routing[outputIndex].push_back(inputChannel);
        }
      }
    }
  }

  void setRoutingMode(int newRoutingMode) {
    if (newRoutingMode == ROUTING_CUSTOM && !customRoutingInitialized) {
      int inputChannels = inputs[POLY_INPUT].getChannels();
      int routingChannels = inputChannels > 0 ? inputChannels : 16;
      routing = getRoutingForMode(routingMode, routingChannels);
      customRoutingInitialized = true;
    }

    routingMode = newRoutingMode;
  }

  void setCustomRoutingToMode(int mode) {
    int inputChannels = inputs[POLY_INPUT].getChannels();
    int routingChannels = inputChannels > 0 ? inputChannels : 16;
    routing = getRoutingForMode(mode, routingChannels);
    customRoutingInitialized = true;
  }

  void onRandomize() override {
    if (routingMode != ROUTING_CUSTOM) {
      return;
    }

    randomizeCustomRouting();
  }

  void processRandomizeTriggers() {
    int triggerChannels = inputs[RANDOMIZE_INPUT].getChannels();
    bool triggeredChannels[16] = {};
    bool triggered = false;

    for (int triggerChannel = 0; triggerChannel < 16; triggerChannel++) {
      float voltage = triggerChannel < triggerChannels
                          ? inputs[RANDOMIZE_INPUT].getVoltage(triggerChannel)
                          : 0.f;
      if (randomizeTriggers[triggerChannel].process(voltage / 2.f)) {
        triggeredChannels[triggerChannel] = true;
        triggered = true;
      }
    }

    if (!triggered || routingMode != ROUTING_CUSTOM) {
      return;
    }

    if (triggerChannels <= 1) {
      randomizeCustomRouting();
      return;
    }

    customRoutingInitialized = true;
    for (int outputIndex = 0; outputIndex < triggerChannels && outputIndex < 16;
         outputIndex++) {
      if (triggeredChannels[outputIndex]) {
        randomizeCustomOutput(outputIndex);
      }
    }
  }

  void randomizeCustomRouting() {
    customRoutingInitialized = true;

    int activeInputChannels = inputs[POLY_INPUT].getChannels();
    int inputPoolSize =
        randomizeInputPool == RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS &&
                activeInputChannels > 0
            ? activeInputChannels
            : 16;
    inputPoolSize = clamp(inputPoolSize, 1, 16);

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      randomizeCustomOutput(outputIndex, inputPoolSize);
    }
  }

  void randomizeCustomOutput(int outputIndex) {
    int activeInputChannels = inputs[POLY_INPUT].getChannels();
    int inputPoolSize =
        randomizeInputPool == RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS &&
                activeInputChannels > 0
            ? activeInputChannels
            : 16;
    randomizeCustomOutput(outputIndex, clamp(inputPoolSize, 1, 16));
  }

  void randomizeCustomOutput(int outputIndex, int inputPoolSize) {
    if (outputIndex < 0 || outputIndex >= 16) {
      return;
    }

    if (randomizeOnlyConnectedOutputs &&
        !outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
      return;
    }

    bool shuffleExisting = randomizeReplacement == RANDOMIZE_SHUFFLE_EXISTING;
    std::vector<int> inputPool = shuffleExisting
                                     ? uniqueValidInputs(routing[outputIndex])
                                     : inputRange(inputPoolSize);
    if (inputPool.empty()) {
      routing[outputIndex].clear();
      return;
    }

    int channelCount = shuffleExisting ? (int)routing[outputIndex].size()
                                       : randomizedChannelCount(outputIndex);
    channelCount = clamp(channelCount, 0, 16);
    if (randomizeReplacement != RANDOMIZE_WITH_REPLACEMENT) {
      channelCount = std::min(channelCount, (int)inputPool.size());
    }

    routing[outputIndex].clear();
    if (randomizeReplacement == RANDOMIZE_WITH_REPLACEMENT) {
      for (int routeIndex = 0; routeIndex < channelCount; routeIndex++) {
        routing[outputIndex].push_back(
            inputPool[randomIndex((int)inputPool.size())]);
      }
    } else {
      std::vector<int> shuffled = shuffledInputs(inputPool);
      for (int routeIndex = 0; routeIndex < channelCount; routeIndex++) {
        routing[outputIndex].push_back(shuffled[routeIndex]);
      }
    }
  }

  void appendCustomRouteToIndex(int outputIndex, int routeIndex,
                                int inputChannel) {
    if (outputIndex < 0 || outputIndex >= 16 || routeIndex < 0 ||
        (inputChannel != SLOLY_PIT_BLANK_INPUT &&
         (inputChannel < 0 || inputChannel >= 16))) {
      return;
    }

    std::vector<int>& outputRouting = routing[outputIndex];
    while ((int)outputRouting.size() < routeIndex &&
           (int)outputRouting.size() < 16) {
      outputRouting.push_back(
          randomInputForNewRoute(outputIndex, inputChannel));
    }

    if ((int)outputRouting.size() < 16) {
      outputRouting.push_back(inputChannel);
    }
  }

  int randomInputForNewRoute(int outputIndex, int reservedInputChannel) {
    std::vector<int> inputPool = randomizationInputPool();
    if (inputPool.empty()) {
      return clamp(reservedInputChannel, 0, 15);
    }

    if (randomizeReplacement == RANDOMIZE_WITHOUT_REPLACEMENT) {
      std::vector<int> availableInputs;
      for (int inputChannel : inputPool) {
        if (inputChannel == reservedInputChannel) {
          continue;
        }

        bool usedByRoute = false;
        for (int existingInputChannel : routing[outputIndex]) {
          if (existingInputChannel == inputChannel) {
            usedByRoute = true;
            break;
          }
        }
        if (!usedByRoute) {
          availableInputs.push_back(inputChannel);
        }
      }

      if (!availableInputs.empty()) {
        inputPool = availableInputs;
      }
    }

    return inputPool[randomIndex((int)inputPool.size())];
  }

  std::vector<int> randomizationInputPool() {
    int activeInputChannels = inputs[POLY_INPUT].getChannels();
    int inputPoolSize =
        randomizeInputPool == RANDOMIZE_ONLY_ACTIVE_INPUT_CHANNELS &&
                activeInputChannels > 0
            ? activeInputChannels
            : 16;
    return inputRange(clamp(inputPoolSize, 1, 16));
  }

  int randomizedChannelCount(int outputIndex) {
    if (randomizeChannelCount == RANDOMIZE_SAME_CHANNEL_COUNT) {
      return (int)routing[outputIndex].size();
    }
    if (randomizeChannelCount == RANDOMIZE_RANDOM_CHANNEL_COUNT) {
      return 1 + randomIndex(16);
    }
    if (randomizeChannelCount == RANDOMIZE_SAME_INPUT_CHANNEL_COUNT) {
      int inputChannels = inputs[POLY_INPUT].getChannels();
      return inputChannels > 0 ? inputChannels : 16;
    }
    return randomizeChannelCount - RANDOMIZE_FIXED_CHANNEL_COUNT + 1;
  }

  std::vector<int> inputRange(int inputChannels) {
    std::vector<int> inputs;
    for (int inputChannel = 0; inputChannel < inputChannels; inputChannel++) {
      inputs.push_back(inputChannel);
    }
    return inputs;
  }

  std::vector<int> uniqueValidInputs(const std::vector<int>& inputs) {
    std::vector<int> uniqueInputs;
    for (int inputChannel : inputs) {
      if (inputChannel < 0 || inputChannel >= 16) {
        continue;
      }

      bool alreadyAdded = false;
      for (int uniqueInput : uniqueInputs) {
        if (uniqueInput == inputChannel) {
          alreadyAdded = true;
          break;
        }
      }
      if (!alreadyAdded) {
        uniqueInputs.push_back(inputChannel);
      }
    }
    return uniqueInputs;
  }

  std::vector<int> shuffledInputs(std::vector<int> inputs) {
    std::vector<int> shuffled = inputs;
    for (int i = (int)shuffled.size() - 1; i > 0; i--) {
      int j = randomIndex(i + 1);
      std::swap(shuffled[i], shuffled[j]);
    }
    return shuffled;
  }

  static int randomIndex(int count) {
    if (count <= 0) {
      return 0;
    }
    return clamp((int)std::floor(random::uniform() * count), 0, count - 1);
  }

  static int routeCharToInputChannel(char routeChar) {
    if (routeChar == '-') {
      return SLOLY_PIT_BLANK_INPUT;
    }
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
    if (inputChannel == SLOLY_PIT_BLANK_INPUT) {
      return '-';
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
      if (inputChannel == SLOLY_PIT_BLANK_INPUT ||
          (inputChannel >= 0 && inputChannel < 16)) {
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

  const std::array<std::vector<int>, 16>& getCachedRoutingForMode(
      int mode, int inputChannels) {
    uint16_t outputConnectionMask = getOutputConnectionMask();
    bool cacheMatches = cachedRoutingValid && cachedRoutingMode == mode &&
                        cachedRoutingChannels == inputChannels &&
                        cachedOutputConnectionMask == outputConnectionMask;
    if (!cacheMatches) {
      cachedRouting = getRoutingForMode(mode, inputChannels);
      cachedRoutingMode = mode;
      cachedRoutingChannels = inputChannels;
      cachedOutputConnectionMask = outputConnectionMask;
      cachedRoutingValid = true;
    }

    return cachedRouting;
  }

  uint16_t getOutputConnectionMask() {
    uint16_t mask = 0;
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
        mask |= 1u << outputIndex;
      }
    }
    return mask;
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
    processRandomizeTriggers();

    int inputChannels = inputs[POLY_INPUT].getChannels();
    int routingChannels = inputChannels > 0 ? inputChannels : 16;

    if (routingMode == ROUTING_SINGLE) {
      for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
        setOutputFromInputChannel(outputIndex, outputIndex, inputChannels);
      }
      return;
    }

    const std::array<std::vector<int>, 16>* currentRouting =
        routingMode == ROUTING_CUSTOM
            ? &routing
            : &getCachedRoutingForMode(routingMode, routingChannels);

    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      setOutputFromRouting(outputIndex, (*currentRouting)[outputIndex],
                           inputChannels);
    }
  }

  void setOutputFromInputChannel(int outputIndex, int inputChannel,
                                 int inputChannels) {
    if (!outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
      setOutputChannels(outputIndex, 0);
      return;
    }

    setOutputChannels(outputIndex, 1);
    outputs[CHANNEL_OUTPUT + outputIndex].setVoltage(
        inputChannel < inputChannels
            ? inputs[POLY_INPUT].getVoltage(inputChannel)
            : 0.f,
        0);
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
    if (!outputs[CHANNEL_OUTPUT + outputIndex].isConnected()) {
      setOutputChannels(outputIndex, 0);
      return;
    }

    setOutputChannels(outputIndex, (int)inputRoute.size());
    for (size_t polyChannel = 0; polyChannel < inputRoute.size();
         polyChannel++) {
      int inputChannel = inputRoute[polyChannel];
      outputs[CHANNEL_OUTPUT + outputIndex].setVoltage(
          inputChannel >= 0 && inputChannel < inputChannels
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

    setOutputChannels(outputIndex, outputChannels);
    for (int polyChannel = 0; polyChannel < outputChannels; polyChannel++) {
      outputs[CHANNEL_OUTPUT + outputIndex].setVoltage(
          inputs[POLY_INPUT].getVoltage(startInputChannel + polyChannel),
          polyChannel);
    }
  }

  void setOutputChannels(int outputIndex, int channels) {
    if (outputChannelCounts[outputIndex] == channels) {
      return;
    }

    outputs[CHANNEL_OUTPUT + outputIndex].setChannels(channels);
    outputChannelCounts[outputIndex] = channels;
  }
};

std::string ComputerscareSlolyPit::RoutingModeParamQuantity::getString() {
  ComputerscareSlolyPit* slolyPit =
      dynamic_cast<ComputerscareSlolyPit*>(module);
  int mode = slolyPit ? slolyPit->routingMode
                      : ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW;
  if (mode < 0 || mode >= 4) {
    mode = ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW;
  }

  return getLabel() + "\n" + SlolyPitRoutingModeNames[mode];
}

std::string ComputerscareSlolyPit::RoutingModeParamQuantity::getDescription() {
  ComputerscareSlolyPit* slolyPit =
      dynamic_cast<ComputerscareSlolyPit*>(module);
  int mode = slolyPit ? slolyPit->routingMode
                      : ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW;
  if (mode < 0 || mode >= 4) {
    mode = ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW;
  }

  return SlolyPitRoutingModeDescriptions[mode];
}

struct SlolyPitRoutingModeItem : MenuItem {
  ComputerscareSlolyPit* module;
  int routingMode;

  void onAction(const event::Action& e) override {
    module->setRoutingMode(routingMode);
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
        SlolyPitRoutingModeDescriptions[routingMode].c_str();

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
    rightText = CHECKMARK(module->routingMode == routingMode);
    box.size.x =
        std::max(bndLabelWidth(APP->window->vg, -1, text.c_str()),
                 bndLabelWidth(
                     APP->window->vg, -1,
                     SlolyPitRoutingModeDescriptions[routingMode].c_str())) +
        34.f;
    box.size.y = 42.f;
    Widget::step();
  }
};

void addSlolyPitRoutingModeItems(Menu* menu, ComputerscareSlolyPit* module) {
  for (int i = 0; i < 4; i++) {
    SlolyPitRoutingModeItem* item = new SlolyPitRoutingModeItem();
    item->text = SlolyPitRoutingModeNames[i];
    item->module = module;
    item->routingMode = i;
    menu->addChild(item);
  }
}

struct SlolyPitSetCustomRoutingItem : MenuItem {
  ComputerscareSlolyPit* module;
  int routingMode;

  void onAction(const event::Action& e) override {
    module->setCustomRoutingToMode(routingMode);
  }
};

struct SlolyPitSetCustomRoutingMenu : MenuItem {
  ComputerscareSlolyPit* module;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    const int modes[] = {ComputerscareSlolyPit::ROUTING_SINGLE,
                         ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW,
                         ComputerscareSlolyPit::ROUTING_DYNAMIC_ABOVE};
    for (int mode : modes) {
      SlolyPitSetCustomRoutingItem* item = new SlolyPitSetCustomRoutingItem();
      item->text = SlolyPitRoutingModeNames[mode];
      item->module = module;
      item->routingMode = mode;
      menu->addChild(item);
    }
    return menu;
  }

  void step() override {
    rightText = RIGHT_ARROW;
    MenuItem::step();
  }
};

struct SlolyPitRandomizationBoolItem : MenuItem {
  ComputerscareSlolyPit* module;
  bool ComputerscareSlolyPit::* option;
  bool value;

  void onAction(const event::Action& e) override { module->*option = value; }

  void step() override {
    rightText = CHECKMARK((module->*option) == value);
    MenuItem::step();
  }
};

struct SlolyPitRandomizationBoolMenu : MenuItem {
  ComputerscareSlolyPit* module;
  bool ComputerscareSlolyPit::* option;
  const std::string* labels;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    for (int i = 0; i < 2; i++) {
      SlolyPitRandomizationBoolItem* item = new SlolyPitRandomizationBoolItem();
      item->text = labels[i];
      item->module = module;
      item->option = option;
      item->value = i == 0;
      menu->addChild(item);
    }
    return menu;
  }

  void step() override {
    rightText = labels[(module->*option) ? 0 : 1] + " " + RIGHT_ARROW;
    MenuItem::step();
  }
};

struct SlolyPitRandomizationIntItem : MenuItem {
  ComputerscareSlolyPit* module;
  int ComputerscareSlolyPit::* option;
  int value;

  void onAction(const event::Action& e) override { module->*option = value; }

  void step() override {
    rightText = CHECKMARK((module->*option) == value);
    MenuItem::step();
  }
};

struct SlolyPitInputPoolMenu : MenuItem {
  ComputerscareSlolyPit* module;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    for (int i = 0; i < 2; i++) {
      SlolyPitRandomizationIntItem* item = new SlolyPitRandomizationIntItem();
      item->text = SlolyPitInputPoolNames[i];
      item->module = module;
      item->option = &ComputerscareSlolyPit::randomizeInputPool;
      item->value = i;
      menu->addChild(item);
    }
    return menu;
  }

  void step() override {
    rightText =
        SlolyPitInputPoolNames[module->randomizeInputPool] + " " + RIGHT_ARROW;
    MenuItem::step();
  }
};

struct SlolyPitReplacementMenu : MenuItem {
  ComputerscareSlolyPit* module;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    for (int i = 0; i < 3; i++) {
      SlolyPitRandomizationIntItem* item = new SlolyPitRandomizationIntItem();
      item->text = SlolyPitReplacementNames[i];
      item->module = module;
      item->option = &ComputerscareSlolyPit::randomizeReplacement;
      item->value = i;
      menu->addChild(item);
    }
    return menu;
  }

  void step() override {
    rightText = SlolyPitReplacementNames[module->randomizeReplacement] + " " +
                RIGHT_ARROW;
    MenuItem::step();
  }
};

struct SlolyPitChannelCountMenu : MenuItem {
  ComputerscareSlolyPit* module;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    for (int i = ComputerscareSlolyPit::RANDOMIZE_SAME_CHANNEL_COUNT;
         i <= ComputerscareSlolyPit::RANDOMIZE_RANDOM_CHANNEL_COUNT; i++) {
      SlolyPitRandomizationIntItem* item = new SlolyPitRandomizationIntItem();
      item->text = SlolyPitChannelCountNames[i];
      item->module = module;
      item->option = &ComputerscareSlolyPit::randomizeChannelCount;
      item->value = i;
      menu->addChild(item);
    }
    SlolyPitRandomizationIntItem* sameInputItem =
        new SlolyPitRandomizationIntItem();
    sameInputItem->text = SlolyPitChannelCountNames[2];
    sameInputItem->module = module;
    sameInputItem->option = &ComputerscareSlolyPit::randomizeChannelCount;
    sameInputItem->value =
        ComputerscareSlolyPit::RANDOMIZE_SAME_INPUT_CHANNEL_COUNT;
    menu->addChild(sameInputItem);

    menu->addChild(new MenuSeparator);

    for (int channels = 1; channels <= 16; channels++) {
      SlolyPitRandomizationIntItem* item = new SlolyPitRandomizationIntItem();
      item->text = std::to_string(channels);
      item->module = module;
      item->option = &ComputerscareSlolyPit::randomizeChannelCount;
      item->value =
          ComputerscareSlolyPit::RANDOMIZE_FIXED_CHANNEL_COUNT + channels - 1;
      menu->addChild(item);
    }
    return menu;
  }

  void step() override {
    rightText =
        channelCountLabel(module->randomizeChannelCount) + " " + RIGHT_ARROW;
    MenuItem::step();
  }

  std::string channelCountLabel(int channelCountMode) {
    if (channelCountMode ==
        ComputerscareSlolyPit::RANDOMIZE_SAME_CHANNEL_COUNT) {
      return SlolyPitChannelCountNames[0];
    }
    if (channelCountMode ==
        ComputerscareSlolyPit::RANDOMIZE_RANDOM_CHANNEL_COUNT) {
      return SlolyPitChannelCountNames[1];
    }
    if (channelCountMode ==
        ComputerscareSlolyPit::RANDOMIZE_SAME_INPUT_CHANNEL_COUNT) {
      return SlolyPitChannelCountNames[2];
    }
    return std::to_string(channelCountMode -
                          ComputerscareSlolyPit::RANDOMIZE_FIXED_CHANNEL_COUNT +
                          1);
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

    if (inputChannel != SLOLY_PIT_BLANK_INPUT &&
        (inputChannel < 0 || inputChannel >= 16)) {
      return;
    }

    if (addRoute) {
      module->appendCustomRouteToIndex(outputIndex, routeIndex, inputChannel);
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
      if (inputChannel == SLOLY_PIT_BLANK_INPUT || inputChannel >= 0) {
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

struct SlolyPitRouteRandomizeItem : MenuItem {
  enum RandomizeMode {
    RANDOMIZE_FULL,
    RANDOMIZE_KEEP_CHANNELS,
    RANDOMIZE_SHUFFLE,
  };

  SlolyPitRouteTextField* textField;
  RandomizeMode mode;

  void onAction(const event::Action& e) override {
    applyRandomization();
    e.unconsume();
  }

  void applyRandomization() {
    if (!textField || !textField->module) {
      return;
    }

    std::string randomized = textField->text;
    if (mode == RANDOMIZE_FULL) {
      int channels = 1 + randomIndex(16);
      randomized = randomRouteText(channels);
    } else if (mode == RANDOMIZE_KEEP_CHANNELS) {
      randomized = randomRouteText(randomized.size());
    } else {
      shuffleText(randomized);
    }

    textField->text = randomized;
    textField->cursor = textField->selection = textField->text.size();
    textField->sanitizeAndApply();
    APP->event->setSelectedWidget(textField);
  }

  static int randomIndex(int count) {
    if (count <= 0) {
      return 0;
    }
    return clamp((int)std::floor(random::uniform() * count), 0, count - 1);
  }

  static std::string randomRouteText(size_t channels) {
    std::string text;
    for (size_t channel = 0; channel < channels && channel < 16; channel++) {
      text.push_back(
          ComputerscareSlolyPit::inputChannelToRouteChar(randomIndex(16)));
    }
    return text;
  }

  static void shuffleText(std::string& text) {
    for (int i = (int)text.size() - 1; i > 0; i--) {
      int j = randomIndex(i + 1);
      std::swap(text[i], text[j]);
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
  int hoveredRouteIndex = -1;
  ui::Tooltip* hoverTooltip = NULL;

  SlolyPitOutputLabels(ComputerscareSlolyPit* module) {
    this->module = module;
    box.pos = Vec(2, 66);
    box.size = Vec(36, rowSpacing * 15 + rowHeight);
  }

  ~SlolyPitOutputLabels() { destroyHoverTooltip(); }

  void draw(const DrawArgs& args) override {
    drawRouteBackgrounds(args);
    drawEditSelection(args);
    drawMainLabelHover(args);
    drawEditRouteBackgrounds(args);
    drawEditRouteHover(args);
    drawEditRouteLabels(args);
    drawLabels(args);
  }

  void onHover(const event::Hover& e) override {
    hoveredRouteIndex = getHoveredRouteIndex(e.pos);
    hoveredOutputIndex =
        hoveredRouteIndex < 0 ? getHoveredOutputIndex(e.pos) : -1;
    if (hoveredOutputIndex >= 0) {
      createHoverTooltip();
      updateHoverTooltip();
      e.consume(this);
    } else if (hoveredRouteIndex >= 0) {
      destroyHoverTooltip();
      e.consume(this);
    } else {
      destroyHoverTooltip();
    }
    Widget::onHover(e);
  }

  int getHoveredOutputIndex(Vec pos) {
    if (!module || !isInMainColumn(pos)) {
      return -1;
    }

    int row = (int)std::floor(pos.y / rowSpacing);
    if (row < 0 || row >= 16) {
      return -1;
    }

    if (module->routingMode == ComputerscareSlolyPit::ROUTING_CUSTOM) {
      return row;
    }

    int visualChannels = getVisualInputChannels();
    std::array<std::vector<int>, 16> currentRouting =
        module->getRoutingForMode(module->routingMode, visualChannels);
    for (int outputIndex = 0; outputIndex < 16; outputIndex++) {
      if (routeContainsVisualChannel(currentRouting[outputIndex], row,
                                     visualChannels)) {
        return outputIndex;
      }
    }

    return -1;
  }

  bool routeContainsVisualChannel(const std::vector<int>& outputRoute,
                                  int visualChannel, int visualChannels) {
    for (int inputChannel : outputRoute) {
      if (inputChannel >= 0 && inputChannel < visualChannels &&
          inputChannel == visualChannel) {
        return true;
      }
    }
    return false;
  }

  void onLeave(const event::Leave& e) override {
    hoveredOutputIndex = -1;
    hoveredRouteIndex = -1;
    destroyHoverTooltip();
    Widget::onLeave(e);
  }

  int getHoveredRouteIndex(Vec pos) {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        module->editingOutputIndex < 0 || module->editingOutputIndex >= 16 ||
        !isInRouteColumn(pos)) {
      return -1;
    }

    int row = (int)std::floor(pos.y / rowSpacing);
    if (row < 0 || row >= 16) {
      return -1;
    }

    const std::vector<int>& inputRoute =
        module->routing[module->editingOutputIndex];
    if (row < (int)inputRoute.size()) {
      return row;
    }
    return inputRoute.size() < 16 ? row : -1;
  }

  void createHoverTooltip() {
    if (!settings::tooltips || hoverTooltip) {
      return;
    }

    hoverTooltip = new ui::Tooltip;
    APP->scene->addChild(hoverTooltip);
  }

  void updateHoverTooltip() {
    if (!hoverTooltip || hoveredOutputIndex < 0 || hoveredOutputIndex >= 16) {
      return;
    }

    hoverTooltip->text = getHoverTooltipText(hoveredOutputIndex);
  }

  void destroyHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    APP->scene->removeChild(hoverTooltip);
    delete hoverTooltip;
    hoverTooltip = NULL;
  }

  std::string getHoverTooltipText(int outputIndex) {
    std::string text = slolyPitOrdinal(outputIndex + 1) + " Output";
    std::vector<int> outputRoute = getDisplayedRoute(outputIndex);
    text += "\n";
    text += routeToTooltipText(outputRoute);
    return text;
  }

  std::vector<int> getDisplayedRoute(int outputIndex) {
    if (!module || outputIndex < 0 || outputIndex >= 16) {
      return {};
    }

    int visualChannels = getVisualInputChannels();
    std::array<std::vector<int>, 16> currentRouting =
        module->getRoutingForMode(module->routingMode, visualChannels);
    return currentRouting[outputIndex];
  }

  std::string routeToTooltipText(const std::vector<int>& outputRoute) {
    if (outputRoute.empty()) {
      return "(none)";
    }

    std::string text;
    for (size_t routeIndex = 0; routeIndex < outputRoute.size(); routeIndex++) {
      if (routeIndex > 0) {
        text += "\n";
      }
      text += std::to_string(routeIndex + 1);
      if (outputRoute[routeIndex] == SLOLY_PIT_BLANK_INPUT) {
        text += ": 0V";
      } else {
        text += ": Input ch ";
        text += std::to_string(outputRoute[routeIndex] + 1);
      }
    }
    return text;
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
      if ((int)inputRoute.size() < 16 && row >= addRouteIndex) {
        openRouteInputMenu(row, true);
        e.consume(this);
        return;
      }
    }

    if (isInMainColumn(e.pos)) {
      module->editingOutputIndex = row;
      e.consume(this);
      return;
    }

    Widget::onButton(e);
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
    menu->addChild(createMenuLabel("  -: 0v blank"));
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel("Randomize:"));
    addRouteRandomizeItem(menu, textField, "Full",
                          SlolyPitRouteRandomizeItem::RANDOMIZE_FULL);
    addRouteRandomizeItem(menu, textField, "Keep channels",
                          SlolyPitRouteRandomizeItem::RANDOMIZE_KEEP_CHANNELS);
    addRouteRandomizeItem(menu, textField, "Shuffle",
                          SlolyPitRouteRandomizeItem::RANDOMIZE_SHUFFLE);

    APP->event->setSelectedWidget(textField);
    textField->selectAll();
  }

  void addRouteRandomizeItem(Menu* menu, SlolyPitRouteTextField* textField,
                             const std::string& text,
                             SlolyPitRouteRandomizeItem::RandomizeMode mode) {
    SlolyPitRouteRandomizeItem* item = new SlolyPitRouteRandomizeItem();
    item->text = text;
    item->textField = textField;
    item->mode = mode;
    menu->addChild(item);
  }

  void openRouteInputMenu(int routeIndex, bool addRoute) {
    int outputIndex = module->editingOutputIndex;
    int inputChannels = clamp(
        module->inputs[ComputerscareSlolyPit::POLY_INPUT].getChannels(), 0, 16);

    Menu* menu = createMenu();
    menu->addChild(
        createMenuLabel(addRoute ? "Add Input Channel" : "Set Input Channel"));
    menu->addChild(createMenuLabel("Output " + std::to_string(outputIndex + 1) +
                                   ", channel " +
                                   std::to_string(routeIndex + 1)));
    menu->addChild(createMenuLabel("Input Channel:"));

    if (!addRoute) {
      SlolyPitRouteInputItem* item = new SlolyPitRouteInputItem();
      item->text = "(end here)";
      item->module = module;
      item->outputIndex = outputIndex;
      item->routeIndex = routeIndex;
      item->inputChannel = -1;
      item->addRoute = false;
      item->clearFromRoute = true;
      menu->addChild(item);
    }

    SlolyPitRouteInputItem* blankItem = new SlolyPitRouteInputItem();
    blankItem->text = "(blank)";
    blankItem->rightText =
        !addRoute && module->routing[outputIndex][routeIndex] ==
                         SLOLY_PIT_BLANK_INPUT
            ? CHECKMARK_STRING
            : "";
    blankItem->module = module;
    blankItem->outputIndex = outputIndex;
    blankItem->routeIndex = routeIndex;
    blankItem->inputChannel = SLOLY_PIT_BLANK_INPUT;
    blankItem->addRoute = addRoute;
    blankItem->clearFromRoute = false;
    menu->addChild(blankItem);
    menu->addChild(new MenuSeparator);

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
    if (!module || hoveredOutputIndex < 0 || hoveredOutputIndex >= 16) {
      return;
    }

    if (module->routingMode == ComputerscareSlolyPit::ROUTING_CUSTOM) {
      drawRouteOutline(args, hoveredOutputIndex, hoveredOutputIndex + 1,
                       backgroundX, backgroundWidth, nvgRGB(0x00, 0x00, 0x00));
      return;
    }

    drawRouteVectorOutline(args, getDisplayedRoute(hoveredOutputIndex),
                           getVisualInputChannels());
  }

  void drawRouteVectorOutline(const DrawArgs& args,
                              const std::vector<int>& inputRoute,
                              int visualChannels) {
    int blockStart = -1;
    int previousChannel = -2;

    for (int inputChannel : inputRoute) {
      if (inputChannel < 0 || inputChannel >= visualChannels) {
        continue;
      }

      if (blockStart < 0) {
        blockStart = inputChannel;
      } else if (inputChannel != previousChannel + 1) {
        drawRouteOutline(args, blockStart, previousChannel + 1, backgroundX,
                         backgroundWidth, nvgRGB(0x00, 0x00, 0x00));
        blockStart = inputChannel;
      }
      previousChannel = inputChannel;
    }

    if (blockStart >= 0) {
      drawRouteOutline(args, blockStart, previousChannel + 1, backgroundX,
                       backgroundWidth, nvgRGB(0x00, 0x00, 0x00));
    }
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
      bool outOfBounds = inputRoute[routeIndex] == SLOLY_PIT_BLANK_INPUT ||
                         inputRoute[routeIndex] >= inputChannels;
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

  void drawEditRouteHover(const DrawArgs& args) {
    if (!module ||
        module->routingMode != ComputerscareSlolyPit::ROUTING_CUSTOM ||
        module->editingOutputIndex < 0 || module->editingOutputIndex >= 16 ||
        hoveredRouteIndex < 0 || hoveredRouteIndex >= 16) {
      return;
    }

    const std::vector<int>& inputRoute =
        module->routing[module->editingOutputIndex];
    if (hoveredRouteIndex < (int)inputRoute.size()) {
      drawRouteOutline(args, hoveredRouteIndex, routeBackgroundX,
                       routeBackgroundWidth, nvgRGB(0x00, 0x00, 0x00));
      return;
    }

    int addRouteIndex = std::min((int)inputRoute.size(), 15);
    for (int routeIndex = addRouteIndex; routeIndex <= hoveredRouteIndex;
         routeIndex++) {
      drawDottedRouteBlock(args, routeIndex, routeBackgroundX,
                           routeBackgroundWidth);
    }
    drawRoutePlus(args, hoveredRouteIndex, routeBackgroundX,
                  routeBackgroundWidth);
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

  void drawRoutePlus(const DrawArgs& args, int channel, float x, float width) {
    float centerX = x + width * 0.5f;
    float centerY = channel * rowSpacing + 1.f + rowHeight * 0.5f;
    float radius = 4.f;

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, centerX - radius, centerY);
    nvgLineTo(args.vg, centerX + radius, centerY);
    nvgMoveTo(args.vg, centerX, centerY - radius);
    nvgLineTo(args.vg, centerX, centerY + radius);
    nvgStrokeWidth(args.vg, 1.6f);
    nvgStrokeColor(args.vg, nvgRGB(0xe4, 0xc4, 0x21));
    nvgStroke(args.vg);
  }

  void drawRouteOutline(const DrawArgs& args, int channel, float x, float width,
                        NVGcolor color) {
    drawRouteOutline(args, channel, channel + 1, x, width, color);
  }

  void drawRouteOutline(const DrawArgs& args, int startChannel, int endChannel,
                        float x, float width, NVGcolor color) {
    startChannel = clamp(startChannel, 0, 16);
    endChannel = clamp(endChannel, startChannel, 16);
    if (endChannel <= startChannel) {
      return;
    }

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

      std::string label = inputRoute[routeIndex] == SLOLY_PIT_BLANK_INPUT
                              ? "-"
                              : std::to_string(inputRoute[routeIndex] + 1);
      nvgText(args.vg, routeTextRightX, textBaselineY + routeIndex * rowSpacing,
              label.c_str(), NULL);
    }
  }
};

struct SlolyPitModeButton : ComputerscareBlankButton {
  ComputerscareSlolyPit* slolyPit = nullptr;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  std::shared_ptr<Svg> routingIcons[4];
  int menuFrame = -1;
  int displayedRoutingMode = -1;

  SlolyPitModeButton() {
    box.size.x *= 0.6f;
    routingIcons[ComputerscareSlolyPit::ROUTING_SINGLE] = APP->window->loadSvg(
        asset::plugin(pluginInstance,
                      "res/components/sloly-pit-routing-icons/single.svg"));
    routingIcons[ComputerscareSlolyPit::ROUTING_DYNAMIC_BELOW] =
        APP->window->loadSvg(asset::plugin(
            pluginInstance,
            "res/components/sloly-pit-routing-icons/dynamic-below.svg"));
    routingIcons[ComputerscareSlolyPit::ROUTING_DYNAMIC_ABOVE] =
        APP->window->loadSvg(asset::plugin(
            pluginInstance,
            "res/components/sloly-pit-routing-icons/dynamic-above.svg"));
    routingIcons[ComputerscareSlolyPit::ROUTING_CUSTOM] = APP->window->loadSvg(
        asset::plugin(pluginInstance,
                      "res/components/sloly-pit-routing-icons/custom.svg"));
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

  void updateRoutingIcon() {
    if (!slolyPit || displayedRoutingMode == slolyPit->routingMode) {
      return;
    }

    displayedRoutingMode = slolyPit->routingMode;
    setIcon(routingIcons[displayedRoutingMode]);
  }

  void step() override {
    ComputerscareBlankButton::step();
    updateRoutingIcon();
    updateMenuFrame();
  }

  void draw(const DrawArgs& args) override {
    updateRoutingIcon();
    updateMenuFrame();
    nvgSave(args.vg);
    nvgScale(args.vg, 0.6f, 1.f);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);
  }

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && isMenuOpen()) {
      updateMenuFrame();
      return;
    }

    ComputerscareBlankButton::onDragEnd(e);
  }

  void onButton(const event::Button& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS) {
      ComputerscareBlankButton::onButton(e);
      return;
    }

    e.consume(this);
    destroyTooltip();
    if (!slolyPit) {
      return;
    }

    Menu* menu = createMenu();
    activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
    menu->addChild(createMenuLabel("Routing Mode"));
    addSlolyPitRoutingModeItems(menu, slolyPit);
    updateMenuFrame();
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

    addInput(createInput<InPort>(Vec(4, 31), module,
                                 ComputerscareSlolyPit::POLY_INPUT));
    addInput(createInput<TinyJack>(Vec(31, 25), module,
                                   ComputerscareSlolyPit::RANDOMIZE_INPUT));
    SlolyPitModeButton* modeButton = createParam<SlolyPitModeButton>(
        Vec(33, 43), module, ComputerscareSlolyPit::MODE_BUTTON_PARAM);
    modeButton->slolyPit = module;
    addParam(modeButton);

    addChild(new SlolyPitOutputLabels(module));

    for (int i = 0; i < 16; i++) {
      addOutput(
          createOutput<TinyJack>(Vec(41, 64 + i * 19), module,
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

    menu->addChild(createSubmenuItem(
        "Routing Mode", SlolyPitRoutingModeNames[slolyPit->routingMode],
        [=](Menu* submenu) {
          addSlolyPitRoutingModeItems(submenu, slolyPit);
        }));

    SlolyPitSetCustomRoutingMenu* setCustomRoutingMenu =
        new SlolyPitSetCustomRoutingMenu();
    setCustomRoutingMenu->text = "Set Custom Routing to";
    setCustomRoutingMenu->module = slolyPit;
    menu->addChild(setCustomRoutingMenu);

    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel("Randomization (Custom mode only)"));

    SlolyPitInputPoolMenu* inputPoolMenu = new SlolyPitInputPoolMenu();
    inputPoolMenu->text = "Input Pool";
    inputPoolMenu->module = slolyPit;
    menu->addChild(inputPoolMenu);

    SlolyPitReplacementMenu* replacementMenu = new SlolyPitReplacementMenu();
    replacementMenu->text = "Replacement";
    replacementMenu->module = slolyPit;
    menu->addChild(replacementMenu);

    SlolyPitChannelCountMenu* channelCountMenu = new SlolyPitChannelCountMenu();
    channelCountMenu->text = "Channel Count";
    channelCountMenu->module = slolyPit;
    menu->addChild(channelCountMenu);

    addRandomizationBoolMenu(
        menu, slolyPit, "Output Targets",
        &ComputerscareSlolyPit::randomizeOnlyConnectedOutputs,
        SlolyPitOutputTargetNames);
  }

  void addRandomizationBoolMenu(Menu* menu, ComputerscareSlolyPit* slolyPit,
                                const std::string& text,
                                bool ComputerscareSlolyPit::* option,
                                const std::string* labels) {
    SlolyPitRandomizationBoolMenu* item = new SlolyPitRandomizationBoolMenu();
    item->text = text;
    item->module = slolyPit;
    item->option = option;
    item->labels = labels;
    menu->addChild(item);
  }
};

Model* modelComputerscareSlolyPit =
    createModel<ComputerscareSlolyPit, ComputerscareSlolyPitWidget>(
        "computerscare-sloly-pit");
