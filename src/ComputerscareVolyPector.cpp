#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"
#include "VolyPectorRandomization.hpp"

namespace {

const int volyPectorNumKnobs = 16;
const int volyPectorNumOutputs = 16;

const std::vector<std::string> volyPectorPortLabels = {
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P"};
const std::vector<std::string> volyPectorNatoLabels = {
    "Alpha", "Bravo",  "Charlie", "Delta", "Echo", "Foxtrot",  "Golf",  "Hotel",
    "India", "Juliet", "Kilo",    "Lima",  "Mike", "November", "Oscar", "Papa"};

std::vector<std::string> withAllLabel(std::vector<std::string> labels) {
  labels.insert(labels.begin(), "(All)");
  return labels;
}

std::vector<std::string> oneToSixteenLabels() {
  std::vector<std::string> labels;
  for (int i = 1; i <= 16; i++) {
    labels.push_back(std::to_string(i));
  }
  return labels;
}

float volyPectorRandomKnobPreviewValue() { return random::uniform() * 10.f; }

}  // namespace

struct ComputerscareVolyPector : ComputerscarePolyModule {
  bool bipolarMainKnobs = false;
  int mainKnobRangeRevision = 0;
  float outputKnobValues[volyPectorNumOutputs][volyPectorNumKnobs] = {};
  float outputScaleValues[volyPectorNumOutputs] = {};
  float outputOffsetValues[volyPectorNumOutputs] = {};
  float channelScaleValues[volyPectorNumKnobs] = {};
  float channelOffsetValues[volyPectorNumKnobs] = {};
  float lastVisibleKnobValues[volyPectorNumKnobs] = {};
  float lastVisibleScaleValue = 1.f;
  float lastVisibleOffsetValue = 0.f;
  rack::dsp::SchmittTrigger channelRandomizeTriggers[volyPectorNumKnobs];
  rack::dsp::SchmittTrigger outputRandomizeTriggers[volyPectorNumOutputs];
  rack::dsp::SchmittTrigger randomizeAllTrigger;
  rack::dsp::SchmittTrigger channelWiggleTriggers[volyPectorNumKnobs];
  rack::dsp::SchmittTrigger outputWiggleTriggers[volyPectorNumOutputs];
  rack::dsp::SchmittTrigger wiggleAllTrigger;
  rack::dsp::SchmittTrigger channelInitializeTriggers[volyPectorNumKnobs];
  rack::dsp::SchmittTrigger outputInitializeTriggers[volyPectorNumOutputs];
  rack::dsp::SchmittTrigger initializeAllTrigger;
  int viewedOutput = 0;
  int viewedChannel = -1;
  bool loadingView = false;
  bool outputsDirty = true;
  bool outputConnected[volyPectorNumOutputs] = {};

  enum ParamIds {
    KNOB,
    POLY_CHANNELS = KNOB + volyPectorNumKnobs,
    GLOBAL_SCALE,
    GLOBAL_OFFSET,
    CHANNEL_SELECTOR,
    OUTPUT_SELECTOR,
    INPUT_SELECTOR,
    RANDOMIZE_PROBABILITY_CONTROL,
    RANDOMIZE_RANGE_CONTROL,
    WIGGLE_PROBABILITY_CONTROL,
    WIGGLE_RANGE_CONTROL,
    NUM_PARAMS
  };
  enum InputIds {
    CHANNEL_RANDOMIZE_INPUT,
    OUTPUT_RANDOMIZE_INPUT,
    RANDOMIZE_ALL_INPUT,
    CHANNEL_WIGGLE_INPUT,
    OUTPUT_WIGGLE_INPUT,
    WIGGLE_ALL_INPUT,
    CHANNEL_INITIALIZE_INPUT,
    OUTPUT_INITIALIZE_INPUT,
    INITIALIZE_ALL_INPUT,
    RANDOMIZE_PROBABILITY_CV_INPUT,
    RANDOMIZE_RANGE_CV_INPUT,
    WIGGLE_PROBABILITY_CV_INPUT,
    WIGGLE_RANGE_CV_INPUT,
    NUM_INPUTS
  };
  enum OutputIds { OUTPUT, NUM_OUTPUTS = OUTPUT + volyPectorNumOutputs };
  enum LightIds { NUM_LIGHTS };

  ComputerscareVolyPector() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    for (int i = 0; i < volyPectorNumKnobs; i++) {
      configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
    }
    configSwitch(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels",
                 polyChannelLabels(false));
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");
    configSwitch(CHANNEL_SELECTOR, 0.f, 16.f, 0.f, "Channel",
                 withAllLabel(oneToSixteenLabels()));
    configSwitch(OUTPUT_SELECTOR, 0.f, volyPectorNumOutputs, 1.f, "Output",
                 withAllLabel(volyPectorPortLabels));
    configSwitch(INPUT_SELECTOR, 0.f, volyPectorNumKnobs - 1, 0.f, "Input",
                 volyPectorPortLabels);
    configParam(RANDOMIZE_PROBABILITY_CONTROL, 0.f, 1.f, 1.f,
                "Randomize Probability", "%", 0.f, 100.f);
    configParam(RANDOMIZE_RANGE_CONTROL, 0.f, 1.f, 1.f,
                "Randomize Range Scale");
    configParam(WIGGLE_PROBABILITY_CONTROL, 0.f, 1.f, 1.f, "Wiggle Probability",
                "%", 0.f, 100.f);
    configParam(WIGGLE_RANGE_CONTROL, 0.f, 1.f, 1.f, "Wiggle Range Scale");

    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS)->resetEnabled = false;
    getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;
    getParamQuantity(CHANNEL_SELECTOR)->randomizeEnabled = false;
    getParamQuantity(OUTPUT_SELECTOR)->randomizeEnabled = false;
    getParamQuantity(INPUT_SELECTOR)->randomizeEnabled = false;
    getParamQuantity(CHANNEL_SELECTOR)->resetEnabled = false;
    getParamQuantity(OUTPUT_SELECTOR)->resetEnabled = false;
    getParamQuantity(INPUT_SELECTOR)->resetEnabled = false;
    getParamQuantity(RANDOMIZE_PROBABILITY_CONTROL)->randomizeEnabled = false;
    getParamQuantity(RANDOMIZE_RANGE_CONTROL)->randomizeEnabled = false;
    getParamQuantity(WIGGLE_PROBABILITY_CONTROL)->randomizeEnabled = false;
    getParamQuantity(WIGGLE_RANGE_CONTROL)->randomizeEnabled = false;

    configInput(CHANNEL_RANDOMIZE_INPUT, "Randomize Output Channel");
    configInput(OUTPUT_RANDOMIZE_INPUT, "Randomize Output Band");
    configInput(RANDOMIZE_ALL_INPUT, "Randomize all");
    configInput(CHANNEL_WIGGLE_INPUT, "Wiggle Output Channel");
    configInput(OUTPUT_WIGGLE_INPUT, "Wiggle Output Band");
    configInput(WIGGLE_ALL_INPUT, "Wiggle all");
    configInput(CHANNEL_INITIALIZE_INPUT, "Initialize Output Channel");
    configInput(OUTPUT_INITIALIZE_INPUT, "Initialize Output Band");
    configInput(INITIALIZE_ALL_INPUT, "Initialize all");
    configInput(RANDOMIZE_PROBABILITY_CV_INPUT, "Randomize Probability CV");
    configInput(RANDOMIZE_RANGE_CV_INPUT, "Randomize Range CV");
    configInput(WIGGLE_PROBABILITY_CV_INPUT, "Wiggle Probability CV");
    configInput(WIGGLE_RANGE_CV_INPUT, "Wiggle Range CV");
    for (int i = 0; i < volyPectorNumOutputs; i++) {
      configOutput(OUTPUT + i, outputName(i));
    }
    for (int i = 0; i < volyPectorNumOutputs; i++) {
      outputScaleValues[i] = 1.f;
      outputOffsetValues[i] = 0.f;
    }
    for (int i = 0; i < volyPectorNumKnobs; i++) {
      channelScaleValues[i] = 1.f;
      channelOffsetValues[i] = 0.f;
    }

