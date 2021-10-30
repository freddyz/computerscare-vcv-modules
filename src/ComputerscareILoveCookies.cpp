#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "dtpulse.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareILoveCookies;
struct CookiesTF2;
struct CookiesSmallDisplay;
struct CookiesCurrentStepDisplay;

const int numFields = 6;
const int numKnobRows = 13;
const int numKnobColumns = 2;
const int numInputRows = 13;
const int numInputColumns = 2;

const int numKnobs = numKnobRows * numKnobColumns;
const int numInputs = numInputRows * numInputColumns;
const std::vector<NVGcolor> outlineColorMap = {COLOR_COMPUTERSCARE_RED, COLOR_COMPUTERSCARE_YELLOW, COLOR_COMPUTERSCARE_BLUE};

const std::string uppercaseLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

struct ComputerscareILoveCookies : Module {
  enum ParamIds {
    KNOB_PARAM,
    MANUAL_CLOCK_PARAM = KNOB_PARAM + numKnobs,
    MANUAL_RESET_PARAM,
    INDIVIDUAL_RESET_PARAM,
    NUM_PARAMS = INDIVIDUAL_RESET_PARAM + numFields
  };
  enum InputIds {
    GLOBAL_CLOCK_INPUT,
    GLOBAL_RESET_INPUT,
    CLOCK_INPUT,
    RESET_INPUT = CLOCK_INPUT + numFields,
    SIGNAL_INPUT = RESET_INPUT + numFields ,
    NUM_INPUTS = SIGNAL_INPUT + numInputs
  };
  enum OutputIds {
    TRG_OUTPUT,
    FIRST_STEP_OUTPUT = TRG_OUTPUT + numFields,
    NUM_OUTPUTS = FIRST_STEP_OUTPUT + numFields
  };
  enum LightIds {
    SWITCH_LIGHTS,
    NUM_LIGHTS = SWITCH_LIGHTS + (numKnobs + numInputs) * numFields
  };

  rack::dsp::SchmittTrigger globalClockTrigger;
  rack::dsp::SchmittTrigger globalResetTriggerInput;

  rack::dsp::SchmittTrigger globalManualClockTrigger;
  rack::dsp::SchmittTrigger globalManualResetTrigger;

  rack::dsp::SchmittTrigger clockTriggers[numFields];
  rack::dsp::SchmittTrigger resetTriggers[numFields];

  rack::dsp::SchmittTrigger manualResetTriggers[numFields];

  SmallLetterDisplay* smallLetterDisplays[numFields];
  SmallLetterDisplay* currentWorkingStepDisplays[numFields];

  AbsoluteSequence newABS[numFields];
  AbsoluteSequence newABSQueue[numFields];

  std::string currentFormula[numFields];
  std::string currentTextFieldValue[numFields];

  std::string upcomingFormula[numFields];
  std::string lastValue[numFields];


  bool manualSet[numFields];
  bool inError[numFields];
  bool shouldChange[numFields] = {false};
  bool changeImminent[numFields] = {false};

  int activeKnobIndex[numFields] = {0};

  int knobRangeEnum = 0;

  int checkCounter = 0;
  int checkCounterLimit = 10000;

  bool jsonLoaded = false;

  std::vector<ParamWidget*> smallLetterKnobs;

  ComputerscareILoveCookies() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    for (int i = 0; i < numFields; i++) {
      manualSet[i] = false;
      inError[i] = false;

      currentFormula[i] = "";
      lastValue[i] = "";
      setNextAbsoluteSequence(i);
      checkIfShouldChange(i);
      resetOneOfThem(i);

      std::string rowi = std::to_string(i + 1);

      configButton(INDIVIDUAL_RESET_PARAM + i, "Reset Row " + rowi );

      configInput(CLOCK_INPUT + i, "Row " + rowi + " Clock");
      configInput(RESET_INPUT + i, "Row " + rowi + " Reset");

      configOutput(TRG_OUTPUT + i, "Row " + rowi + " CV");
      configOutput(FIRST_STEP_OUTPUT + i, "Row " + rowi + " End of Cycle");
    }
    for (int k = 0; k < numKnobs; k++) {
      configParam( KNOB_PARAM + k, 0.f, 10.f, 0.0f, string::f("knob %c", knoblookup[k]));

      configInput(SIGNAL_INPUT + k, string::f("%c", uppercaseLetters.at(k)));
    }

