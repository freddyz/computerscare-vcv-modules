#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "window.hpp"
#include "dtpulse.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareLaundrySoup;
struct LaundryTextField;
struct LaundryTF2;
struct LaundrySmallDisplay;
struct ComputerscareLaundrySoupWidget;

const int numFields = 6;

/*class MyTextField : public LedDisplayTextField {

public:
  int fontSize = 16;
  int rowIndex = 0;
  bool inError = false;
  MyTextField() : LedDisplayTextField() {}
  void setModule(ComputerscareLaundrySoup* _module) {
    module = _module;
  }
  //virtual void onTextChange() override;
  int getTextPosition(Vec mousePos) override {
    bndSetFont(font->handle);
    int textPos = bndIconLabelTextPosition(gVg, textOffset.x, textOffset.y,
                                           box.size.x - 2 * textOffset.x, box.size.y - 2 * textOffset.y,
                                           -1, fontSize, text.c_str(), mousePos.x, mousePos.y);
    bndSetFont(gGuiFont->handle);
    return textPos;
  }
*/

/*void MyTextField::onTextChange() {
  std::string value = module->textFields[this->rowIndex]->text;
  LaundrySoupSequence lss = LaundrySoupSequence(value);

  if(!lss.inError && matchParens(value)) {
    module->textFields[this->rowIndex]->inError=false;
      module->setNextAbsoluteSequence(this->rowIndex);
      module->updateDisplayBlink(this->rowIndex);
  }
  else {
    module->textFields[this->rowIndex]->inError=true;
  }

}*/
struct ComputerscareLaundrySoup : Module {
  enum ParamIds {
    MANUAL_CLOCK_PARAM,
    MANUAL_RESET_PARAM,
    INDIVIDUAL_RESET_PARAM,
    NUM_PARAMS = INDIVIDUAL_RESET_PARAM + numFields
  };
  enum InputIds {
    GLOBAL_CLOCK_INPUT,
    GLOBAL_RESET_INPUT,
    CLOCK_INPUT,
    RESET_INPUT = CLOCK_INPUT + numFields,
    NUM_INPUTS = RESET_INPUT + numFields
  };
  enum OutputIds {
    TRG_OUTPUT,
    FIRST_STEP_OUTPUT = TRG_OUTPUT + numFields,
    NUM_OUTPUTS = FIRST_STEP_OUTPUT + numFields
  };
  enum LightIds {
    SWITCH_LIGHTS,
    NUM_LIGHTS
  };

  rack::dsp::SchmittTrigger globalClockTrigger;
  rack::dsp::SchmittTrigger globalResetTriggerInput;

  rack::dsp::SchmittTrigger globalManualClockTrigger;
  rack::dsp::SchmittTrigger globalManualResetTrigger;

  rack::dsp::SchmittTrigger clockTriggers[numFields];
  rack::dsp::SchmittTrigger resetTriggers[numFields];

  LaundryTF2* textFields[numFields];
  LaundrySmallDisplay* smallLetterDisplays[numFields];

  std::string currentFormula[numFields];
  std::string lastValue[numFields];

  rack::dsp::SchmittTrigger manualResetTriggers[numFields];

  LaundrySoupSequence laundrySequences[numFields];

  bool activeStep[numFields] = {false};

  bool shouldChange[numFields] = {false};
  bool changeImminent[numFields] = {false};
  bool manualSet[numFields] = {false};