    loadCurrentView();
    loadCurrentControls();
    captureVisibleControls();
    updateParamLabels();
  }

  int selectedOutput() {
    int value = math::clamp((int)std::round(params[OUTPUT_SELECTOR].getValue()),
                            0, volyPectorNumOutputs);
    return value - 1;
  }

  int selectedChannel() {
    int value =
        math::clamp((int)std::round(params[CHANNEL_SELECTOR].getValue()), 0,
                    volyPectorNumKnobs);
    return value - 1;
  }

  int normalizedOutputView() {
    int output = selectedOutput();
    return output < 0 ? 0 : output;
  }

  bool channelViewActive() { return selectedChannel() >= 0; }

  void storeViewedControls() {
    if (viewedChannel >= 0) {
      channelScaleValues[viewedChannel] = params[GLOBAL_SCALE].getValue();
      channelOffsetValues[viewedChannel] = params[GLOBAL_OFFSET].getValue();
    } else {
      outputScaleValues[viewedOutput] = params[GLOBAL_SCALE].getValue();
      outputOffsetValues[viewedOutput] = params[GLOBAL_OFFSET].getValue();
    }
  }

  void loadCurrentControls() {
    int channel = selectedChannel();
    if (channel >= 0) {
      params[GLOBAL_SCALE].setValue(channelScaleValues[channel]);
      params[GLOBAL_OFFSET].setValue(channelOffsetValues[channel]);
    } else {
      int output = normalizedOutputView();
      params[GLOBAL_SCALE].setValue(outputScaleValues[output]);
      params[GLOBAL_OFFSET].setValue(outputOffsetValues[output]);
    }
  }

  void captureVisibleControls() {
    for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
      lastVisibleKnobValues[knob] = params[KNOB + knob].getValue();
    }
    lastVisibleScaleValue = params[GLOBAL_SCALE].getValue();
    lastVisibleOffsetValue = params[GLOBAL_OFFSET].getValue();
  }

  bool visibleControlsChanged() {
    for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
      if (lastVisibleKnobValues[knob] != params[KNOB + knob].getValue()) {
        return true;
      }
    }
    return lastVisibleScaleValue != params[GLOBAL_SCALE].getValue() ||
           lastVisibleOffsetValue != params[GLOBAL_OFFSET].getValue();
  }

  void storeCurrentView() {
    if (channelViewActive()) {
      int channel = selectedChannel();
      for (int output = 0; output < volyPectorNumOutputs; output++) {
        outputKnobValues[output][channel] = params[KNOB + output].getValue();
      }
      return;
    }

    int output = normalizedOutputView();
    for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
      outputKnobValues[output][channel] = params[KNOB + channel].getValue();
    }
  }

  void storeViewedView() {
    if (viewedChannel >= 0) {
      for (int output = 0; output < volyPectorNumOutputs; output++) {
        outputKnobValues[output][viewedChannel] =
            params[KNOB + output].getValue();
      }
    } else {
      for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
        outputKnobValues[viewedOutput][channel] =
            params[KNOB + channel].getValue();
      }
    }
  }

  void loadCurrentView() {
    int channel = selectedChannel();
    int output = normalizedOutputView();
    loadingView = true;
    if (channel >= 0) {
      for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
        params[KNOB + knob].setValue(
            clampKnobValue(outputKnobValues[knob][channel]));
      }
    } else {
      for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
        params[KNOB + knob].setValue(
            clampKnobValue(outputKnobValues[output][knob]));
      }
    }
    loadingView = false;
    viewedChannel = channel;
    viewedOutput = output;
  }

  void syncView() {
    int output = normalizedOutputView();
    int channel = selectedChannel();
    if (channel != viewedChannel || output != viewedOutput) {
      storeViewedView();
      storeViewedControls();
      loadCurrentView();
      loadCurrentControls();
      captureVisibleControls();
      updateParamLabels();
      outputsDirty = true;
      return;
    }

    if (!loadingView && visibleControlsChanged()) {
      storeViewedView();
      storeViewedControls();
      captureVisibleControls();
      outputsDirty = true;
    }
  }

  void selectOutputView(int output) {
    storeViewedView();
    storeViewedControls();
    params[CHANNEL_SELECTOR].setValue(0.f);
    params[OUTPUT_SELECTOR].setValue(
        math::clamp(output, 0, volyPectorNumOutputs - 1) + 1);
    loadCurrentView();
    loadCurrentControls();
    captureVisibleControls();
    updateParamLabels();
    outputsDirty = true;
  }

  void selectChannelView(int channel) {
    storeViewedView();
    storeViewedControls();
    params[OUTPUT_SELECTOR].setValue(0.f);
    params[CHANNEL_SELECTOR].setValue(
        math::clamp(channel, 0, volyPectorNumKnobs - 1) + 1);
    loadCurrentView();
    loadCurrentControls();
    captureVisibleControls();
    updateParamLabels();
    outputsDirty = true;
  }

  float minimumAllowedKnobValue() { return bipolarMainKnobs ? -10.f : 0.f; }

  float randomizeProbability(computerscare::volypector::RandomizeMode mode) {
    int paramId = mode == computerscare::volypector::RandomizeMode::WIGGLE
                      ? WIGGLE_PROBABILITY_CONTROL
                      : RANDOMIZE_PROBABILITY_CONTROL;
    int inputId = mode == computerscare::volypector::RandomizeMode::WIGGLE
                      ? WIGGLE_PROBABILITY_CV_INPUT
                      : RANDOMIZE_PROBABILITY_CV_INPUT;
    return math::clamp(
        params[paramId].getValue() + inputs[inputId].getVoltage() / 10.f, 0.f,
        1.f);
  }

  float randomizeRangeScale(computerscare::volypector::RandomizeMode mode) {
    int paramId = mode == computerscare::volypector::RandomizeMode::WIGGLE
                      ? WIGGLE_RANGE_CONTROL
                      : RANDOMIZE_RANGE_CONTROL;
    int inputId = mode == computerscare::volypector::RandomizeMode::WIGGLE
                      ? WIGGLE_RANGE_CV_INPUT
                      : RANDOMIZE_RANGE_CV_INPUT;
    return math::clamp(
        params[paramId].getValue() + inputs[inputId].getVoltage() / 10.f, 0.f,
        1.f);
  }

  computerscare::volypector::RandomizeSettings randomizeSettings(
      computerscare::volypector::RandomizeMode mode) {
    float minAllowed = minimumAllowedKnobValue();
    float rangeScale = randomizeRangeScale(mode);
    computerscare::volypector::RandomizeSettings settings;
    settings.chance = randomizeProbability(mode);
    settings.mode = mode;
    if (mode == computerscare::volypector::RandomizeMode::REPLACE) {
      settings.minValue = minAllowed * rangeScale;
      settings.maxValue = 10.f * rangeScale;
    } else {
      settings.minValue = minAllowed;
      settings.maxValue = 10.f;
    }
    settings.wiggleMin = -2.f * rangeScale;
    settings.wiggleMax = 2.f * rangeScale;
    return settings;
  }

  float randomKnobValue(float currentValue,
                        computerscare::volypector::RandomizeMode mode) {
    return computerscare::volypector::randomizeValue(currentValue,
                                                     randomizeSettings(mode));
  }

  void restoreViewSelection(float outputSelection, float channelSelection) {
    params[OUTPUT_SELECTOR].setValue(outputSelection);
    params[CHANNEL_SELECTOR].setValue(channelSelection);
  }

  void resetCurrentView() {
    float outputSelection = params[OUTPUT_SELECTOR].getValue();
    float channelSelection = params[CHANNEL_SELECTOR].getValue();
    restoreViewSelection(outputSelection, channelSelection);
    if (channelViewActive()) {
      int channel = selectedChannel();
      for (int output = 0; output < volyPectorNumOutputs; output++) {
        outputKnobValues[output][channel] = 0.f;
      }
    } else {
      int output = normalizedOutputView();
      for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
        outputKnobValues[output][channel] = 0.f;
      }
    }
    loadCurrentView();
    updateParamLabels();
    captureVisibleControls();
    outputsDirty = true;
  }

  void randomizeCurrentView(computerscare::volypector::RandomizeMode mode) {
    float outputSelection = params[OUTPUT_SELECTOR].getValue();
    float channelSelection = params[CHANNEL_SELECTOR].getValue();
    restoreViewSelection(outputSelection, channelSelection);
    if (channelViewActive()) {
      int channel = selectedChannel();
      for (int output = 0; output < volyPectorNumOutputs; output++) {
        outputKnobValues[output][channel] =
            randomKnobValue(outputKnobValues[output][channel], mode);
      }
    } else {
      int output = normalizedOutputView();
      for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
        outputKnobValues[output][channel] =
            randomKnobValue(outputKnobValues[output][channel], mode);
      }
    }
    loadCurrentView();
    updateParamLabels();
    captureVisibleControls();
    outputsDirty = true;
  }

  void randomizeChannel(int channel,
                        computerscare::volypector::RandomizeMode mode) {
    channel = math::clamp(channel, 0, volyPectorNumKnobs - 1);
    storeViewedView();
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      outputKnobValues[output][channel] =
          randomKnobValue(outputKnobValues[output][channel], mode);
    }
    if (!channelViewActive() || selectedChannel() == channel) {
      loadCurrentView();
      captureVisibleControls();
    }
    outputsDirty = true;
  }

  void randomizeOutput(int output,
                       computerscare::volypector::RandomizeMode mode) {
    output = math::clamp(output, 0, volyPectorNumOutputs - 1);
    storeViewedView();
    for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
      outputKnobValues[output][channel] =
          randomKnobValue(outputKnobValues[output][channel], mode);
    }
    if (channelViewActive() || normalizedOutputView() == output) {
      loadCurrentView();
      captureVisibleControls();
    }
    outputsDirty = true;
  }

  void randomizeAllValues(computerscare::volypector::RandomizeMode mode) {
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
        outputKnobValues[output][channel] =
            randomKnobValue(outputKnobValues[output][channel], mode);
      }
    }
    loadCurrentView();
    updateParamLabels();
    captureVisibleControls();
    outputsDirty = true;
  }

  void initializeChannel(int channel) {
    channel = math::clamp(channel, 0, volyPectorNumKnobs - 1);
    storeViewedView();
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      outputKnobValues[output][channel] = 0.f;
    }
    if (!channelViewActive() || selectedChannel() == channel) {
      loadCurrentView();
      captureVisibleControls();
    }
    outputsDirty = true;
  }

  void initializeOutput(int output) {
    output = math::clamp(output, 0, volyPectorNumOutputs - 1);
    storeViewedView();
    for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
      outputKnobValues[output][channel] = 0.f;
    }
    if (channelViewActive() || normalizedOutputView() == output) {
      loadCurrentView();
      captureVisibleControls();
    }
    outputsDirty = true;
  }

  float highestInputVoltage(int inputId) {
    int channels = inputs[inputId].getChannels();
    float voltage = 0.f;
    for (int channel = 0; channel < channels; channel++) {
      float channelVoltage = inputs[inputId].getVoltage(channel);
      if (channelVoltage > voltage) {
        voltage = channelVoltage;
      }
    }
    return voltage;
  }

  void processTriggers() {
    bool channelRandomizeConnected =
        inputs[CHANNEL_RANDOMIZE_INPUT].isConnected();
    bool outputRandomizeConnected =
        inputs[OUTPUT_RANDOMIZE_INPUT].isConnected();
    bool randomizeAllConnected = inputs[RANDOMIZE_ALL_INPUT].isConnected();
    bool channelWiggleConnected = inputs[CHANNEL_WIGGLE_INPUT].isConnected();
    bool outputWiggleConnected = inputs[OUTPUT_WIGGLE_INPUT].isConnected();
    bool wiggleAllConnected = inputs[WIGGLE_ALL_INPUT].isConnected();
    bool channelInitializeConnected =
        inputs[CHANNEL_INITIALIZE_INPUT].isConnected();
    bool outputInitializeConnected =
        inputs[OUTPUT_INITIALIZE_INPUT].isConnected();
    bool initializeAllConnected = inputs[INITIALIZE_ALL_INPUT].isConnected();

    if (!channelRandomizeConnected && !outputRandomizeConnected &&
        !randomizeAllConnected && !channelWiggleConnected &&
        !outputWiggleConnected && !wiggleAllConnected &&
        !channelInitializeConnected && !outputInitializeConnected &&
        !initializeAllConnected) {
      return;
    }

    int channelTriggerChannels =
        channelRandomizeConnected
            ? inputs[CHANNEL_RANDOMIZE_INPUT].getChannels()
            : 0;
    int outputTriggerChannels =
        outputRandomizeConnected ? inputs[OUTPUT_RANDOMIZE_INPUT].getChannels()
                                 : 0;
    int channelWiggleChannels =
        channelWiggleConnected ? inputs[CHANNEL_WIGGLE_INPUT].getChannels() : 0;
    int outputWiggleChannels =
        outputWiggleConnected ? inputs[OUTPUT_WIGGLE_INPUT].getChannels() : 0;
    int channelInitializeChannels =
        channelInitializeConnected
            ? inputs[CHANNEL_INITIALIZE_INPUT].getChannels()
            : 0;
    int outputInitializeChannels =
        outputInitializeConnected
            ? inputs[OUTPUT_INITIALIZE_INPUT].getChannels()
            : 0;

    if (randomizeAllConnected &&
        randomizeAllTrigger.process(highestInputVoltage(RANDOMIZE_ALL_INPUT) /
                                    2.f)) {
      randomizeAllValues(computerscare::volypector::RandomizeMode::REPLACE);
    }
    if (wiggleAllConnected &&
        wiggleAllTrigger.process(highestInputVoltage(WIGGLE_ALL_INPUT) / 2.f)) {
      randomizeAllValues(computerscare::volypector::RandomizeMode::WIGGLE);
    }
    if (initializeAllConnected &&
        initializeAllTrigger.process(highestInputVoltage(INITIALIZE_ALL_INPUT) /
                                     2.f)) {
      initializeAllValues();
    }

    if (!channelRandomizeConnected && !outputRandomizeConnected &&
        !channelWiggleConnected && !outputWiggleConnected &&
        !channelInitializeConnected && !outputInitializeConnected) {
      return;
    }

    for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
      float channelVoltage =
          channelRandomizeConnected && channel < channelTriggerChannels
              ? inputs[CHANNEL_RANDOMIZE_INPUT].getVoltage(channel)
              : 0.f;
      if (channelRandomizeConnected &&
          channelRandomizeTriggers[channel].process(channelVoltage / 2.f)) {
        randomizeChannel(channel,
                         computerscare::volypector::RandomizeMode::REPLACE);
      }

      float outputVoltage =
          outputRandomizeConnected && channel < outputTriggerChannels
              ? inputs[OUTPUT_RANDOMIZE_INPUT].getVoltage(channel)
              : 0.f;
      if (outputRandomizeConnected &&
          outputRandomizeTriggers[channel].process(outputVoltage / 2.f)) {
        randomizeOutput(channel,
                        computerscare::volypector::RandomizeMode::REPLACE);
      }

      float channelWiggleVoltage =
          channelWiggleConnected && channel < channelWiggleChannels
              ? inputs[CHANNEL_WIGGLE_INPUT].getVoltage(channel)
              : 0.f;
      if (channelWiggleConnected &&
          channelWiggleTriggers[channel].process(channelWiggleVoltage / 2.f)) {
        randomizeChannel(channel,
                         computerscare::volypector::RandomizeMode::WIGGLE);
      }

      float outputWiggleVoltage =
          outputWiggleConnected && channel < outputWiggleChannels
              ? inputs[OUTPUT_WIGGLE_INPUT].getVoltage(channel)
              : 0.f;
      if (outputWiggleConnected &&
          outputWiggleTriggers[channel].process(outputWiggleVoltage / 2.f)) {
        randomizeOutput(channel,
                        computerscare::volypector::RandomizeMode::WIGGLE);
      }

      float channelInitializeVoltage =
          channelInitializeConnected && channel < channelInitializeChannels
              ? inputs[CHANNEL_INITIALIZE_INPUT].getVoltage(channel)
              : 0.f;
      if (channelInitializeConnected &&
          channelInitializeTriggers[channel].process(channelInitializeVoltage /
                                                     2.f)) {
        initializeChannel(channel);
      }

      float outputInitializeVoltage =
          outputInitializeConnected && channel < outputInitializeChannels
              ? inputs[OUTPUT_INITIALIZE_INPUT].getVoltage(channel)
              : 0.f;
      if (outputInitializeConnected &&
          outputInitializeTriggers[channel].process(outputInitializeVoltage /
                                                    2.f)) {
        initializeOutput(channel);
      }
    }
  }

  void initializeAllValues() {
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
        outputKnobValues[output][channel] = 0.f;
      }
      outputScaleValues[output] = 1.f;
      outputOffsetValues[output] = 0.f;
    }
    for (int channel = 0; channel < volyPectorNumKnobs; channel++) {
      channelScaleValues[channel] = 1.f;
      channelOffsetValues[channel] = 0.f;
    }
    loadCurrentView();
    loadCurrentControls();
    updateParamLabels();
    captureVisibleControls();
    outputsDirty = true;
  }

  void syncOutputConnections() {
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      bool connected = outputs[OUTPUT + output].isConnected();
      if (outputConnected[output] != connected) {
        outputConnected[output] = connected;
        outputsDirty = true;
      }
    }
  }

  void onRandomize() override {
    randomizeCurrentView(computerscare::volypector::RandomizeMode::REPLACE);
  }

  void resetVisibleRandomizeControls() {
    params[RANDOMIZE_PROBABILITY_CONTROL].setValue(1.f);
    params[RANDOMIZE_RANGE_CONTROL].setValue(1.f);
    params[WIGGLE_PROBABILITY_CONTROL].setValue(1.f);
    params[WIGGLE_RANGE_CONTROL].setValue(1.f);
  }

  void onReset() override {
    resetCurrentView();
    resetVisibleRandomizeControls();
  }

  float clampKnobValue(float value) {
    return math::clamp(value, bipolarMainKnobs ? -10.f : 0.f, 10.f);
  }

  float visibleOrStoredValue(int output, int channel) {
    if (viewedChannel == channel) {
      return params[KNOB + output].getValue();
    }
    if (viewedChannel < 0 && viewedOutput == output) {
      return params[KNOB + channel].getValue();
    }
    return outputKnobValues[output][channel];
  }

  float scaledOutputValue(int output, int channel) {
    float value = visibleOrStoredValue(output, channel);
    float outputScale = outputScaleValues[output];
    float channelScale = channelScaleValues[channel];
    float outputOffset = outputOffsetValues[output];
    float channelOffset = channelOffsetValues[channel];
    if (viewedChannel == channel) {
      channelScale = params[GLOBAL_SCALE].getValue();
      channelOffset = params[GLOBAL_OFFSET].getValue();
    } else if (viewedChannel < 0 && viewedOutput == output) {
      outputScale = params[GLOBAL_SCALE].getValue();
      outputOffset = params[GLOBAL_OFFSET].getValue();
    }
    return value * outputScale * channelScale + outputOffset + channelOffset;
  }

  std::string outputName(int output) { return outputBandName(output); }

  std::string outputBandName(int output) {
    return volyPectorNatoLabels[math::clamp(output, 0,
                                            volyPectorNumOutputs - 1)] +
           " Band";
  }

  std::string channelName(int channel) {
    return "Channel " +
           std::to_string(math::clamp(channel, 0, volyPectorNumKnobs - 1) + 1);
  }

  void updateParamLabels() {
    int channel = selectedChannel();
    int output = normalizedOutputView();
    for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
      engine::ParamQuantity* pq = getParamQuantity(KNOB + knob);
      if (!pq) continue;
      if (channel >= 0) {
        pq->name = outputName(knob) + ", " + channelName(channel);
      } else {
        pq->name = outputName(output) + ", " + channelName(knob);
      }
    }

    engine::ParamQuantity* scalePq = getParamQuantity(GLOBAL_SCALE);
    engine::ParamQuantity* offsetPq = getParamQuantity(GLOBAL_OFFSET);
    if (channel >= 0) {
      if (scalePq) scalePq->name = channelName(channel) + " Scale";
      if (offsetPq) offsetPq->name = channelName(channel) + " Offset";
    } else {
      if (scalePq) scalePq->name = outputName(output) + " Scale";
      if (offsetPq) offsetPq->name = outputName(output) + " Offset";
    }
  }

  void setMainKnobRange(bool bipolar) {
    bool changed = bipolarMainKnobs != bipolar;
    bipolarMainKnobs = bipolar;
    for (int i = 0; i < volyPectorNumKnobs; i++) {
      engine::ParamQuantity* pq = getParamQuantity(KNOB + i);
      if (!pq) continue;
      pq->minValue = bipolar ? -10.f : 0.f;
      pq->maxValue = 10.f;
      pq->defaultValue = 0.f;
      pq->setValue(math::clamp(pq->getValue(), pq->minValue, pq->maxValue));
    }
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
        outputKnobValues[output][knob] = math::clamp(
            outputKnobValues[output][knob], bipolar ? -10.f : 0.f, 10.f);
      }
    }
    if (changed) {
      mainKnobRangeRevision++;
      outputsDirty = true;
    }
  }

  bool readBipolarMainKnobs(json_t* rootJ) {
    json_t* bipolarMainKnobsJ = json_object_get(rootJ, "bipolarMainKnobs");
    return bipolarMainKnobsJ && json_boolean_value(bipolarMainKnobsJ);
  }

  json_t* dataToJson() override {
    storeViewedView();
    storeViewedControls();
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "bipolarMainKnobs",
                        json_boolean(bipolarMainKnobs));
    json_t* outputKnobValuesJ = json_array();
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      json_t* outputJ = json_array();
      for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
        json_array_append_new(outputJ,
                              json_real(outputKnobValues[output][knob]));
      }
      json_array_append_new(outputKnobValuesJ, outputJ);
    }
    json_object_set_new(rootJ, "outputKnobValues", outputKnobValuesJ);
    json_t* outputScaleValuesJ = json_array();
    json_t* outputOffsetValuesJ = json_array();
    json_t* channelScaleValuesJ = json_array();
    json_t* channelOffsetValuesJ = json_array();
    for (int i = 0; i < volyPectorNumOutputs; i++) {
      json_array_append_new(outputScaleValuesJ,
                            json_real(outputScaleValues[i]));
      json_array_append_new(outputOffsetValuesJ,
                            json_real(outputOffsetValues[i]));
    }
    for (int i = 0; i < volyPectorNumKnobs; i++) {
      json_array_append_new(channelScaleValuesJ,
                            json_real(channelScaleValues[i]));
      json_array_append_new(channelOffsetValuesJ,
                            json_real(channelOffsetValues[i]));
    }
    json_object_set_new(rootJ, "outputScaleValues", outputScaleValuesJ);
    json_object_set_new(rootJ, "outputOffsetValues", outputOffsetValuesJ);
    json_object_set_new(rootJ, "channelScaleValues", channelScaleValuesJ);
    json_object_set_new(rootJ, "channelOffsetValues", channelOffsetValuesJ);
    return rootJ;
  }

  void paramsFromJson(json_t* rootJ) override {
    json_t* dataJ = json_object_get(rootJ, "data");
    if (dataJ) {
      setMainKnobRange(readBipolarMainKnobs(dataJ));
    }
    Module::paramsFromJson(rootJ);
    updateParamLabels();
    captureVisibleControls();
    outputsDirty = true;
  }

  void dataFromJson(json_t* rootJ) override {
    setMainKnobRange(readBipolarMainKnobs(rootJ));
    json_t* outputScaleValuesJ = json_object_get(rootJ, "outputScaleValues");
    json_t* outputOffsetValuesJ = json_object_get(rootJ, "outputOffsetValues");
    json_t* channelScaleValuesJ = json_object_get(rootJ, "channelScaleValues");
    json_t* channelOffsetValuesJ =
        json_object_get(rootJ, "channelOffsetValues");
    if (outputScaleValuesJ) {
      for (int i = 0; i < volyPectorNumOutputs; i++) {
        json_t* valueJ = json_array_get(outputScaleValuesJ, i);
        if (valueJ) {
          outputScaleValues[i] = json_number_value(valueJ);
        }
      }
    }
    if (outputOffsetValuesJ) {
      for (int i = 0; i < volyPectorNumOutputs; i++) {
        json_t* valueJ = json_array_get(outputOffsetValuesJ, i);
        if (valueJ) {
          outputOffsetValues[i] = json_number_value(valueJ);
        }
      }
    }
    if (channelScaleValuesJ) {
      for (int i = 0; i < volyPectorNumKnobs; i++) {
        json_t* valueJ = json_array_get(channelScaleValuesJ, i);
        if (valueJ) {
          channelScaleValues[i] = json_number_value(valueJ);
        }
      }
    }
    if (channelOffsetValuesJ) {
      for (int i = 0; i < volyPectorNumKnobs; i++) {
        json_t* valueJ = json_array_get(channelOffsetValuesJ, i);
        if (valueJ) {
          channelOffsetValues[i] = json_number_value(valueJ);
        }
      }
    }
    json_t* outputKnobValuesJ = json_object_get(rootJ, "outputKnobValues");
    if (outputKnobValuesJ) {
      for (int output = 0; output < volyPectorNumOutputs; output++) {
        json_t* outputJ = json_array_get(outputKnobValuesJ, output);
        if (!outputJ) continue;
        for (int knob = 0; knob < volyPectorNumKnobs; knob++) {
          json_t* valueJ = json_array_get(outputJ, knob);
          if (valueJ) {
            outputKnobValues[output][knob] =
                clampKnobValue(json_number_value(valueJ));
          }
        }
      }
      loadCurrentView();
      loadCurrentControls();
      updateParamLabels();
      captureVisibleControls();
      outputsDirty = true;
    } else {
      loadCurrentView();
      loadCurrentControls();
      updateParamLabels();
      storeViewedView();
      storeViewedControls();
      captureVisibleControls();
      outputsDirty = true;
    }
  }

  void process(const ProcessArgs& args) override {
    (void)args;
    processTriggers();
    ComputerscarePolyModule::checkCounter();
    if (counter == 0) {
      syncOutputConnections();
    }
    if (counter == 0 || outputsDirty) {
      syncView();
    }
    if (!outputsDirty) {
      return;
    }
    for (int output = 0; output < volyPectorNumOutputs; output++) {
      if (!outputConnected[output]) {
        outputs[OUTPUT + output].setChannels(0);
        continue;
      }
      outputs[OUTPUT + output].setChannels(polyChannels);
      for (int channel = 0; channel < polyChannels; channel++) {
        outputs[OUTPUT + output].setVoltage(scaledOutputValue(output, channel),
                                            channel);
      }
    }
    outputsDirty = false;
  }

  void checkPoly() override {
    int oldPolyChannels = polyChannels;
    polyChannels = params[POLY_CHANNELS].getValue();
    if (polyChannels == 0) {
      polyChannels = 16;
      params[POLY_CHANNELS].setValue(16);
    }
    if (oldPolyChannels != polyChannels) {
      outputsDirty = true;
    }
  }
};