    configButton(MANUAL_CLOCK_PARAM, "Manual Clock Advance");
    configButton(MANUAL_RESET_PARAM, "Manual Reset");

    configInput(GLOBAL_CLOCK_INPUT, "Global Clock");
    configInput(GLOBAL_RESET_INPUT, "Global Reset");

  }
  json_t *dataToJson() override {
    json_t *rootJ = json_object();

    json_t *sequencesJ = json_array();
    json_t *knobRangeJ = json_integer(knobRangeEnum);

    for (int i = 0; i < numFields; i++) {
      json_t *sequenceJ = json_string(currentTextFieldValue[i].c_str());
      json_array_append_new(sequencesJ, sequenceJ);


    }
    json_object_set_new(rootJ, "sequences", sequencesJ);
    json_object_set_new(rootJ, "knobRange", knobRangeJ);

    return rootJ;
  }

  void dataFromJson(json_t *rootJ) override {
    std::string val;
    int count;
    json_t *sequencesJ = json_object_get(rootJ, "sequences");
    if (sequencesJ) {
      for (int i = 0; i < numFields; i++) {

        json_t *sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ) {
          val = json_string_value(sequenceJ);

          // currentFormula[i] = val;
          //currentTextFieldValue[i] = val;
          currentTextFieldValue[i] = val;

          manualSet[i] = true;
        }
      }
      jsonLoaded = true;
    }
    else {
      json_t *textJLegacy = json_object_get(rootJ, "data");
      if (textJLegacy) {
        json_t *seqJLegacy = json_object_get(textJLegacy, "sequences");

        if (seqJLegacy) {
          for (int i = 0; i < numFields; i++) {
            json_t *sequenceJ = json_array_get(seqJLegacy, i);
            if (sequenceJ)
              val = json_string_value(sequenceJ);
            // currentFormula[i] = val;
            //lastValue[i] = val;
            currentTextFieldValue[i] = val;
            //upcomingFormula[i]=val;
            manualSet[i] = true;

          }
        }
      }
    }
    json_t *knobRangeJ = json_object_get(rootJ, "knobRange");
    if (knobRangeJ) {
      knobRangeEnum = json_integer_value(knobRangeJ);
    }


  }
  void process(const ProcessArgs &args) override;



  void onRandomize() override {
  }
  void randomizeShuffle() {

  }
  void wiggleKnobs() {
    for (int i = 0; i < numKnobs; i++) {
      float prev = params[KNOB_PARAM + i].getValue();
      if (random::uniform() < 0.7) {
        float rv = (10 * random::uniform() + 2 * prev) / 3;
        params[KNOB_PARAM + i].setValue(rv);
      }
    }
  }
  std::string randomCookieFormula() {
    std::string mainlookup = knoblookup;
    std::string str = "";
    std::string randchar = "";

    float ru;
    int length = 0;
    length = floor(random::uniform() * 12) + 2;
    str = "";
    for (int j = 0; j < length; j++) {
      randchar = mainlookup[floor(random::uniform() * mainlookup.size())];
      str = str + randchar;
      ru = random::uniform();
      if (ru < 0.1) {
        str = "(" + str + ")";
      }
    }
    return str;
  }

  void randomizeAllFields() {
    for (int i = 0; i < numFields; i++) {
      randomizeAField(i);
    }
  }

  void randomizeAField(int i) {
    currentTextFieldValue[i] = randomCookieFormula();
    manualSet[i] = true;
    setNextAbsoluteSequence(i);


  }
  void setNextAbsoluteSequence(int index) {
    newABSQueue[index] = AbsoluteSequence(upcomingFormula[index], knobandinputlookup);
    shouldChange[index] = true;
  }
  void setAbsoluteSequenceFromQueue(int index) {
    newABS[index] = newABSQueue[index];
    currentFormula[index] = upcomingFormula[index];
    newABS[index].incrementAndCheck();
  }
  void checkIfShouldChange(int index) {
    if (shouldChange[index]) {
      setAbsoluteSequenceFromQueue(index);
      shouldChange[index] = false;
    }
  }

  void incrementInternalStep(int i) {
    newABS[i].incrementAndCheck();
    if (newABS[i].readHead == 0) {
      setChangeImminent(i, false);
      checkIfShouldChange(i);
    }
  }

  void resetOneOfThem(int i) {
    newABS[i].readHead = -1;
  }
  void setChangeImminent(int i, bool value) {
    changeImminent[i] = value;
  }
  std::string getDisplayString(int index) {
    std::string lhs = std::to_string(this->newABS[index].readHead + 1);
    std::string rhs =  std::to_string(this->newABS[index].numTokens);

    padTo(lhs, 3, ' ');
    padTo(rhs, 3, ' ');

    std::string val = lhs + "\n" + rhs;

    return val;
  }
  float mapKnobValue(float rawValue, int rowIndex) {
    // raw value is between 0 and +10

    float mappedValue = 0.f;
    int mapEnum = knobRangeEnum;
    switch (mapEnum) {
    case 0: mappedValue = rawValue; break;//0..10
    case 1: mappedValue = mapValue(rawValue, 0.f, 0.5); break;//0..5
    case 2: mappedValue = mapValue(rawValue, 0.f, 0.2); break;//0..2
    case 3: mappedValue = mapValue(rawValue, 0.f, 0.1); break;//0..1


    case 4: mappedValue = mapValue(rawValue, -5, 2.f); break;//-10..10
    case 5: mappedValue = mapValue(rawValue, -5, 1.f); break;//-5..5

    case 6: mappedValue = mapValue(rawValue, -5, 0.4); break;//-2..2

    case 7: mappedValue = mapValue(rawValue, -5, 0.2); break;//-1..1
    }
    return mappedValue;
  }
  float mapValue(float input, float offset, float multiplier) {
    return (input + offset) * multiplier;
  }
  void checkTextField(int channel) {
    std::string textFieldValue = currentTextFieldValue[channel];

    if (textFieldValue != currentFormula[channel] && textFieldValue != upcomingFormula[channel]) {

      AbsoluteSequence pendingSequence = AbsoluteSequence(textFieldValue, knobandinputlookup);
      if (!pendingSequence.inError && matchParens(textFieldValue)) {
        upcomingFormula[channel] = textFieldValue;
        setNextAbsoluteSequence(channel);
        inError[channel] = false;
      }
      else {
        inError[channel] = true;
      }
    }

  }

};