  ComputerscareLaundrySoup() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    for (int i = 0; i < numFields; i++) {
      currentFormula[i] = "";
      lastValue[i] = "";
      setNextAbsoluteSequence(i);
      checkIfShouldChange(i);
      resetOneOfThem(i);

    }
  }
  void process(const ProcessArgs &args) override;



  void onRandomize() override {
    randomizeAllFields();
  }

  void randomizeAllFields() {
    std::string mainlookup = "111111111111111111122223333333344444444444444445556667778888888888888999";
    std::string string = "";
    std::string randchar = "";
    int length = 0;

    for (int i = 0; i < numFields; i++) {
      length = floor(random::uniform() * 12) + 1;
      string = "";
      for (int j = 0; j < length; j++) {
        randchar = mainlookup[floor(random::uniform() * mainlookup.size())];
        string = string + randchar;
      }
      currentFormula[i] = string;
      manualSet[i] = true;
      setNextAbsoluteSequence(i);
    }

  }

  void setNextAbsoluteSequence(int index) {
    shouldChange[index] = true;
  }
  void setAbsoluteSequenceFromQueue(int index) {
    LaundrySoupSequence lss = LaundrySoupSequence(currentFormula[index]);
    laundrySequences[index] = lss;
    if (!lss.inError) {
      laundrySequences[index] = lss;
      printf("not in error channel %i\n", index);
      laundrySequences[index].print();
    }
    else {
      printf("ERROR\n");
      lss.print();
      //textFields[index]->inError = true;
    }
  }
  void checkIfShouldChange(int index) {
    if (shouldChange[index]) {
      setAbsoluteSequenceFromQueue(index);
      shouldChange[index] = false;
    }
  }

  /*
  lets say the sequence "332" is entered in the 0th (first)
  numSteps[0] would then be 8 (3 + 3 + 2)
  absoluteSequences[0] would be the vector (1,0,0,1,0,0,1,0)
  absoluteStep[0] would run from 0 to 7

  332-4 (332 offset by 4)

  */
  void incrementInternalStep(int i) {
    laundrySequences[i].incrementAndCheck();
    if (laundrySequences[i].readHead == 0) {
      this->setChangeImminent(i, false);
    }
  }
  std::string getDisplayString(int index) {
    std::string lhs = std::to_string(this->laundrySequences[index].readHead + 1);
    std::string rhs =  std::to_string(this->laundrySequences[index].numSteps);

    padTo(lhs, 3, ' ');
    padTo(rhs, 3, ' ');

    std::string val = lhs + "\n" + rhs;

    return val;
  }
  void setChangeImminent(int i, bool value) {
    changeImminent[i] = value;
  }
  void resetOneOfThem(int i) {
    this->laundrySequences[i].readHead = -1;
  }
};


