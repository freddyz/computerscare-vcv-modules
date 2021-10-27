#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

struct ComputerscarePultiMoly;

const int numKnobs = 16;
const int numToggles = 16;

const int numInputs = 16;
const int numOutputs = 16;

const int numInputCities = 16;

const int numKnobBlocks = 2;

const int numPolyChannels = 16;


struct ComputerscarePultiMoly : ComputerscarePolyModule {
  ComputerscareSVGPanel* panelRef;


  /*
    //matrix multiplication
    C = T * I + O

    //would require 16^4 = 65536 float values, taking up about 3MB

    city[z][ch] = inputTrim[i][ch]*inputs[i][ch]+offset[i][ch] + globalChannelOffset[ch]

    city[z][ch] = matrix[z]

  */

  float cityValues[numOutputs][numKnobs] = {};
  float cityScaleValues[numOutputs] = {};
  float cityOffsetValues[numOutputs] = {};

  int lastEditKnobs[numKnobBlocks] = {};


  enum ParamIds {
    TOGGLES,
    POLY_CHANNELS = TOGGLES + numToggles,
    GLOBAL_SCALE,
    GLOBAL_OFFSET,
    EDITING_CITY,
    EDITING_INPUT,
    EDITING_FOCUS,
    EDIT_KNOB = EDITING_FOCUS + numKnobBlocks,
    CITY_SCALE = EDIT_KNOB + numKnobBlocks * numKnobs,
    CITY_OFFSET = CITY_SCALE + numKnobBlocks,
    NUM_PARAMS = CITY_OFFSET + numKnobBlocks

  };
  enum InputIds {
    CITY_INPUT,
    NUM_INPUTS = CITY_INPUT + numInputs
  };
  enum OutputIds {
    CITY_OUTPUT,
    NUM_OUTPUTS = CITY_OUTPUT + numOutputs
  };
  enum LightIds {
    NUM_LIGHTS
  };


