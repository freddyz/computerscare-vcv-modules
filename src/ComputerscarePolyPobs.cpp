#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

namespace {

const int polyPobsNumKnobs = 16;
const int polyPobsNumInputs = 16;
const int polyPobsNumOutputs = 16;

const std::vector<std::string> polyPobsPortLabels = {
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P"};

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

float polyPobsIndexJitter(int index, int salt, float amount) {
  int value = (index * 37 + salt * 17) % 7;
  return (value - 3) * amount;
}

}  // namespace

struct ComputerscarePolyPobs : ComputerscarePolyModule {
  bool bipolarMainKnobs = false;
  int mainKnobRangeRevision = 0;
  float outputKnobValues[polyPobsNumOutputs][polyPobsNumKnobs] = {};
  float outputScaleValues[polyPobsNumOutputs] = {};
  float outputOffsetValues[polyPobsNumOutputs] = {};
  float channelScaleValues[polyPobsNumKnobs] = {};
  float channelOffsetValues[polyPobsNumKnobs] = {};
  int viewedOutput = 0;
  int viewedChannel = -1;
  bool loadingView = false;

  enum ParamIds {
    KNOB,
    POLY_CHANNELS = KNOB + polyPobsNumKnobs,
    GLOBAL_SCALE,
    GLOBAL_OFFSET,
    CHANNEL_SELECTOR,
    OUTPUT_SELECTOR,
    INPUT_SELECTOR,
    NUM_PARAMS
  };
  enum InputIds { INPUT, NUM_INPUTS = INPUT + polyPobsNumInputs };
  enum OutputIds { OUTPUT, NUM_OUTPUTS = OUTPUT + polyPobsNumOutputs };
  enum LightIds { NUM_LIGHTS };

  ComputerscarePolyPobs() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    for (int i = 0; i < polyPobsNumKnobs; i++) {
      configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
    }
    configSwitch(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels",
                 polyChannelLabels(false));
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");
    configSwitch(CHANNEL_SELECTOR, 0.f, 16.f, 0.f, "Channel",
                 withAllLabel(oneToSixteenLabels()));
    configSwitch(OUTPUT_SELECTOR, 0.f, polyPobsNumOutputs, 1.f, "Output",
                 withAllLabel(polyPobsPortLabels));
    configSwitch(INPUT_SELECTOR, 0.f, polyPobsNumInputs - 1, 0.f, "Input",
                 polyPobsPortLabels);

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

    for (int i = 0; i < polyPobsNumInputs; i++) {
      configInput(INPUT + i, polyPobsPortLabels[i]);
    }
    for (int i = 0; i < polyPobsNumOutputs; i++) {
      configOutput(OUTPUT + i, polyPobsPortLabels[i]);
    }
    for (int i = 0; i < polyPobsNumOutputs; i++) {
      outputScaleValues[i] = 1.f;
      outputOffsetValues[i] = 0.f;
    }
    for (int i = 0; i < polyPobsNumKnobs; i++) {
      channelScaleValues[i] = 1.f;
      channelOffsetValues[i] = 0.f;
    }

    loadCurrentView();
    loadCurrentControls();
    updateParamLabels();
  }

  int selectedOutput() {
    int value = math::clamp((int)std::round(params[OUTPUT_SELECTOR].getValue()),
                            0, polyPobsNumOutputs);
    return value - 1;
  }

  int selectedChannel() {
    int value =
        math::clamp((int)std::round(params[CHANNEL_SELECTOR].getValue()), 0,
                    polyPobsNumKnobs);
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

  void storeCurrentView() {
    if (channelViewActive()) {
      int channel = selectedChannel();
      for (int output = 0; output < polyPobsNumOutputs; output++) {
        outputKnobValues[output][channel] = params[KNOB + output].getValue();
      }
      return;
    }

    int output = normalizedOutputView();
    for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
      outputKnobValues[output][channel] = params[KNOB + channel].getValue();
    }
  }