void ComputerscareLaundrySoup::process(const ProcessArgs &args) {

  bool globalGateIn = globalClockTrigger.isHigh();
  bool atFirstStep = false;
  bool atLastStepAfterIncrement = false;
  bool clocked = globalClockTrigger.process(inputs[GLOBAL_CLOCK_INPUT].getVoltage());
  bool currentTriggerIsHigh = false;
  bool currentTriggerClocked = false;

  bool globalManualResetClicked = globalManualResetTrigger.process(params[MANUAL_RESET_PARAM].getValue());
  bool globalManualClockClicked = globalManualClockTrigger.process(params[MANUAL_CLOCK_PARAM].getValue());

  bool globalResetTriggered = globalResetTriggerInput.process(inputs[GLOBAL_RESET_INPUT].getVoltage() / 2.f);
  bool currentResetActive = false;
  bool currentResetTriggered = false;
  bool currentManualResetClicked;

  for (int i = 0; i < numFields; i++) {
    currentResetActive = inputs[RESET_INPUT + i].isConnected();
    currentResetTriggered = resetTriggers[i].process(inputs[RESET_INPUT + i].getVoltage() / 2.f);
    currentTriggerIsHigh = clockTriggers[i].isHigh();
    currentTriggerClocked = clockTriggers[i].process(inputs[CLOCK_INPUT + i].getVoltage());

    currentManualResetClicked = manualResetTriggers[i].process(params[INDIVIDUAL_RESET_PARAM + i].getValue());

    if (this->laundrySequences[i].numSteps > 0) {
      if (inputs[CLOCK_INPUT + i].isConnected()) {
        if (currentTriggerClocked || globalManualClockClicked) {
          incrementInternalStep(i);
          activeStep[i] = (this->laundrySequences[i].peekWorkingStep() == 1);
          atLastStepAfterIncrement = this->laundrySequences[i].atLastStep();
          if (atLastStepAfterIncrement) checkIfShouldChange(i);
        }
      }
      else {
        if ((inputs[GLOBAL_CLOCK_INPUT].isConnected() && clocked) || globalManualClockClicked) {
          incrementInternalStep(i);
          activeStep[i] = (this->laundrySequences[i].peekWorkingStep() == 1);
          atLastStepAfterIncrement = this->laundrySequences[i].atLastStep();
          if (atLastStepAfterIncrement) checkIfShouldChange(i);
        }
      }

      atFirstStep = (this->laundrySequences[i].readHead == 0);
      if (globalManualResetClicked || currentManualResetClicked) {
        setChangeImminent(i, true);
        checkIfShouldChange(i);
        resetOneOfThem(i);
      }
      if ( (currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {

        setChangeImminent(i, false);
        checkIfShouldChange(i);
        resetOneOfThem(i);

      }
      else {
        if (atFirstStep && !currentResetActive && !inputs[GLOBAL_RESET_INPUT].isConnected()) {
          //checkIfShouldChange(i);
        }
      }
    }

    if (inputs[CLOCK_INPUT + i].isConnected()) {
      outputs[TRG_OUTPUT + i].setVoltage((currentTriggerIsHigh && activeStep[i]) ? 10.0f : 0.0f);
      outputs[FIRST_STEP_OUTPUT + i].setVoltage((currentTriggerIsHigh && atFirstStep) ? 10.f : 0.0f);
    }
    else {
      outputs[TRG_OUTPUT + i].setVoltage((globalGateIn && activeStep[i]) ? 10.0f : 0.0f);
      outputs[FIRST_STEP_OUTPUT + i].setVoltage((globalGateIn && atFirstStep) ? 10.f : 0.0f);
    }
  }
}
struct LaundryTF2 : ComputerscareTextField
{
  ComputerscareLaundrySoup *module;
  //int fontSize = 16;
  int rowIndex = 0;

  LaundryTF2(int i)
  {
    rowIndex = i;
    ComputerscareTextField();
  };
  void draw(const DrawArgs &args) override
  {
    if (module)
    {
      if (module->manualSet[rowIndex]) {
        text = module->currentFormula[rowIndex];
        module->manualSet[rowIndex] = false;
      }
      std::string value = text.c_str();
      if (value != module->lastValue[rowIndex])
      {
        LaundrySoupSequence lss = LaundrySoupSequence(value);
        module->lastValue[rowIndex] = value;
        if (!lss.inError && matchParens(value)) {
          inError = false;
          module->currentFormula[rowIndex] = value;
          module->setNextAbsoluteSequence(this->rowIndex);
        }
        else {
          inError = true;
        }
      }
    }
    ComputerscareTextField::draw(args);
  }

  //void draw(const DrawArgs &args) override;
  //int getTextPosition(math::Vec mousePos) override;
};

struct LaundrySmallDisplay : SmallLetterDisplay
{
  ComputerscareLaundrySoup *module;
  int type;
  int index;
  LaundrySmallDisplay(int i)
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
  }

};

struct ComputerscareLaundrySoupWidget : ModuleWidget {

  double verticalSpacing = 18.4;
  int verticalStart = 22;
  ComputerscareLaundrySoupWidget(ComputerscareLaundrySoup *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareLaundrySoupPanel.svg")));

    //global clock input
    addInput(createInput<InPort>(mm2px(Vec(2 , 0)),  module, ComputerscareLaundrySoup::GLOBAL_CLOCK_INPUT));

    //global reset input
    addInput(createInput<InPort>(mm2px(Vec(12 , 0)),  module, ComputerscareLaundrySoup::GLOBAL_RESET_INPUT));

    //momentary clock and reset buttons
    addParam(createParam<ComputerscareClockButton>(mm2px(Vec(2 , 8)), module, ComputerscareLaundrySoup::MANUAL_CLOCK_PARAM));
    addParam(createParam<ComputerscareResetButton>(mm2px(Vec(12 , 8)), module, ComputerscareLaundrySoup::MANUAL_RESET_PARAM));

    for (int i = 0; i < numFields; i++) {
      //first-step output
      addOutput(createOutput<OutPort>(mm2px(Vec(42 , verticalStart + verticalSpacing * i - 11)),  module, ComputerscareLaundrySoup::FIRST_STEP_OUTPUT + i));

      //individual output
      addOutput(createOutput<OutPort>(mm2px(Vec(54 , verticalStart + verticalSpacing * i - 11)),  module, ComputerscareLaundrySoup::TRG_OUTPUT + i));

      //individual clock input
      addInput(createInput<InPort>(mm2px(Vec(2, verticalStart + verticalSpacing * i - 10)),  module, ComputerscareLaundrySoup::CLOCK_INPUT + i));

      //individual reset input
      addInput(createInput<InPort>(mm2px(Vec(12, verticalStart + verticalSpacing * i - 10)),  module, ComputerscareLaundrySoup::RESET_INPUT + i));

      textFieldTemp = new LaundryTF2(i);
      textFieldTemp->box.pos = mm2px(Vec(1, verticalStart + verticalSpacing * i));
      textFieldTemp->module = module;
      textFieldTemp->box.size = mm2px(Vec(64, 7));
      textFieldTemp->multiline = false;
      textFieldTemp->color = nvgRGB(0xC0, 0xE7, 0xDE);
      textFieldTemp->text = "";

      laundryTextFields[i] = textFieldTemp;
      addChild(textFieldTemp);


      // active / total steps display
      smallLetterDisplay = new LaundrySmallDisplay(i);
      smallLetterDisplay->box.pos = mm2px(Vec(22, verticalStart - 9.2 + verticalSpacing * i));
      smallLetterDisplay->box.size = Vec(60, 30);
      smallLetterDisplay->value = std::to_string(3);
      smallLetterDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
      smallLetterDisplay->module = module;
      addChild(smallLetterDisplay);
      laundrySmallDisplays[i] = smallLetterDisplay;

      addParam(createParam<ComputerscareInvisibleButton>(mm2px(Vec(20, verticalStart - 9.2 + verticalSpacing * i)), module, ComputerscareLaundrySoup::INDIVIDUAL_RESET_PARAM + i));

    }
    laundry = module;
  }
  json_t *toJson() override
  {
    json_t *rootJ = ModuleWidget::toJson();

    json_t *sequencesJ = json_array();
    for (int i = 0; i < numFields; i++) {
      json_t *sequenceJ = json_string(laundryTextFields[i]->text.c_str());
      json_array_append_new(sequencesJ, sequenceJ);
    }
    json_object_set_new(rootJ, "sequences", sequencesJ);

    return rootJ;
  }

  void fromJson(json_t *rootJ) override
  {
    std::string val;
    ModuleWidget::fromJson(rootJ);
    json_t *sequencesJ = json_object_get(rootJ, "sequences");
    if (sequencesJ) {
      for (int i = 0; i < numFields; i++) {

        json_t *sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ)
          val = json_string_value(sequenceJ);
        laundryTextFields[i]->text = val;
        laundry->currentFormula[i] = val;
      }
    }
  }

  ComputerscareLaundrySoup *laundry;

  LaundryTF2 *textFieldTemp;
  LaundryTF2 *laundryTextFields[numFields];
  LaundrySmallDisplay* smallLetterDisplay;
  LaundrySmallDisplay* laundrySmallDisplays[numFields];

};

Model *modelComputerscareLaundrySoup = createModel<ComputerscareLaundrySoup, ComputerscareLaundrySoupWidget>("computerscare-laundry-soup");