  ComputerscarePultiMoly()  {

    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    initStorage();

    for (int i = 0; i < numKnobs * numKnobBlocks; i++) {
      configParam(EDIT_KNOB + i, 0.f, 10.f, 0.f, "Edit Block " + std::to_string(((i - i % numKnobs)) / numKnobs + 1) + ", ch " + std::to_string((i % numKnobs) + 1));
    }
    for (int i = 0; i < numKnobBlocks; i++) {
      configParam(EDITING_FOCUS + i, 1.f, 16.f, 1.f, "Editing ");
      configParam(CITY_SCALE + i, -2.f, 2.f, 1.f, "Block " + std::to_string(i + 1) + " scale");
      configParam(CITY_OFFSET + i, -10.f, 10.f, 0.f, "Block " + std::to_string(i + 1) + " offset", " volts");

      getParamQuantity(EDITING_FOCUS + i)->randomizeEnabled = false;
      getParamQuantity(CITY_SCALE + i)->randomizeEnabled = false;
      getParamQuantity(CITY_OFFSET + i)->randomizeEnabled = false;

      lastEditKnobs[i] = 0;

    }
    for (int i = 0; i < numInputCities; i++) {
      configInput(CITY_INPUT + i, "City " + std::to_string(i + 1) + " Input");
      configOutput(CITY_OUTPUT + i, "City " + std::to_string(i + 1) + " Output");


    }
    configParam(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels");
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");

    configParam(EDITING_CITY, 1.f, 16.f, 1.f, "Editing City");

    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;

  }
  void initStorage() {
    for (int i = 0; i < numOutputs; i++) {
      cityScaleValues[i] = 1.f;
      cityOffsetValues[i] = 0.f;
      for (int ch = 0; ch < numPolyChannels; ch++) {
        cityValues[i][ch] = 0.f;
      }
    }
  }
  void processCityOutput(int index) {
    float trim = cityScaleValues[index];
    float offset = cityOffsetValues[index];
    for (int i = 0; i < polyChannels; i++) {
      outputs[CITY_OUTPUT + index].setVoltage(trim * cityValues[index][i] + offset, i);
    }
  }
  void process(const ProcessArgs &args) override {
    ComputerscarePolyModule::checkCounter();

  }
  void checkPoly() override {
    polyChannels = params[POLY_CHANNELS].getValue();

    if (polyChannels == 0) {
      polyChannels = 16;
      params[POLY_CHANNELS].setValue(16);
    }
    for (int i = 0; i < numOutputs; i++) {
      outputs[CITY_OUTPUT + i].setChannels(polyChannels);
    }
    checkKnobs();
  }
  float calculateValue(int city, int ch) {
    // float scale = params[CITY_SCALE + city].getValue();
    //float offset = params[CITY_OFFSET + city].getValue();
    // return scale * params[EDIT_KNOB + city * numKnobs + ch].getValue() + offset;
    return params[EDIT_KNOB + city * numKnobs + ch].getValue();
  }
  void checkKnobs() {
    for (int city = 0; city < numKnobBlocks; city++) {
      int editKnobValue = params[EDITING_FOCUS + city].getValue();
      if (lastEditKnobs[city] != editKnobValue) {
        lastEditKnobs[city] = editKnobValue;
        setEditKnobsFromCityValues(city, editKnobValue - 1);
      }
      cityScaleValues[lastEditKnobs[city] - 1] = params[CITY_SCALE + city].getValue();
      cityOffsetValues[lastEditKnobs[city] - 1] = params[CITY_OFFSET + city].getValue();

      //float offset = params[CITY_OFFSET + city].getValue();
      for (int ch = 0; ch < numKnobs; ch++) {
        cityValues[lastEditKnobs[city] - 1][ch] = calculateValue(city, ch);
      }
    }
    for (int outputCity = 0; outputCity < numOutputs; outputCity++) {
      processCityOutput(outputCity);
    }
  }
  void setEditKnobsFromCityValues(int block, int city) {
    params[CITY_SCALE + block].setValue(cityScaleValues[lastEditKnobs[block] - 1]);
    params[CITY_OFFSET + block].setValue(cityOffsetValues[lastEditKnobs[block] - 1]);

    for (int i = 0; i < numKnobs; i++) {
      params[EDIT_KNOB + block * numKnobs + i].setValue(cityValues[city][i]);
    }
  }
  void dataFromJson(json_t *rootJ) override {

    json_t *cityValuesJ = json_object_get(rootJ, "cityValues");
    json_t *cityOffsetsJ = json_object_get(rootJ, "cityOffsets");
    json_t *cityScalesJ = json_object_get(rootJ, "cityScales");

    //json_real_value


    if (cityOffsetsJ)
    {
      for (int city = 0; city < numOutputs; city++) {
        json_t *cityOffsetJ = json_array_get(cityOffsetsJ, city);
        if (cityOffsetJ) {
          cityOffsetValues[city] = json_real_value(cityOffsetJ);
        }

        json_t *cityScaleJ = json_array_get(cityScalesJ, city);
        if (cityScaleJ) {
          cityScaleValues[city] = json_real_value(cityScaleJ);
        }

        for (int ch = 0; ch < numPolyChannels; ch++) {
          json_t *cityValueJ = json_array_get(cityValuesJ, city * numPolyChannels + ch);
          if (cityValueJ) {
            cityValues[city][ch] = json_real_value(cityValueJ);
          }
        }
      }
    }
  }
  json_t *dataToJson() override {
    json_t *rootJ = json_object();
    json_t *cityValuesJ = json_array();
    json_t *cityOffsetsJ = json_array();
    json_t *cityScalesJ = json_array();
    for (int k = 0; k < numOutputs; k++) {
      json_t *coffset_j = json_real(cityOffsetValues[k]);
      json_array_append_new(cityOffsetsJ, coffset_j);

      json_t *cscale_j = json_real(cityScaleValues[k]);
      json_array_append_new(cityScalesJ, cscale_j);

      for (int i = 0; i < numKnobs; i++)
      {
        json_t *cv_j = json_real(cityValues[k][i]);
        json_array_append_new(cityValuesJ, cv_j);
      }
    }
    json_object_set_new(rootJ, "cityValues", cityValuesJ);
    json_object_set_new(rootJ, "cityOffsets", cityOffsetsJ);
    json_object_set_new(rootJ, "cityScales", cityScalesJ);
    return rootJ;
  }
};


struct NoRandomSmallKnob : SmallKnob {
  NoRandomSmallKnob() {
    SmallKnob();
  };
};
struct NoRandomMediumSmallKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));

  NoRandomMediumSmallKnob() {
    setSvg(enabledSvg);
    RoundKnob();
  };
};