void ComputerscareILoveCookies::process(const ProcessArgs &args) {


  bool globalGateIn = globalClockTrigger.isHigh();
  bool activeStep = 0;
  bool atFirstStep = false;
  bool globalTriggerClocked = globalClockTrigger.process(inputs[GLOBAL_CLOCK_INPUT].getVoltage());
  bool globalManualResetClicked = globalManualResetTrigger.process(params[MANUAL_RESET_PARAM].getValue());
  bool globalManualClockClicked = globalManualClockTrigger.process(params[MANUAL_CLOCK_PARAM].getValue());
  bool currentTriggerIsHigh;
  bool currentTriggerClocked;
  bool globalResetTriggered = globalResetTriggerInput.process(inputs[GLOBAL_RESET_INPUT].getVoltage() / 2.f);
  bool currentResetActive;
  bool currentResetTriggered;
  bool currentManualResetClicked;
  bool outputConnected;
  float knobRawValue = 0.f;
  float inV[16] = {10.f};

  if (checkCounter > checkCounterLimit) {
    if (!jsonLoaded) {
      for (int i = 0; i < numFields; i++) {
        //currentTextFieldValue[i] = i < numFields - 1 ? std::to_string(i + 1) : "abcd";
        manualSet[i] = true;
      }
      for (int i = 0; i < numFields; i++) {
        checkTextField(i);
        checkIfShouldChange(i);
      }
      jsonLoaded = true;
    }
    else {
      for (int i = 0; i < numFields; i++) {
        checkTextField(i);
      }
    }
    checkCounter = 0;
  }
  checkCounter++;

  for (int i = 0; i < numFields; i++) {
    activeStep = false;
    currentResetActive = inputs[RESET_INPUT + i].isConnected();
    outputConnected = outputs[TRG_OUTPUT + i].isConnected();

    currentResetTriggered = resetTriggers[i].process(inputs[RESET_INPUT + i].getVoltage() / 2.f);
    currentManualResetClicked = manualResetTriggers[i].process(params[INDIVIDUAL_RESET_PARAM + i].getValue());

    currentTriggerIsHigh = clockTriggers[i].isHigh();
    currentTriggerClocked = clockTriggers[i].process(inputs[CLOCK_INPUT + i].getVoltage());

    if (true) {
      if (inputs[CLOCK_INPUT + i].isConnected()) {
        if (currentTriggerClocked) {
          incrementInternalStep(i);
          activeKnobIndex[i] = newABS[i].peekWorkingStep();
        }
      }
      else {
        if ((inputs[GLOBAL_CLOCK_INPUT].isConnected() && globalTriggerClocked) || globalManualClockClicked) {
          incrementInternalStep(i);
          activeKnobIndex[i] = newABS[i].peekWorkingStep();
        }
      }
      if ((currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {
        resetOneOfThem(i);
      }

      atFirstStep = (newABS[i].readHead == 0);

      if (globalManualResetClicked || currentManualResetClicked) {
        setChangeImminent(i, true);
        resetOneOfThem(i);
      }
      if ( (currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {
        resetOneOfThem(i);
        setChangeImminent(i, false);
      }
      else {
        if (atFirstStep && !currentResetActive && !inputs[GLOBAL_RESET_INPUT].isConnected()) {
          //checkIfShouldChange(i);
        }
      }
    }
    if (outputConnected) {
      if (activeKnobIndex[i] < 0) {
        outputs[TRG_OUTPUT + i].setChannels(1);
        outputs[TRG_OUTPUT + i].setVoltage(0.f);
      }
      else if (activeKnobIndex[i] < 26) {
        outputs[TRG_OUTPUT + i].setChannels(1);
        knobRawValue = params[activeKnobIndex[i]].getValue();
        outputs[TRG_OUTPUT + i].setVoltage(mapKnobValue(knobRawValue, i));
      }
      else if (activeKnobIndex[i] < 52) {
        outputs[TRG_OUTPUT + i].setChannels(inputs[SIGNAL_INPUT + activeKnobIndex[i] - 26].getChannels());
        inputs[SIGNAL_INPUT + activeKnobIndex[i] - 26].readVoltages(inV);
        outputs[TRG_OUTPUT + i].writeVoltages(inV);
      }
      else if (activeKnobIndex[i] < 78) {
        outputs[TRG_OUTPUT + i].setChannels(1);
        outputs[TRG_OUTPUT + i].setVoltage(newABS[i].exactFloats[activeKnobIndex[i] - 52]);
      }
      else if (activeKnobIndex[i] < 104) {
        outputs[TRG_OUTPUT + i].setChannels(1);
        outputs[TRG_OUTPUT + i].setVoltage(2.22);
      }
      else {
        outputs[TRG_OUTPUT + i].setChannels(1);
        outputs[TRG_OUTPUT + i].setVoltage(0.f);
      }
    }
    if (inputs[CLOCK_INPUT + i].isConnected()) {
      outputs[FIRST_STEP_OUTPUT + i].setVoltage((currentTriggerIsHigh && atFirstStep) ? 10.f : 0.0f);
    }
    else {
      outputs[FIRST_STEP_OUTPUT + i].setVoltage((globalGateIn && atFirstStep) ? 10.f : 0.0f);
    }
  }
}


struct WiggleKnobsMenuItem : MenuItem {
  ComputerscareILoveCookies *cookies;
  void onAction(const event::Action &e) override {
    cookies->wiggleKnobs();
  }
};
struct RandomizeTextFieldsMenuItem : MenuItem {
  ComputerscareILoveCookies *cookies;
  void onAction(const event::Action &e) override {
    srand(time(0));
    cookies->randomizeAllFields();

  }
};
struct CookiesKnobRangeItem : MenuItem {
  ComputerscareILoveCookies *cookies;
  int knobRangeEnum;
  void onAction(const event::Action &e) override {
    cookies->knobRangeEnum = knobRangeEnum;
  }
  void step() override {
    rightText = CHECKMARK(cookies->knobRangeEnum == knobRangeEnum);
    MenuItem::step();
  }
};
struct CookiesTF2 : ComputerscareTextField
{
  ComputerscareILoveCookies *module;
  //int fontSize = 16;
  int rowIndex = 0;

  CookiesTF2(int i)
  {
    rowIndex = i;
    ComputerscareTextField();
  };
  void draw(const DrawArgs &args) override
  {
    if (module)
    {
      if (module->manualSet[rowIndex]) {
        text = module->currentTextFieldValue[rowIndex];
        module->manualSet[rowIndex] = false;
      }
      std::string value = text.c_str();
      module->currentTextFieldValue[rowIndex] = value;
      inError = module->inError[rowIndex];
    }
    else {
      text = "we,love{}@9,cook(ies)";
    }
    ComputerscareTextField::draw(args);
  }
};

struct CookiesSmallDisplay : SmallLetterDisplay
{
  ComputerscareILoveCookies *module;
  int type;
  int index;
  CookiesSmallDisplay(int i)
  {
    index = i;
    SmallLetterDisplay();
  };
  void draw(const DrawArgs &args)
  {
    //this->setNumDivisionsString();
    if (module)
    {
      value = module->getDisplayString(index);
      blink = module->shouldChange[index];
      doubleblink = module->changeImminent[index];
      SmallLetterDisplay::draw(args);
    }
    else {
      value = "4\n20";
      SmallLetterDisplay::draw(args);
    }
  }

};
struct CookiesCurrentStepDisplay : SmallLetterDisplay
{
  ComputerscareILoveCookies *module;
  int type;
  int index;
  CookiesCurrentStepDisplay(int i)
  {
    index = i;
    fontSize = 26;
    textAlign = 4;
    SmallLetterDisplay();
  };
  void draw(const DrawArgs &args)
  {
    if (module)
    {
      value = module->newABS[index].getWorkingStepDisplay();

      SmallLetterDisplay::draw(args);
    }
  }

};

struct ComputerscareILoveCookiesWidget : ModuleWidget {

  double verticalSpacing = 18.4;
  int verticalStart = 24;
  double xStart = 41;
  int index = 0;
  int inputindex = 0;
  double knobPosX = 0.0;
  double knobPosY = 0.0;
  double knobXStart = 20;
  double knobYStart = 2;
  double knobRowWidth = 11;
  double knobColumnHeight = 9.58;

  double inputPosX = 0.0;
  double inputPosY = 0.0;
  double inputXStart = 0;
  double inputYStart = 0;
  double inputRowWidth = 9.4;
  double inputColumnHeight = 9.7;

  ComputerscareILoveCookiesWidget(ComputerscareILoveCookies *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareILoveCookiesPanel.svg")));

    for (int i = 0; i < numKnobRows; i++) {
      for (int j = 0; j < numKnobColumns; j++) {
        knobPosX = knobXStart + j * knobRowWidth;
        knobPosY = knobYStart + i * knobColumnHeight + j * 2.0;
        index = numKnobColumns * i + j;


        //label for knob a-z
        smallLetterDisplay = new SmallLetterDisplay();
        smallLetterDisplay->box.pos = mm2px(Vec(knobPosX + 6, knobPosY - 2));
        smallLetterDisplay->box.size = Vec(20, 20);
        smallLetterDisplay->value = knoblookup[index];

        addChild(smallLetterDisplay);

        ParamWidget* knob =  createParam<SmoothKnob>(mm2px(Vec(knobPosX, knobPosY)), module, ComputerscareILoveCookies::KNOB_PARAM + index);

        //module->smallLetterKnobs.push_back(knob);
        addParam(knob);

      }
    }
    for (int k = 0; k < numInputRows; k++) {
      for (int m = 0; m < numInputColumns; m++) {
        inputPosX = inputXStart + m * inputRowWidth;
        inputPosY = inputYStart + k * inputColumnHeight + m * 2.0;
        inputindex = numInputColumns * k + m;

        if (m % 2) {
          addInput(createInput<InPort>(mm2px(Vec(inputPosX , inputPosY)),  module, ComputerscareILoveCookies::SIGNAL_INPUT + inputindex));
        }
        else {
          addInput(createInput<PointingUpPentagonPort>(mm2px(Vec(inputPosX , inputPosY)),  module, ComputerscareILoveCookies::SIGNAL_INPUT + inputindex));
        }

        //label for input A-Z
        smallLetterDisplay = new SmallLetterDisplay();
        smallLetterDisplay->box.pos = mm2px(Vec(inputPosX + 6, inputPosY - 1));
        smallLetterDisplay->box.size = Vec(20, 20);
        smallLetterDisplay->value = inputlookup[inputindex];

        addChild(smallLetterDisplay);
        //module->smallLetterDisplays[i] = smallLetterDisplay;
      }
    }

    //global clock input
    addInput(createInput<InPort>(mm2px(Vec(2 + xStart , 0)), module, ComputerscareILoveCookies::GLOBAL_CLOCK_INPUT));

    //global reset input
    addInput(createInput<InPort>(mm2px(Vec(12 + xStart , 0)),  module, ComputerscareILoveCookies::GLOBAL_RESET_INPUT));
    addParam(createParam<ComputerscareResetButton>(mm2px(Vec(12 + xStart , 9)), module, ComputerscareILoveCookies::MANUAL_RESET_PARAM));
    addParam(createParam<ComputerscareClockButton>(mm2px(Vec(2 + xStart , 9)), module, ComputerscareILoveCookies::MANUAL_CLOCK_PARAM));


    for (int i = 0; i < numFields; i++) {
      //first-step output
      addOutput(createOutput<OutPort>(mm2px(Vec(42 + xStart , verticalStart + verticalSpacing * i - 11)), module, ComputerscareILoveCookies::FIRST_STEP_OUTPUT + i));

      //individual output
      addOutput(createOutput<OutPort>(mm2px(Vec(54 + xStart , verticalStart + verticalSpacing * i - 11)),  module, ComputerscareILoveCookies::TRG_OUTPUT + i));

      //individual clock input
      addInput(createInput<InPort>(mm2px(Vec(2 + xStart, verticalStart + verticalSpacing * i - 10)),  module, ComputerscareILoveCookies::CLOCK_INPUT + i));

      //individual reset input
      addInput(createInput<InPort>(mm2px(Vec(12 + xStart, verticalStart + verticalSpacing * i - 10)), module, ComputerscareILoveCookies::RESET_INPUT + i));

      //sequence input field
      textField = new CookiesTF2(i);
      textField->box.pos = mm2px(Vec(1 + xStart, verticalStart + verticalSpacing * i));
      textField->box.size = mm2px(Vec(63, 7));
      textField->multiline = false;
      textField->color = nvgRGB(0xC0, 0xE7, 0xDE);
      textField->module = module;
      addChild(textField);
      cookiesTextFields[i] = textField;

      //active/total steps display
      cookiesSmallDisplay = new CookiesSmallDisplay(i);
      cookiesSmallDisplay->box.pos = mm2px(Vec(21 + xStart, verticalStart - 9.2 + verticalSpacing * i));
      cookiesSmallDisplay->box.size = Vec(60, 30);
      cookiesSmallDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
      cookiesSmallDisplay->value = "?\n?";
      addChild(cookiesSmallDisplay);
      cookiesSmallDisplay->module = module;
      cookiesSmallDisplays[i] = cookiesSmallDisplay;

      //active/total steps display
      currentWorkingStepDisplay = new CookiesCurrentStepDisplay(i);
      currentWorkingStepDisplay->box.pos = mm2px(Vec(11 + xStart, verticalStart - 7.0 + verticalSpacing * i));
      currentWorkingStepDisplay->box.size = mm2px(Vec(2, 10));

      currentWorkingStepDisplay->value = "?";
      currentWorkingStepDisplay->module = module;
      addChild(currentWorkingStepDisplay);
      currentWorkingStepDisplays[i] = currentWorkingStepDisplay;

      addParam(createParam<ComputerscareInvisibleButton>(mm2px(Vec(21 + xStart, verticalStart - 9.9 + verticalSpacing * i)), module, ComputerscareILoveCookies::INDIVIDUAL_RESET_PARAM + i));
    }
    cookies = module;
  }


  /*void fromJson(json_t *rootJ) override
  { std::string val;
    ModuleWidget::fromJson(rootJ);
    json_t *sequencesJ = json_object_get(rootJ, "sequences");//legacy
    if (sequencesJ) {
      for (int i = 0; i < numFields; i++) {
        json_t *sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ) {
          val = json_string_value(sequenceJ);
          cookies->currentTextFieldValue[i] = val;
          cookies->manualSet[i] = true;
        }
      }
      cookies->jsonLoaded = true;
    }
    else {
      json_t *textJLegacy = json_object_get(rootJ, "data");
      if (textJLegacy) {
        json_t *seqJLegacy = json_object_get(textJLegacy, "sequences");

        if (seqJLegacy) {
          for (int i = 0; i < numFields; i++) {
            json_t *sequenceJ = json_array_get(seqJLegacy, i);
            if (sequenceJ)
              val = json_string_value(sequenceJ);
            cookiesTextFields[i]->text = val;
            cookies->currentFormula[i] = val;
          }
        }
      }
    }
  }*/


  ComputerscareILoveCookies *cookies;

  CookiesTF2 *textField;
  CookiesTF2 *cookiesTextFields[numFields];



  CookiesSmallDisplay* cookiesSmallDisplay;
  CookiesSmallDisplay* cookiesSmallDisplays[numFields];

  SmallLetterDisplay* smallLetterDisplay;

  CookiesCurrentStepDisplay* currentWorkingStepDisplay;
  CookiesCurrentStepDisplay* currentWorkingStepDisplays[numFields];


  void appendContextMenu(Menu *menu) override;
};

void ComputerscareILoveCookiesWidget::appendContextMenu(Menu *menu) {
  ComputerscareILoveCookies *cookiesModule = dynamic_cast<ComputerscareILoveCookies *>(this->module);

  MenuLabel *spacerLabel = new MenuLabel();
  menu->addChild(spacerLabel);

  MenuLabel *modeLabel = new MenuLabel();
  modeLabel->text = "Premium Randomizations";
  menu->addChild(modeLabel);

  WiggleKnobsMenuItem *wiggleKnobsMenuItem = new WiggleKnobsMenuItem();
  wiggleKnobsMenuItem->text = "Wiggle Knobs";
  wiggleKnobsMenuItem->cookies = cookiesModule;
  menu->addChild(wiggleKnobsMenuItem);

  RandomizeTextFieldsMenuItem *randomizeTextFieldsMenuItem = new RandomizeTextFieldsMenuItem();
  randomizeTextFieldsMenuItem->text = "Randomize Text Fields";
  randomizeTextFieldsMenuItem->cookies = cookiesModule;
  menu->addChild(randomizeTextFieldsMenuItem);


  menu->addChild(construct<MenuLabel>());
  menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Knob Range"));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, "  0v ... +10v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 0));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, "  0v ...  +5v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 1));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, "  0v ...  +2v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 2));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, "  0v ...  +1v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 3));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, "-10v ... +10v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 4));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, " -5v ...  +5v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 5));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, " -2v ...  +2v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 6));
  menu->addChild(construct<CookiesKnobRangeItem>(&MenuItem::text, " -1v ...  +1v", &CookiesKnobRangeItem::cookies, cookiesModule, &CookiesKnobRangeItem::knobRangeEnum, 7));

};






Model *modelComputerscareILoveCookies = createModel<ComputerscareILoveCookies, ComputerscareILoveCookiesWidget>("computerscare-i-love-cookies");