struct VolyPectorNoRandomSmallKnob : SmallKnob {
  bool previewMode = false;
  float previewValue = 1.f;

  VolyPectorNoRandomSmallKnob() { SmallKnob(); }

  void draw(const DrawArgs& args) override {
    if (previewMode && !getParamQuantity()) {
      float angle = math::rescale(previewValue, -2.f, 2.f, minAngle, maxAngle);
      math::Vec center = sw->box.getCenter();
      tw->identity();
      tw->translate(center);
      tw->rotate(angle);
      tw->translate(center.neg());
      fb->dirty = true;
    }
    SmallKnob::draw(args);
  }
};

struct VolyPectorNoRandomMediumSmallKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
  bool previewMode = false;
  float previewValue = 0.f;

  VolyPectorNoRandomMediumSmallKnob() {
    setSvg(enabledSvg);
    ComputerscareRoundKnob();
  }

  void draw(const DrawArgs& args) override {
    if (previewMode && !getParamQuantity()) {
      float angle =
          math::rescale(previewValue, -10.f, 10.f, minAngle, maxAngle);
      math::Vec center = sw->box.getCenter();
      tw->identity();
      tw->translate(center);
      tw->rotate(angle);
      tw->translate(center.neg());
      fb->dirty = true;
    }
    ComputerscareRoundKnob::draw(args);
  }
};

struct VolyPectorDisableableSmoothKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  int mainKnobRangeRevision = -1;
  ComputerscarePolyModule* module = NULL;
  bool previewMode = false;
  bool previewChannelView = false;
  int previewSelectedChannel = 0;
  int previewPolyChannels = 16;
  float previewValue = 0.f;

  VolyPectorDisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  void step() override {
    if (module) {
      ComputerscareVolyPector* pobs =
          dynamic_cast<ComputerscareVolyPector*>(module);
      if (pobs && mainKnobRangeRevision != pobs->mainKnobRangeRevision) {
        event::Change eChange;
        onChange(eChange);
        mainKnobRangeRevision = pobs->mainKnobRangeRevision;
      }

      bool candidate =
          pobs &&
          ((!pobs->channelViewActive() && channel > module->polyChannels - 1) ||
           (pobs->channelViewActive() &&
            pobs->selectedChannel() > module->polyChannels - 1));
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        event::Change eChange;
        onChange(eChange);
        fb->dirty = true;
        disabled = candidate;
      }
    } else if (previewMode) {
      bool candidate =
          (!previewChannelView && channel > previewPolyChannels - 1) ||
          (previewChannelView &&
           previewSelectedChannel > previewPolyChannels - 1);
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        event::Change eChange;
        onChange(eChange);
        fb->dirty = true;
        disabled = candidate;
      }
    }
    ComputerscareRoundKnob::step();
  }

  void draw(const DrawArgs& args) override {
    if (previewMode && !module) {
      float angle = math::rescale(previewValue, 0.f, 10.f, minAngle, maxAngle);
      math::Vec center = sw->box.getCenter();
      tw->identity();
      tw->translate(center);
      tw->rotate(angle);
      tw->translate(center.neg());
      fb->dirty = true;
    }
    ComputerscareRoundKnob::draw(args);
  }
};