  void storeViewedView() {
    if (viewedChannel >= 0) {
      for (int output = 0; output < polyPobsNumOutputs; output++) {
        outputKnobValues[output][viewedChannel] =
            params[KNOB + output].getValue();
      }
    } else {
      for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
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
      for (int knob = 0; knob < polyPobsNumKnobs; knob++) {
        params[KNOB + knob].setValue(
            clampKnobValue(outputKnobValues[knob][channel]));
      }
    } else {
      for (int knob = 0; knob < polyPobsNumKnobs; knob++) {
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
      updateParamLabels();
      return;
    }

    if (!loadingView) {
      storeViewedView();
      storeViewedControls();
    }
  }

  void selectOutputView(int output) {
    storeViewedView();
    storeViewedControls();
    params[CHANNEL_SELECTOR].setValue(0.f);
    params[OUTPUT_SELECTOR].setValue(
        math::clamp(output, 0, polyPobsNumOutputs - 1) + 1);
    loadCurrentView();
    loadCurrentControls();
    updateParamLabels();
  }

  void selectChannelView(int channel) {
    storeViewedView();
    storeViewedControls();
    params[OUTPUT_SELECTOR].setValue(0.f);
    params[CHANNEL_SELECTOR].setValue(
        math::clamp(channel, 0, polyPobsNumKnobs - 1) + 1);
    loadCurrentView();
    loadCurrentControls();
    updateParamLabels();
  }

  float randomKnobValue() {
    float minValue = bipolarMainKnobs ? -10.f : 0.f;
    return minValue + random::uniform() * (10.f - minValue);
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
      for (int output = 0; output < polyPobsNumOutputs; output++) {
        outputKnobValues[output][channel] = 0.f;
      }
    } else {
      int output = normalizedOutputView();
      for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
        outputKnobValues[output][channel] = 0.f;
      }
    }
    loadCurrentView();
    updateParamLabels();
  }

  void randomizeCurrentView() {
    float outputSelection = params[OUTPUT_SELECTOR].getValue();
    float channelSelection = params[CHANNEL_SELECTOR].getValue();
    restoreViewSelection(outputSelection, channelSelection);
    if (channelViewActive()) {
      int channel = selectedChannel();
      for (int output = 0; output < polyPobsNumOutputs; output++) {
        outputKnobValues[output][channel] = randomKnobValue();
      }
    } else {
      int output = normalizedOutputView();
      for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
        outputKnobValues[output][channel] = randomKnobValue();
      }
    }
    loadCurrentView();
    updateParamLabels();
  }

  void randomizeAllValues() {
    for (int output = 0; output < polyPobsNumOutputs; output++) {
      for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
        outputKnobValues[output][channel] = randomKnobValue();
      }
    }
    loadCurrentView();
    updateParamLabels();
  }

  void initializeAllValues() {
    for (int output = 0; output < polyPobsNumOutputs; output++) {
      for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
        outputKnobValues[output][channel] = 0.f;
      }
      outputScaleValues[output] = 1.f;
      outputOffsetValues[output] = 0.f;
    }
    for (int channel = 0; channel < polyPobsNumKnobs; channel++) {
      channelScaleValues[channel] = 1.f;
      channelOffsetValues[channel] = 0.f;
    }
    loadCurrentView();
    loadCurrentControls();
    updateParamLabels();
  }

  void onRandomize() override { randomizeCurrentView(); }

  void onReset() override { resetCurrentView(); }

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

  std::string outputName(int output) {
    return "Output " +
           polyPobsPortLabels[math::clamp(output, 0, polyPobsNumOutputs - 1)];
  }

  std::string channelName(int channel) {
    return "Ch " +
           std::to_string(math::clamp(channel, 0, polyPobsNumKnobs - 1) + 1);
  }

  void updateParamLabels() {
    int channel = selectedChannel();
    int output = normalizedOutputView();
    for (int knob = 0; knob < polyPobsNumKnobs; knob++) {
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
    for (int i = 0; i < polyPobsNumKnobs; i++) {
      engine::ParamQuantity* pq = getParamQuantity(KNOB + i);
      if (!pq) continue;
      pq->minValue = bipolar ? -10.f : 0.f;
      pq->maxValue = 10.f;
      pq->defaultValue = 0.f;
      pq->setValue(math::clamp(pq->getValue(), pq->minValue, pq->maxValue));
    }
    for (int output = 0; output < polyPobsNumOutputs; output++) {
      for (int knob = 0; knob < polyPobsNumKnobs; knob++) {
        outputKnobValues[output][knob] = math::clamp(
            outputKnobValues[output][knob], bipolar ? -10.f : 0.f, 10.f);
      }
    }
    if (changed) {
      mainKnobRangeRevision++;
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
    for (int output = 0; output < polyPobsNumOutputs; output++) {
      json_t* outputJ = json_array();
      for (int knob = 0; knob < polyPobsNumKnobs; knob++) {
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
    for (int i = 0; i < polyPobsNumOutputs; i++) {
      json_array_append_new(outputScaleValuesJ,
                            json_real(outputScaleValues[i]));
      json_array_append_new(outputOffsetValuesJ,
                            json_real(outputOffsetValues[i]));
    }
    for (int i = 0; i < polyPobsNumKnobs; i++) {
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
  }

  void dataFromJson(json_t* rootJ) override {
    setMainKnobRange(readBipolarMainKnobs(rootJ));
    json_t* outputScaleValuesJ = json_object_get(rootJ, "outputScaleValues");
    json_t* outputOffsetValuesJ = json_object_get(rootJ, "outputOffsetValues");
    json_t* channelScaleValuesJ = json_object_get(rootJ, "channelScaleValues");
    json_t* channelOffsetValuesJ =
        json_object_get(rootJ, "channelOffsetValues");
    if (outputScaleValuesJ) {
      for (int i = 0; i < polyPobsNumOutputs; i++) {
        json_t* valueJ = json_array_get(outputScaleValuesJ, i);
        if (valueJ) {
          outputScaleValues[i] = json_number_value(valueJ);
        }
      }
    }
    if (outputOffsetValuesJ) {
      for (int i = 0; i < polyPobsNumOutputs; i++) {
        json_t* valueJ = json_array_get(outputOffsetValuesJ, i);
        if (valueJ) {
          outputOffsetValues[i] = json_number_value(valueJ);
        }
      }
    }
    if (channelScaleValuesJ) {
      for (int i = 0; i < polyPobsNumKnobs; i++) {
        json_t* valueJ = json_array_get(channelScaleValuesJ, i);
        if (valueJ) {
          channelScaleValues[i] = json_number_value(valueJ);
        }
      }
    }
    if (channelOffsetValuesJ) {
      for (int i = 0; i < polyPobsNumKnobs; i++) {
        json_t* valueJ = json_array_get(channelOffsetValuesJ, i);
        if (valueJ) {
          channelOffsetValues[i] = json_number_value(valueJ);
        }
      }
    }
    json_t* outputKnobValuesJ = json_object_get(rootJ, "outputKnobValues");
    if (outputKnobValuesJ) {
      for (int output = 0; output < polyPobsNumOutputs; output++) {
        json_t* outputJ = json_array_get(outputKnobValuesJ, output);
        if (!outputJ) continue;
        for (int knob = 0; knob < polyPobsNumKnobs; knob++) {
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
    } else {
      loadCurrentView();
      loadCurrentControls();
      updateParamLabels();
      storeViewedView();
      storeViewedControls();
    }
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();
    syncView();
    for (int output = 0; output < polyPobsNumOutputs; output++) {
      outputs[OUTPUT + output].setChannels(polyChannels);
      for (int channel = 0; channel < polyChannels; channel++) {
        outputs[OUTPUT + output].setVoltage(scaledOutputValue(output, channel),
                                            channel);
      }
    }
  }

  void checkPoly() override {
    polyChannels = params[POLY_CHANNELS].getValue();
    if (polyChannels == 0) {
      polyChannels = 16;
      params[POLY_CHANNELS].setValue(16);
    }
  }
};

struct PolyPobsNoRandomSmallKnob : SmallKnob {
  PolyPobsNoRandomSmallKnob() { SmallKnob(); }
};

struct PolyPobsNoRandomMediumSmallKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));

  PolyPobsNoRandomMediumSmallKnob() {
    setSvg(enabledSvg);
    ComputerscareRoundKnob();
  }
};

struct PolyPobsDisableableSmoothKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  int mainKnobRangeRevision = -1;
  ComputerscarePolyModule* module = NULL;

  PolyPobsDisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }

  void step() override {
    if (module) {
      ComputerscarePolyPobs* pobs =
          dynamic_cast<ComputerscarePolyPobs*>(module);
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
    }
    ComputerscareRoundKnob::step();
  }
};

struct PolyPobsKnobLabel : SmallLetterDisplay {
  ComputerscarePolyPobs* module = NULL;
  int index = 0;
  bool previousChannelView = false;

  void draw(const DrawArgs& args) override {
    bool channelView = module && module->channelViewActive();
    if (channelView != previousChannelView || value.empty()) {
      value =
          channelView ? polyPobsPortLabels[index] : std::to_string(index + 1);
      previousChannelView = channelView;
    }
    SmallLetterDisplay::draw(args);
  }
};

struct PolyPobsLabelButton : ComputerscareBlankButton {
  ComputerscarePolyPobs* module = NULL;
  std::string value;
  int outputIndex = 0;
  int channelIndex = 0;
  bool outputLabel = false;
  bool pressed = false;
  bool latchedDown = false;
  float xScale = 0.66f;
  float yScale = 1.34f;
  Vec weirdOffset = Vec(0.f, 0.f);

  PolyPobsLabelButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
  }

  void configure(int index, bool isOutputLabel) {
    outputIndex = index;
    channelIndex = index;
    outputLabel = isOutputLabel;
    xScale = (outputLabel ? 0.7f : 0.62f) +
             polyPobsIndexJitter(index, outputLabel ? 2 : 5, 0.01f);
    yScale = (outputLabel ? 1.38f : 1.32f) +
             polyPobsIndexJitter(index, outputLabel ? 7 : 11, 0.014f);
    weirdOffset = Vec(polyPobsIndexJitter(index, outputLabel ? 13 : 19, 0.42f),
                      polyPobsIndexJitter(index, outputLabel ? 23 : 29, 0.34f));
    box.size.x *= xScale;
    box.size.y *= yScale;
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
    if (!module) {
      return false;
    }
    if (outputLabel) {
      return module->selectedOutput() == outputIndex &&
             !module->channelViewActive();
    }
    return module->selectedChannel() == channelIndex;
  }

  bool inactive() {
    return !outputLabel && module && channelIndex > module->polyChannels - 1;
  }

  void step() override {
    ComputerscareBlankButton::step();
    setDownFrame(pressed || selected());
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
        box.size.x * 0.5f + weirdOffset.x + (down ? 3.6f * xScale : 0.f);
    float textY =
        box.size.y * 0.52f + weirdOffset.y + (down ? 2.9f * yScale : 0.f);

    nvgSave(args.vg);
    if (isSelected) {
      nvgFillColor(args.vg, COLOR_COMPUTERSCARE_YELLOW);
      for (float dx = -1.8f; dx <= 1.8f; dx += 1.8f) {
        for (float dy = -1.8f; dy <= 1.8f; dy += 1.8f) {
          if (dx == 0.f && dy == 0.f) continue;
          nvgText(args.vg, textX + dx, textY + dy, value.c_str(), NULL);
        }
      }
      nvgFillColor(args.vg, BLACK);
      for (float dx = -0.8f; dx <= 0.8f; dx += 0.8f) {
        for (float dy = -0.8f; dy <= 0.8f; dy += 0.8f) {
          if (dx == 0.f && dy == 0.f) continue;
          nvgText(args.vg, textX + dx, textY + dy, value.c_str(), NULL);
        }
      }
      nvgFillColor(args.vg,
                   isInactive ? labelColor : COLOR_COMPUTERSCARE_YELLOW);
    } else {
      nvgFillColor(args.vg, labelColor);
    }
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

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      bool wasPressed = pressed;
      setPressedState(false);
      if (wasPressed) {
        runAction();
      }
    }
    ComputerscareBlankButton::onDragEnd(e);
  }
};