struct DisableableSmoothKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  ComputerscarePolyModule *module;

  DisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }
  void step() override {
    if (module) {
      bool candidate = channel > module->polyChannels - 1;
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        onChange(*(new event::Change()));
        fb->dirty = true;
        disabled = candidate;
      }
    }
    else {
    }
    RoundKnob::step();
  }
};
struct DisableableSnapSmoothKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-dot-indicator.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  ComputerscarePolyModule *module;

  DisableableSnapSmoothKnob() {
    setSvg(enabledSvg);
    snap = true;
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }
  void step() override {
    if (module) {
      bool candidate = channel > module->polyChannels - 1;
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        onChange(*(new event::Change()));
        fb->dirty = true;
        disabled = candidate;
      }
    }
    else {
    }
    RoundKnob::step();
  }
};

struct ComputerscarePultiMolyWidget : ModuleWidget {
  ComputerscarePultiMolyWidget(ComputerscarePultiMoly *module) {
    setModule(module);
    //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePultiMolyPanel.svg")));
    box.size = Vec((2 + numKnobBlocks) * 4 * 15, 380);
    {
      ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePultiMolyPanel.svg")));
      addChild(panel);
    }



    channelWidget = new PolyOutputChannelsWidget(Vec(1, 24), module, ComputerscarePultiMoly::POLY_CHANNELS);

    addChild(channelWidget);

    addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module, ComputerscarePultiMoly::GLOBAL_SCALE));
    addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscarePultiMoly::GLOBAL_OFFSET));

    //addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscarePultiMoly::EDITING_CITY));



    float xEditingFocus;
    float yEditingFocus;

    float xx;
    float yy;

    float inputXStart = 0;
    float inputXSpacing = 50;
    float inputYStart = 20;

    float yInitial = 86;
    float ySpacing =  34;

    float yRightColumnOffset = 14.3 / 8;

    float xInitial = 10 + inputXSpacing;
    float citySpacing = 60;

    float outputXStart = inputXStart + inputXSpacing + numKnobBlocks * citySpacing + 20;
    float outputYStart = 20;




    for (int cityInput = 0; cityInput < numInputCities; cityInput++) {
      xx = inputXStart + 1.4f + 24.3 * (cityInput - cityInput % 8) / 8;
      yy = inputYStart + ySpacing * (cityInput % 8) +  yRightColumnOffset * (cityInput - cityInput % 8) ;
      addInput(createInput<InPort>(Vec(xx, yy), module, ComputerscarePultiMoly::CITY_INPUT + cityInput));
    }

    for (int block = 0; block < numKnobBlocks; block++) {
      xx = xInitial + block * citySpacing + 1.4f ;
      yy = yInitial - 33;
      addLabeledKnob("Edit", xx + 15, yy - 25, module, ComputerscarePultiMoly::EDITING_FOCUS + block , 0, 0, block, 1);
      //addParam(createParam<NoRandomMediumSmallKnob>(Vec(xx , yy + 20 ), module, ComputerscarePultiMoly::EDITING_FOCUS + block));

      addParam(createParam<NoRandomSmallKnob>(Vec(xx , yy ), module, ComputerscarePultiMoly::CITY_SCALE + block));
      addParam(createParam<NoRandomMediumSmallKnob>(Vec(xx + 32, yy), module, ComputerscarePultiMoly::CITY_OFFSET + block));

      for (int i = 0; i < numKnobs; i++) {
        xx = xInitial + block * citySpacing + 1.4f + 24.3 * (i - i % 8) / 8;
        yy = yInitial + ySpacing * (i % 8) +  yRightColumnOffset * (i - i % 8) ;
        addLabeledKnob(std::to_string(i + 1), xx, yy, module, ComputerscarePultiMoly::EDIT_KNOB + i + block * numKnobs, 0, 0, i, 0);
      }
    }

    for (int outputIndex = 0; outputIndex < numInputCities; outputIndex++) {
      xx = outputXStart + 1.4f + 24.3 * (outputIndex - outputIndex % 8) / 8;
      yy = outputYStart + ySpacing * (outputIndex % 8) +  yRightColumnOffset * (outputIndex - outputIndex % 8) ;
      int paramIndex = ComputerscarePultiMoly::CITY_OUTPUT + outputIndex;

      addOutput(createOutput<PointingUpPentagonPort>(Vec(xx, yy), module, paramIndex));
      //addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module, ComputerscareKnolyPobs::POLY_OUTPUT));

      // focusHighlightPort = new FocusHighlightPort(Vec(xx, yy), module, ComputerscarePultiMoly::CITY_OUTPUT + outputIndex);
      //focusHighlightPort->module = module;

      //addChild(focusHighlightPort);

    }

  }

  struct FocusHighlightPort : Widget {
    // std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));
    //std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob-disabled.svg"));

    int channel = 0;
    bool disabled = false;
    ComputerscarePolyModule *module;

    FocusHighlightPort(math::Vec pos, ComputerscarePolyModule *module, int paramIndex) {
      //addOutput(createOutput<PointingUpPentagonPort>(pos, module, paramIndex));
    }
    void step() override {
      /* if (module) {
         bool candidate = channel > module->polyChannels - 1;
         if (disabled != candidate) {
           setSvg(candidate ? disabledSvg : enabledSvg);
           onChange(*(new event::Change()));
           fb->dirty = true;
           disabled = candidate;
         }
       }
       else {
       }*/
      Widget::step();
    }

  };


  void addLabeledKnob(std::string label, int x, int y, ComputerscarePultiMoly *module, int index, float labelDx, float labelDy, int channel, int type = 0) {

    smallLetterDisplay = new SmallLetterDisplay();
    smallLetterDisplay->box.size = Vec(5, 10);
    smallLetterDisplay->fontSize = 18;
    smallLetterDisplay->value = label;
    smallLetterDisplay->textAlign = 1;

    ParamWidget* pob;


    if (type == 0) {
      DisableableSmoothKnob* fader;
      pob =  createParam<DisableableSmoothKnob>(Vec(x, y), module, index);
      fader = dynamic_cast<DisableableSmoothKnob*>(pob);
      fader->module = module;
      fader->channel = channel;
      addParam(fader);
    } else if (type == 1) {
      DisableableSnapSmoothKnob* fader;
      pob =  createParam<DisableableSnapSmoothKnob>(Vec(x, y), module, index);
      fader = dynamic_cast<DisableableSnapSmoothKnob*>(pob);
      fader->module = module;
      fader->channel = channel;
      addParam(fader);
    }


    smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);
    addChild(smallLetterDisplay);
  }
  PolyOutputChannelsWidget* channelWidget;
  PolyChannelsDisplay* channelDisplay;
  DisableableSmoothKnob* fader;
  SmallLetterDisplay* smallLetterDisplay;
  FocusHighlightPort* focusHighlightPort;
};

Model *modelComputerscarePultiMoly = createModel<ComputerscarePultiMoly, ComputerscarePultiMolyWidget>("computerscare-pulti-moly");