struct VolyPectorKnobLabel : SmallLetterDisplay {
  ComputerscareVolyPector* module = NULL;
  int index = 0;
  bool previousChannelView = false;
  bool previewMode = false;
  bool previewChannelView = false;

  void draw(const DrawArgs& args) override {
    bool channelView = module ? module->channelViewActive()
                              : (previewMode && previewChannelView);
    if (channelView != previousChannelView || value.empty()) {
      value =
          channelView ? volyPectorPortLabels[index] : std::to_string(index + 1);
      previousChannelView = channelView;
    }
    SmallLetterDisplay::draw(args);
  }
};

struct VolyPectorViewTitle : Widget {
  ComputerscareVolyPector* module = NULL;
  bool previewMode = false;
  bool previewChannelView = false;
  int previewSelectedOutput = 0;
  int previewSelectedChannel = 0;
  float rotation = -0.11f;
  float skew = 0.08f;
  float xScale = 1.04f;
  float yScale = 0.96f;
  float fontSize = 16.5f;
  std::string fontPath =
      asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf");

  std::string title() {
    if (module) {
      if (module->channelViewActive()) {
        return "Channel " + std::to_string(module->selectedChannel() + 1);
      }
      return volyPectorNatoLabels[module->normalizedOutputView()] + " Band";
    }
    if (previewMode && previewChannelView) {
      return "Channel " + std::to_string(previewSelectedChannel + 1);
    }
    return volyPectorNatoLabels[previewSelectedOutput] + " Band";
  }

  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
    if (!font) {
      return;
    }

    nvgSave(args.vg);
    nvgTranslate(args.vg, box.size.x * 0.5f, box.size.y * 0.5f);
    nvgRotate(args.vg, rotation);
    nvgSkewX(args.vg, skew);
    nvgScale(args.vg, xScale, yScale);
    nvgFontFaceId(args.vg, font->handle);
    std::string text = title();
    float currentFontSize = text == "November Band" ? 13.5f : fontSize;
    nvgFontSize(args.vg, currentFontSize);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x00));
    nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
    nvgRestore(args.vg);
  }
};