struct PolyPobsActionButton : ComputerscareBlankButton {
  ComputerscarePolyPobs* module = NULL;
  std::string label;
  bool initialize = false;
  float xScale = 1.62f;
  float yScale = 1.18f;
  bool pressed = false;

  PolyPobsActionButton() {
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
    box.size = Vec(box.size.x * xScale, box.size.y * yScale);
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
    float textXOffset = pressed ? 3.6f * xScale : 0.f;
    float textYOffset = pressed ? 2.9f * yScale : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + textXOffset,
            box.size.y * 0.48f + textYOffset, label.c_str(), NULL);
  }

  void runAction() {
    if (!module) {
      return;
    }
    if (initialize) {
      module->initializeAllValues();
    } else {
      module->randomizeAllValues();
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

  void onDragEnd(const event::DragEnd& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      bool wasPressed = pressed;
      setPressedFrame(false);
      if (wasPressed) {
        runAction();
      }
    }
    ComputerscareBlankButton::onDragEnd(e);
  }
};

struct PolyPobsSelectorMenuItem : MenuItem {
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

struct PolyPobsSelectorButton : ComputerscareBlankButton {
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

  PolyPobsSelectorButton() {
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
      PolyPobsSelectorMenuItem* item =
          createMenuItem<PolyPobsSelectorMenuItem>(labels[i]);
      item->module = module;
      item->paramId = paramId;
      item->companionParamId = companionParamId;
      item->value = i;
      menu->addChild(item);
    }
    updateMenuFrame();
  }
};

struct PolyPobsOutputBackground : TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 4.f);
    nvgFillColor(args.vg, nvgRGBA(0x42, 0x42, 0x42, 0x9c));
    nvgFill(args.vg);
  }
};

struct ComputerscarePolyPobsWidget : ModuleWidget {
  ComputerscarePolyPobsWidget(ComputerscarePolyPobs* module) {
    setModule(module);
    box.size = Vec(8 * 15, 380);

    ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
    panel->box.size = box.size;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscarePolyPobsPanel.svg")));
    addChild(panel);

    addChild(new PolyOutputChannelsWidget(
        Vec(91.f, 1.f), module, ComputerscarePolyPobs::POLY_CHANNELS));
    addParam(createParam<PolyPobsNoRandomSmallKnob>(
        Vec(13, 75), module, ComputerscarePolyPobs::GLOBAL_SCALE));
    addParam(createParam<PolyPobsNoRandomMediumSmallKnob>(
        Vec(35, 78), module, ComputerscarePolyPobs::GLOBAL_OFFSET));
    PolyPobsActionButton* randomizeAllButton =
        createWidget<PolyPobsActionButton>(Vec(4.f, 28.f));
    randomizeAllButton->module = module;
    randomizeAllButton->label = "RAND ALL";
    addChild(randomizeAllButton);
    PolyPobsActionButton* initializeAllButton =
        createWidget<PolyPobsActionButton>(Vec(4.f, 50.f));
    initializeAllButton->module = module;
    initializeAllButton->label = "INIT ALL";
    initializeAllButton->initialize = true;
    addChild(initializeAllButton);

    PolyPobsOutputBackground* outputBackground =
        createWidget<PolyPobsOutputBackground>(Vec(84.f, 25.f));
    outputBackground->box.size = Vec(34.f, 350.f);
    addChild(outputBackground);

    for (int i = 0; i < polyPobsNumKnobs; i++) {
      float x = 1.4f + 24.3f * (i - i % 8) / 8.f;
      float y = 108.f + 30.f * (i % 8) + 14.3f / 8.f * (i - i % 8);
      addLabeledKnob(x, y, module, i, i >= 8 ? 12.f : -3.f, 2.f);
    }

    for (int i = 0; i < polyPobsNumInputs; i++) {
      addPortPair(polyPobsPortLabels[i], 58.f, 31.f + i * 21.5f, module, i,
                  ComputerscarePolyPobs::OUTPUT + i);
    }
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscarePolyPobs* module =
        dynamic_cast<ComputerscarePolyPobs*>(this->module);
    if (!module) return;

    struct MainKnobRangeItem : MenuItem {
      ComputerscarePolyPobs* module;
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
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Main Knob Range"));
    menu->addChild(construct<MainKnobRangeItem>(
        &MenuItem::text, "Unipolar", &MainKnobRangeItem::module, module,
        &MainKnobRangeItem::bipolar, false));
    menu->addChild(construct<MainKnobRangeItem>(
        &MenuItem::text, "Bipolar", &MainKnobRangeItem::module, module,
        &MainKnobRangeItem::bipolar, true));
  }

  void addSelector(Vec pos, ComputerscarePolyPobs* module, int paramId,
                   std::string prefix, std::string menuTitle,
                   std::vector<std::string> labels, int defaultValue,
                   int companionParamId) {
    PolyPobsSelectorButton* button = createWidget<PolyPobsSelectorButton>(pos);
    button->module = module;
    button->paramId = paramId;
    button->companionParamId = companionParamId;
    button->prefix = prefix;
    button->menuTitle = menuTitle;
    button->labels = labels;
    button->defaultValue = defaultValue;
    addChild(button);
  }

  void addLabeledKnob(float x, float y, ComputerscarePolyPobs* module,
                      int index, float labelDx, float labelDy) {
    PolyPobsKnobLabel* smallLetterDisplay = new PolyPobsKnobLabel();
    smallLetterDisplay->module = module;
    smallLetterDisplay->index = index;
    smallLetterDisplay->box.size = Vec(5, 10);
    smallLetterDisplay->fontSize = 18;
    smallLetterDisplay->textAlign = 1;

    ParamWidget* pob = createParam<PolyPobsDisableableSmoothKnob>(
        Vec(x, y), module, ComputerscarePolyPobs::KNOB + index);

    PolyPobsDisableableSmoothKnob* fader =
        dynamic_cast<PolyPobsDisableableSmoothKnob*>(pob);

    fader->module = module;
    fader->channel = index;

    addParam(fader);

    smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);

    addChild(smallLetterDisplay);
  }

  void addPortPair(std::string label, float x, float y,
                   ComputerscarePolyPobs* module, int outputIndex,
                   int outputId) {
    PolyPobsLabelButton* channelDisplay = new PolyPobsLabelButton();
    channelDisplay->module = module;
    channelDisplay->configure(outputIndex, false);
    channelDisplay->box.pos = Vec(x + 4.f, y - 7.f);
    channelDisplay->value = std::to_string(outputIndex + 1);
    addChild(channelDisplay);

    PolyPobsLabelButton* outputDisplay = new PolyPobsLabelButton();
    outputDisplay->module = module;
    outputDisplay->configure(outputIndex, true);
    outputDisplay->box.pos = Vec(x + 21.f, y - 8.f);
    outputDisplay->value = label;
    addChild(outputDisplay);

    addOutput(createOutput<TinyJack>(Vec(x + 39.f, y), module, outputId));
  }
};

Model* modelComputerscarePolyPobs =
    createModel<ComputerscarePolyPobs, ComputerscarePolyPobsWidget>(
        "computerscare-polypobs");