struct VolyPectorLabelButton : ComputerscareBlankButton {
  ComputerscareVolyPector* module = NULL;
  ui::Tooltip* hoverTooltip = NULL;
  std::string value;
  int outputIndex = 0;
  int channelIndex = 0;
  bool outputLabel = false;
  bool pressed = false;
  bool latchedDown = false;
  bool previewMode = false;
  bool previewChannelView = false;
  int previewSelectedOutput = 0;
  int previewSelectedChannel = 0;
  int previewPolyChannels = 16;
  float xScale = 0.66f;
  float yScale = 1.46f;
  Vec weirdOffset = Vec(0.f, 0.f);

  VolyPectorLabelButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
  }

  ~VolyPectorLabelButton() { destroyHoverTooltip(); }

  void configure(int index, bool isOutputLabel) {
    outputIndex = index;
    channelIndex = index;
    outputLabel = isOutputLabel;
    xScale = outputLabel ? 0.7f : 0.62f;
    yScale = outputLabel ? 1.52f : 1.46f;
    weirdOffset = Vec(0.f, 0.f);
    box.size.x *= xScale;
    box.size.y *= yScale;
  }

  std::string tooltipText() const {
    if (outputLabel) {
      return volyPectorNatoLabels[outputIndex] + " Band";
    }
    return "Channel " + std::to_string(channelIndex + 1);
  }

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

    hoverTooltip->text = tooltipText();
  }

  void destroyHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    APP->scene->removeChild(hoverTooltip);
    delete hoverTooltip;
    hoverTooltip = NULL;
  }

  void setDownFrame(bool down) {
    if (latchedDown == down) {
      return;
    }
    latchedDown = down;
    if (frames.size() > 1) {
      sw->setSvg(frames[down ? 1 : 0]);
      fb->setDirty();
    }
  }

  void setPressedState(bool isPressed) {
    pressed = isPressed;
    setDownFrame(pressed || selected());
  }

  bool selected() {
    if (outputLabel) {
      if (module) {
        return module->selectedOutput() == outputIndex &&
               !module->channelViewActive();
      }
      return previewMode && !previewChannelView &&
             previewSelectedOutput == outputIndex;
    }
    if (module) {
      return module->selectedChannel() == channelIndex;
    }
    return previewMode && previewChannelView &&
           previewSelectedChannel == channelIndex;
  }

  bool inactive() {
    if (outputLabel) {
      return false;
    }
    int activePolyChannels = module ? module->polyChannels
                                    : (previewMode ? previewPolyChannels : 16);
    return channelIndex > activePolyChannels - 1;
  }

  void step() override {
    ComputerscareBlankButton::step();
    setDownFrame(pressed || selected());
    updateHoverTooltip();
  }

  void drawButton(const DrawArgs& args) {
    nvgSave(args.vg);
    nvgTranslate(args.vg, weirdOffset.x, weirdOffset.y);
    nvgScale(args.vg, xScale, yScale);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);
  }

  void drawText(const DrawArgs& args) {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }

    bool isSelected = selected();
    bool isInactive = inactive();
    NVGcolor labelColor = isInactive ? nvgRGB(0x78, 0x78, 0x78) : BLACK;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, outputLabel ? 18.f : 15.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    bool down = pressed || isSelected;
    float textX =
        box.size.x * 0.5f + weirdOffset.x + (down ? 3.6f * xScale : -1.3f);
    float textY =
        box.size.y * 0.52f + weirdOffset.y + (down ? 2.9f * yScale : -1.7f);

    nvgSave(args.vg);
    nvgFillColor(args.vg, labelColor);
    nvgText(args.vg, textX, textY, value.c_str(), NULL);
    nvgRestore(args.vg);
  }

  void draw(const DrawArgs& args) override {
    drawButton(args);
    drawText(args);
  }

  void runAction() {
    if (!module) {
      return;
    }
    if (outputLabel) {
      module->selectOutputView(outputIndex);
    } else {
      module->selectChannelView(channelIndex);
    }
  }

  void onButton(const event::Button& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
      ComputerscareBlankButton::onButton(e);
      return;
    }

    if (e.action == GLFW_PRESS) {
      setPressedState(true);
      e.consume(this);
      return;
    }

    if (e.action == GLFW_RELEASE) {
      setPressedState(false);
      runAction();
      e.consume(this);
      return;
    }

    ComputerscareBlankButton::onButton(e);
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

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      bool wasPressed = pressed;
      setPressedState(false);
      if (wasPressed) {
        runAction();
      }
      return;
    }
    ComputerscareBlankButton::onDragEnd(e);
  }
};

struct VolyPectorActionButton : ComputerscareBlankButton {
  ComputerscareVolyPector* module = NULL;
  ui::Tooltip* hoverTooltip = NULL;
  std::string label;
  bool initialize = false;
  bool wiggle = false;
  float xScale = 0.92f;
  float yScale = 1.18f;
  bool pressed = false;

  VolyPectorActionButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
    box.size = Vec(box.size.x * xScale, box.size.y * yScale);
  }

  void setXScale(float scale) {
    box.size.x *= scale / xScale;
    xScale = scale;
  }

  ~VolyPectorActionButton() { destroyHoverTooltip(); }

  std::string tooltipText() const {
    if (initialize) {
      return "Initialize all values";
    }
    return wiggle ? "Wiggle all values" : "Randomize all values";
  }

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

    hoverTooltip->text = tooltipText();
  }

  void destroyHoverTooltip() {
    if (!hoverTooltip) {
      return;
    }

    APP->scene->removeChild(hoverTooltip);
    delete hoverTooltip;
    hoverTooltip = NULL;
  }

  void setPressedFrame(bool isPressed) {
    if (pressed == isPressed) {
      return;
    }
    pressed = isPressed;
    if (frames.size() > 1) {
      sw->setSvg(frames[pressed ? 1 : 0]);
      fb->setDirty();
    }
  }

  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, xScale, yScale);
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
    nvgFillColor(args.vg, BLACK);
    float textXOffset = pressed ? 3.6f * xScale : -1.8f;
    float textYOffset = pressed ? 2.9f * yScale : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + textXOffset,
            box.size.y * 0.48f + textYOffset, label.c_str(), NULL);
  }

  void step() override {
    ComputerscareBlankButton::step();
    updateHoverTooltip();
  }

  void runAction() {
    if (!module) {
      return;
    }
    if (initialize) {
      module->initializeAllValues();
    } else if (wiggle) {
      module->randomizeAllValues(
          computerscare::volypector::RandomizeMode::WIGGLE);
    } else {
      module->randomizeAllValues(
          computerscare::volypector::RandomizeMode::REPLACE);
    }
  }

  void onButton(const event::Button& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
      ComputerscareBlankButton::onButton(e);
      return;
    }

    if (e.action == GLFW_PRESS) {
      setPressedFrame(true);
      e.consume(this);
      return;
    }

    if (e.action == GLFW_RELEASE) {
      setPressedFrame(false);
      runAction();
      e.consume(this);
      return;
    }

    ComputerscareBlankButton::onButton(e);
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

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      bool wasPressed = pressed;
      setPressedFrame(false);
      if (wasPressed) {
        runAction();
      }
      return;
    }
    ComputerscareBlankButton::onDragEnd(e);
  }
};

struct VolyPectorSelectorMenuItem : MenuItem {
  Module* module = NULL;
  int paramId = -1;
  int companionParamId = -1;
  int value = 0;

  void onAction(const event::Action& e) override {
    if (module && paramId >= 0) {
      module->params[paramId].setValue(value);
      if (value > 0 && companionParamId >= 0) {
        module->params[companionParamId].setValue(0.f);
      }
    }
  }

  void step() override {
    if (module && paramId >= 0) {
      rightText = CHECKMARK(
          (int)std::round(module->params[paramId].getValue()) == value);
    }
    MenuItem::step();
  }
};

struct VolyPectorSelectorButton : ComputerscareBlankButton {
  Module* module = NULL;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  std::vector<std::string> labels;
  std::string prefix;
  std::string menuTitle;
  int paramId = -1;
  int companionParamId = -1;
  int defaultValue = 0;
  int menuFrame = -1;
  float xScale = 1.85f;
  float yScale = 1.22f;

  VolyPectorSelectorButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
    box.size = Vec(box.size.x * xScale, box.size.y * yScale);
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

  int currentValue() const {
    if (module && paramId >= 0) {
      return math::clamp((int)std::round(module->params[paramId].getValue()), 0,
                         (int)labels.size() - 1);
    }
    return math::clamp(defaultValue, 0, (int)labels.size() - 1);
  }

  void step() override {
    ComputerscareBlankButton::step();
    updateMenuFrame();
  }

  void draw(const DrawArgs& args) override {
    updateMenuFrame();
    nvgSave(args.vg);
    nvgScale(args.vg, xScale, yScale);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);

    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }

    std::string text = prefix + labels[currentValue()];
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    bool pressed = isMenuOpen();
    float xOffset = pressed ? 1.5f : 0.f;
    float yOffset = pressed ? 1.5f : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + xOffset, box.size.y * 0.47f + yOffset,
            text.c_str(), NULL);
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
    if (!module) {
      return;
    }

    Menu* menu = createMenu();
    activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
    menu->addChild(createMenuLabel(menuTitle));
    for (int i = 0; i < (int)labels.size(); i++) {
      VolyPectorSelectorMenuItem* item =
          createMenuItem<VolyPectorSelectorMenuItem>(labels[i]);
      item->module = module;
      item->paramId = paramId;
      item->companionParamId = companionParamId;
      item->value = i;
      menu->addChild(item);
    }
    updateMenuFrame();
  }
};

struct ComputerscareVolyPectorWidget : ModuleWidget {
  bool previewMode = false;
  bool previewChannelView = false;
  int previewSelectedOutput = 0;
  int previewSelectedChannel = 0;
  int previewPolyChannels = 16;
  float previewKnobValues[volyPectorNumKnobs] = {};

  ComputerscareVolyPectorWidget(ComputerscareVolyPector* module) {
    setModule(module);
    if (!module) {
      previewMode = true;
      previewChannelView = random::uniform() < 0.5f;
      previewSelectedOutput =
          (int)std::floor(random::uniform() * volyPectorNumOutputs);
      previewSelectedChannel =
          (int)std::floor(random::uniform() * volyPectorNumKnobs);
      previewPolyChannels = 1 + (int)std::floor(random::uniform() * 16.f);
      for (int i = 0; i < volyPectorNumKnobs; i++) {
        previewKnobValues[i] = volyPectorRandomKnobPreviewValue();
      }
    }
    box.size = Vec(8 * 15, 380);

    struct Layout {
      Vec polyChannelsPos = Vec(28.f, 1.f);

      Vec mainScalePos = Vec(34.f, 36.f);
      Vec mainOffsetPos = Vec(7.f, 40.f);
      Vec focusLabelPos = Vec(4.f, 23.5f);

      Vec knobGridStart = Vec(4.2f, 68.f);
      Vec knobGridSpacing = Vec(25.5f, 27.9f);
      float knobGridSecondColumnYOffset = -5.f;
      Vec knobLabelOffset = Vec(-5.5f, 1.4f);
      Vec knobSecondColumnLabelOffset = Vec(18.f, 1.4f);
      float knobSecondColumnLabelWidth = 18.f;

      Vec outputRowsStart = Vec(56.f, 4.f);
      Vec outputRowsSpacing = Vec(0.f, 19.6f);
      Vec outputChannelButtonOffset = Vec(2.f, -4.f);
      Vec outputBandButtonOffset = Vec(22.f, -5.f);
      Vec outputJackOffset = Vec(44.f, -2.f);

      float bottomRandY = 316.f;
      float bottomWiggleY = 334.f;
      float bottomInitY = 352.f;
      float bottomAllJackX = 42.f;
      float bottomActionX = 4.f;
      float bottomRightActionX = 98.f;
      float bottomRightActionYOffset = 3.f;
      float bottomRightActionScale = 0.78f;
      float bottomChannelJackX = 61.f;
      float bottomBandJackX = 80.f;
      float bottomJackYOffset = 3.f;

      float randomizeProbabilityY = 290.f;
      float randomizeRangeY = 310.f;
      float randomizeCvX = 2.f;
      float randomizeKnobX = 18.f;
      float wiggleProbabilityY = 332.f;
      float wiggleRangeY = 352.f;
      float wiggleCvX = 2.f;
      float wiggleKnobX = 18.f;
    } layout;

    ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
    panel->box.size = box.size;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareVolyPectorPanel.svg")));
    addChild(panel);

    addChild(new PolyOutputChannelsWidget(
        layout.polyChannelsPos, module, ComputerscareVolyPector::POLY_CHANNELS,
        previewPolyChannels));
    VolyPectorNoRandomSmallKnob* scaleKnob =
        createParam<VolyPectorNoRandomSmallKnob>(
            layout.mainScalePos, module, ComputerscareVolyPector::GLOBAL_SCALE);
    scaleKnob->previewMode = previewMode;
    scaleKnob->previewValue = -2.f + random::uniform() * 4.f;
    addParam(scaleKnob);
    VolyPectorNoRandomMediumSmallKnob* offsetKnob =
        createParam<VolyPectorNoRandomMediumSmallKnob>(
            layout.mainOffsetPos, module,
            ComputerscareVolyPector::GLOBAL_OFFSET);
    offsetKnob->previewMode = previewMode;
    offsetKnob->previewValue = -10.f + random::uniform() * 20.f;
    addParam(offsetKnob);

    addInput(createInput<TinyJack>(
        Vec(layout.randomizeCvX,
            layout.randomizeProbabilityY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::RANDOMIZE_PROBABILITY_CV_INPUT));
    addParam(createParam<ScrambleKnob>(
        Vec(layout.randomizeKnobX, layout.randomizeProbabilityY), module,
        ComputerscareVolyPector::RANDOMIZE_PROBABILITY_CONTROL));
    addInput(createInput<TinyJack>(
        Vec(layout.randomizeCvX,
            layout.randomizeRangeY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::RANDOMIZE_RANGE_CV_INPUT));
    VolyPectorNoRandomSmallKnob* randomizeRangeKnob =
        createParam<VolyPectorNoRandomSmallKnob>(
            Vec(layout.randomizeKnobX, layout.randomizeRangeY), module,
            ComputerscareVolyPector::RANDOMIZE_RANGE_CONTROL);
    addParam(randomizeRangeKnob);
    addInput(createInput<TinyJack>(
        Vec(layout.wiggleCvX,
            layout.wiggleProbabilityY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::WIGGLE_PROBABILITY_CV_INPUT));
    addParam(createParam<ScrambleKnob>(
        Vec(layout.wiggleKnobX, layout.wiggleProbabilityY), module,
        ComputerscareVolyPector::WIGGLE_PROBABILITY_CONTROL));
    addInput(createInput<TinyJack>(
        Vec(layout.wiggleCvX, layout.wiggleRangeY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::WIGGLE_RANGE_CV_INPUT));
    VolyPectorNoRandomSmallKnob* wiggleRangeKnob =
        createParam<VolyPectorNoRandomSmallKnob>(
            Vec(layout.wiggleKnobX, layout.wiggleRangeY), module,
            ComputerscareVolyPector::WIGGLE_RANGE_CONTROL);
    addParam(wiggleRangeKnob);

    VolyPectorActionButton* randomizeAllButton =
        createWidget<VolyPectorActionButton>(
            Vec(layout.bottomRightActionX,
                layout.bottomRandY + layout.bottomRightActionYOffset));
    randomizeAllButton->module = module;
    randomizeAllButton->label = "RAND";
    randomizeAllButton->setXScale(layout.bottomRightActionScale);
    addChild(randomizeAllButton);
    VolyPectorActionButton* wiggleAllButton =
        createWidget<VolyPectorActionButton>(
            Vec(layout.bottomRightActionX,
                layout.bottomWiggleY + layout.bottomRightActionYOffset));
    wiggleAllButton->module = module;
    wiggleAllButton->label = "WIG";
    wiggleAllButton->wiggle = true;
    wiggleAllButton->setXScale(layout.bottomRightActionScale);
    addChild(wiggleAllButton);
    VolyPectorActionButton* initializeAllButton =
        createWidget<VolyPectorActionButton>(
            Vec(layout.bottomRightActionX,
                layout.bottomInitY + layout.bottomRightActionYOffset));
    initializeAllButton->module = module;
    initializeAllButton->label = "INIT";
    initializeAllButton->initialize = true;
    initializeAllButton->setXScale(layout.bottomRightActionScale);
    addChild(initializeAllButton);

    addInput(createInput<TinyJack>(
        Vec(layout.bottomAllJackX,
            layout.bottomRandY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::RANDOMIZE_ALL_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomChannelJackX,
            layout.bottomRandY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::CHANNEL_RANDOMIZE_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomBandJackX,
            layout.bottomRandY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::OUTPUT_RANDOMIZE_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomAllJackX,
            layout.bottomWiggleY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::WIGGLE_ALL_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomChannelJackX,
            layout.bottomWiggleY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::CHANNEL_WIGGLE_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomBandJackX,
            layout.bottomWiggleY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::OUTPUT_WIGGLE_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomAllJackX,
            layout.bottomInitY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::INITIALIZE_ALL_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomChannelJackX,
            layout.bottomInitY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::CHANNEL_INITIALIZE_INPUT));
    addInput(createInput<TinyJack>(
        Vec(layout.bottomBandJackX,
            layout.bottomInitY + layout.bottomJackYOffset),
        module, ComputerscareVolyPector::OUTPUT_INITIALIZE_INPUT));

    VolyPectorViewTitle* viewTitle = new VolyPectorViewTitle();
    viewTitle->module = module;
    viewTitle->previewMode = previewMode;
    viewTitle->previewChannelView = previewChannelView;
    viewTitle->previewSelectedOutput = previewSelectedOutput;
    viewTitle->previewSelectedChannel = previewSelectedChannel;
    viewTitle->box.pos = layout.focusLabelPos;
    viewTitle->box.size = Vec(52.f, 18.f);
    addChild(viewTitle);

    for (int i = 0; i < volyPectorNumKnobs; i++) {
      int column = i / 8;
      int row = i % 8;
      Vec pos =
          layout.knobGridStart +
          Vec(column * layout.knobGridSpacing.x,
              row * layout.knobGridSpacing.y +
                  (column == 1 ? layout.knobGridSecondColumnYOffset : 0.f));
      Vec labelOffset = column == 0 ? layout.knobLabelOffset
                                    : layout.knobSecondColumnLabelOffset;
      addLabeledKnob(pos.x, pos.y, module, i, labelOffset.x, labelOffset.y,
                     column == 1 ? layout.knobSecondColumnLabelWidth : 0.f);
    }

    for (int i = 0; i < volyPectorNumOutputs; i++) {
      Vec pos = layout.outputRowsStart + layout.outputRowsSpacing * i;
      addPortPair(volyPectorPortLabels[i], pos.x, pos.y, module, i,
                  ComputerscareVolyPector::OUTPUT + i,
                  layout.outputChannelButtonOffset,
                  layout.outputBandButtonOffset, layout.outputJackOffset);
    }
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareVolyPector* module =
        dynamic_cast<ComputerscareVolyPector*>(this->module);
    if (!module) return;

    struct MainKnobRangeItem : MenuItem {
      ComputerscareVolyPector* module;
      bool bipolar;
      void onAction(const event::Action& e) override {
        module->setMainKnobRange(bipolar);
      }
      void step() override {
        rightText = CHECKMARK(module->bipolarMainKnobs == bipolar);
        MenuItem::step();
      }
    };
    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Knob Range"));
    menu->addChild(construct<MainKnobRangeItem>(
        &MenuItem::text, "Unipolar", &MainKnobRangeItem::module, module,
        &MainKnobRangeItem::bipolar, false));
    menu->addChild(construct<MainKnobRangeItem>(
        &MenuItem::text, "Bipolar", &MainKnobRangeItem::module, module,
        &MainKnobRangeItem::bipolar, true));
  }

  void addSelector(Vec pos, ComputerscareVolyPector* module, int paramId,
                   std::string prefix, std::string menuTitle,
                   std::vector<std::string> labels, int defaultValue,
                   int companionParamId) {
    VolyPectorSelectorButton* button =
        createWidget<VolyPectorSelectorButton>(pos);
    button->module = module;
    button->paramId = paramId;
    button->companionParamId = companionParamId;
    button->prefix = prefix;
    button->menuTitle = menuTitle;
    button->labels = labels;
    button->defaultValue = defaultValue;
    addChild(button);
  }

  void addLabeledKnob(float x, float y, ComputerscareVolyPector* module,
                      int index, float labelDx, float labelDy,
                      float labelRightAlignWidth = 0.f) {
    VolyPectorKnobLabel* smallLetterDisplay = new VolyPectorKnobLabel();
    smallLetterDisplay->module = module;
    smallLetterDisplay->index = index;
    smallLetterDisplay->previewMode = previewMode;
    smallLetterDisplay->previewChannelView = previewChannelView;
    smallLetterDisplay->box.size = Vec(5, 10);
    smallLetterDisplay->fontSize = 16;
    smallLetterDisplay->letterSpacing = 1.6f;
    smallLetterDisplay->textAlign =
        labelRightAlignWidth > 0.f ? NVG_ALIGN_RIGHT : NVG_ALIGN_LEFT;
    if (labelRightAlignWidth > 0.f) {
      smallLetterDisplay->breakRowWidth = labelRightAlignWidth;
    }
    smallLetterDisplay->box.pos =
        Vec(x + labelDx - labelRightAlignWidth, y - 10 + labelDy);

    addChild(smallLetterDisplay);

    ParamWidget* pob = createParam<VolyPectorDisableableSmoothKnob>(
        Vec(x, y), module, ComputerscareVolyPector::KNOB + index);

    VolyPectorDisableableSmoothKnob* fader =
        dynamic_cast<VolyPectorDisableableSmoothKnob*>(pob);

    fader->module = module;
    fader->channel = index;
    fader->previewMode = previewMode;
    fader->previewChannelView = previewChannelView;
    fader->previewSelectedChannel = previewSelectedChannel;
    fader->previewPolyChannels = previewPolyChannels;
    fader->previewValue = previewKnobValues[index];
    addParam(fader);
  }

  void addPortPair(std::string label, float x, float y,
                   ComputerscareVolyPector* module, int outputIndex,
                   int outputId, Vec channelButtonOffset,
                   Vec outputButtonOffset, Vec outputJackOffset) {
    VolyPectorLabelButton* channelDisplay = new VolyPectorLabelButton();
    channelDisplay->module = module;
    channelDisplay->configure(outputIndex, false);
    channelDisplay->previewMode = previewMode;
    channelDisplay->previewChannelView = previewChannelView;
    channelDisplay->previewSelectedOutput = previewSelectedOutput;
    channelDisplay->previewSelectedChannel = previewSelectedChannel;
    channelDisplay->previewPolyChannels = previewPolyChannels;
    channelDisplay->box.pos = Vec(x, y) + channelButtonOffset;
    channelDisplay->value = std::to_string(outputIndex + 1);
    addChild(channelDisplay);

    VolyPectorLabelButton* outputDisplay = new VolyPectorLabelButton();
    outputDisplay->module = module;
    outputDisplay->configure(outputIndex, true);
    outputDisplay->previewMode = previewMode;
    outputDisplay->previewChannelView = previewChannelView;
    outputDisplay->previewSelectedOutput = previewSelectedOutput;
    outputDisplay->previewSelectedChannel = previewSelectedChannel;
    outputDisplay->previewPolyChannels = previewPolyChannels;
    outputDisplay->box.pos = Vec(x, y) + outputButtonOffset;
    outputDisplay->value = label;
    addChild(outputDisplay);

    addOutput(
        createOutput<TinyJack>(Vec(x, y) + outputJackOffset, module, outputId));
  }
};

Model* modelComputerscareVolyPector =
    createModel<ComputerscareVolyPector, ComputerscareVolyPectorWidget>(
        "computerscare-volypector");
