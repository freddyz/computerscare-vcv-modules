#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "window.hpp"
#include "dtpulse.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareILoveCookies;

const int numFields = 6;
const int numKnobRows = 13;
const int numKnobColumns = 2;
const int numInputRows = 13;
const int numInputColumns = 2;

const int LG_FONT_SIZE = 18;
const int MED_FONT_SIZE = 12;
const int SM_FONT_SIZE = 8;

const int numKnobs = numKnobRows * numKnobColumns;
const int numInputs = numInputRows * numInputColumns;
const std::vector<NVGcolor> outlineColorMap = {COLOR_COMPUTERSCARE_RED,COLOR_COMPUTERSCARE_YELLOW,COLOR_COMPUTERSCARE_BLUE};


class MyTextFieldCookie : public LedDisplayTextField {

public:
  int fontSize = LG_FONT_SIZE;
  int rowIndex=0;
  bool inError = false;
  MyTextFieldCookie() : LedDisplayTextField() {}
  void setModule(ComputerscareILoveCookies* _module) {
    module = _module;
  }
  virtual void onTextChange() override;
  int getTextPosition(Vec mousePos) override {
    bndSetFont(font->handle);
    int textPos = bndIconLabelTextPosition(gVg, textOffset.x, textOffset.y,
      box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
      -1, fontSize, text.c_str(), mousePos.x, mousePos.y);
    bndSetFont(gGuiFont->handle);
    return textPos;
  }
  void draw(NVGcontext *vg) override {
    nvgScissor(vg, 0, 0, box.size.x, box.size.y);

    // Background
    nvgFontSize(vg, fontSize);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 5.0);
    if(inError) {
      nvgFillColor(vg, COLOR_COMPUTERSCARE_PINK);
    }
    else {
      nvgFillColor(vg, nvgRGB(0x00, 0x00, 0x00));
    }
    nvgFill(vg);

    // Text
    if (font->handle >= 0) {
      bndSetFont(font->handle);

      NVGcolor highlightColor = color;
      highlightColor.a = 0.5;
      int begin = min(cursor, selection);
      int end = (this == gFocusedWidget) ? max(cursor, selection) : -1;
      //bndTextField(vg,textOffset.x,textOffset.y+2, box.size.x, box.size.y, -1, 0, 0, const char *text, int cbegin, int cend);
      bndIconLabelCaret(vg, textOffset.x, textOffset.y - 3,
        box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
        -1, color, fontSize, text.c_str(), highlightColor, begin, end);

      bndSetFont(gGuiFont->handle);
    }

    nvgResetScissor(vg);
  };

private:
  ComputerscareILoveCookies* module;
};

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
    NUM_LIGHTS = SWITCH_LIGHTS + (numKnobs + numInputs)*numFields
	};

  SchmittTrigger globalClockTrigger;
  SchmittTrigger globalResetTriggerInput;

  SchmittTrigger globalManualClockTrigger;
  SchmittTrigger globalManualResetTrigger;

  SchmittTrigger clockTriggers[numFields];
  SchmittTrigger resetTriggers[numFields];

  SchmittTrigger manualResetTriggers[numFields];

  MyTextFieldCookie* textFields[numFields];
  SmallLetterDisplay* smallLetterDisplays[numFields];
  SmallLetterDisplay* currentWorkingStepDisplays[numFields];

  AbsoluteSequence newABS[numFields];
  AbsoluteSequence newABSQueue[numFields];
  
  bool shouldChange[numFields] = {false};
  bool changeImminent[numFields] = {false};

  int activeKnobIndex[numFields] = {0};
 
  std::vector<ParamWidget*> smallLetterKnobs;

ComputerscareILoveCookies() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	json_t *toJson() override
  {
		json_t *rootJ = json_object();
    
    json_t *sequencesJ = json_array();
    for (int i = 0; i < numFields; i++) {
      json_t *sequenceJ = json_string(textFields[i]->text.c_str());
      json_array_append_new(sequencesJ, sequenceJ);
    }
    json_object_set_new(rootJ, "sequences", sequencesJ);

    return rootJ;
  } 
  
  void fromJson(json_t *rootJ) override
  {
    json_t *sequencesJ = json_object_get(rootJ, "sequences");
    if (sequencesJ) {
      for (int i = 0; i < numFields; i++) {
        json_t *sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ)
          textFields[i]->text = json_string_value(sequenceJ);
      }
    }
    onCreate();
  }
 
  void onRandomize() override {
  }
  void randomizeShuffle() {

  }
  void wiggleKnobs() {
   for(int i = 0; i < numKnobs; i++) {
      float prev = params[KNOB_PARAM+i].value;
      if(randomUniform() < 0.7) {
        float rv = (10*randomUniform()+2*prev)/3;
        this->smallLetterKnobs[i]->setValue(rv);
        params[KNOB_PARAM+i].value = rv;
      }
    }
  }
  void randomizeTextFields() {
    std::string mainlookup = knoblookup;
    std::string str = "";
    std::string randchar = "";
    
    float ru;
    int length = 0;
    for (int i = 0; i < numFields; i++) {
      
      length = floor(randomUniform()*12) + 2;
      str = "";
      for(int j = 0; j < length; j++) {
        randchar = mainlookup[floor(randomUniform()*mainlookup.size())];
        str = str + randchar;
        ru = randomUniform();
        if(ru < 0.1) {
          str = "(" + str + ")";
        }
      }
      textFields[i]->text = str;
      setNextAbsoluteSequence(i);
    }
  }
  void checkLength(int index) {
    std::string value=textFields[index]->text;
    int length = value.length();
      textFields[index]->fontSize = length > 17 ? (length > 30 ? SM_FONT_SIZE : MED_FONT_SIZE) : LG_FONT_SIZE;
  }
  void setNextAbsoluteSequence(int index) {
    newABSQueue[index] = AbsoluteSequence(textFields[index]->text,knobandinputlookup);
    shouldChange[index] = true;
  }
  void setAbsoluteSequenceFromQueue(int index) {
    newABS[index] = newABSQueue[index];
    newABS[index].incrementAndCheck();
  }
  void checkIfShouldChange(int index) {
    if(shouldChange[index]) {
      setAbsoluteSequenceFromQueue(index);
      shouldChange[index] = false;
      updateDisplayBlink(index);
    }
  }
  void updateDisplayBlink(int index) {
    smallLetterDisplays[index]->blink = shouldChange[index];
  }
void onCreate () override
  {
    for(int i = 0; i < numFields; i++) {
      setNextAbsoluteSequence(i);
      checkIfShouldChange(i);
      resetOneOfThem(i);
    }
  }

  void onReset () override
  {
    onCreate();
  }

  void incrementInternalStep(int i) {
    newABS[i].incrementAndCheck();
    if(newABS[i].readHead == 0) {
      this->setChangeImminent(i,false);
      checkIfShouldChange(i);
    }
    this->smallLetterDisplays[i]->value = this->getDisplayString(i);
    this->currentWorkingStepDisplays[i]->value = this->newABS[i].getWorkingStepDisplay();
  }

  void resetOneOfThem(int i) {
    newABS[i].readHead = -1;
  }
  void setChangeImminent(int i,bool value) {
    this->smallLetterDisplays[i]->doubleblink = value;
  }
  std::string getDisplayString(int index) {
    std::string lhs = std::to_string(this->newABS[index].readHead + 1);
    std::string rhs =  std::to_string(this->newABS[index].numTokens);

    padTo(lhs, 3,' ');
    padTo(rhs, 3,' ');
    
    std::string val = lhs + "\n" + rhs;

    return val;
  }
	float mapKnobValue(float rawValue, int rowIndex) {
		// raw value is between 0 and +10
		/*
		0:	-10,10
		1:	-5,5
		2:	0,10
		3:	0,5
		4:	0,1
    5: -1,1
    6: 0,2
    7: 0,3
    8: -2,2
		*/
		float mappedValue = 0.f;
		int mapEnum = 2;
		switch(mapEnum) {
			case 0: mappedValue = mapValue(rawValue,-5.f,2.f); break;
			case 1: mappedValue = mapValue(rawValue,-5.f,1.f); break;
			case 2: mappedValue = rawValue; break;
			case 3: mappedValue = mapValue(rawValue,0.f,0.5); break;
			case 4: mappedValue = mapValue(rawValue,0.f,0.1); break;
      case 5: mappedValue = mapValue(rawValue,-5,0.2); break;
      case 6: mappedValue = mapValue(rawValue,0.f,0.2); break;
      case 7: mappedValue = mapValue(rawValue,0.f,1/3); break;
      case 8: mappedValue = mapValue(rawValue,-5.f,0.4); break;
		}
		return mappedValue;
	}
  float mapValue(float input, float offset, float multiplier) {
    return (input + offset) * multiplier;
  }

};


void ComputerscareILoveCookies::step() {


  bool globalGateIn = globalClockTrigger.isHigh();
  bool activeStep = 0;
  bool atFirstStep = false;
  bool globalTriggerClocked = globalClockTrigger.process(inputs[GLOBAL_CLOCK_INPUT].value);
  bool globalManualResetClicked = globalManualResetTrigger.process(params[MANUAL_RESET_PARAM].value);
  bool globalManualClockClicked = globalManualClockTrigger.process(params[MANUAL_CLOCK_PARAM].value);
  bool currentTriggerIsHigh;
  bool currentTriggerClocked;
  bool globalResetTriggered = globalResetTriggerInput.process(inputs[GLOBAL_RESET_INPUT].value / 2.f);
  bool currentResetActive;
  bool currentResetTriggered;
  bool currentManualResetClicked;
  bool onLastStepAfterIncrement;
	float knobRawValue = 0.f;
  for(int i = 0; i < numFields; i++) {
    activeStep = false;
    currentResetActive = inputs[RESET_INPUT + i].active;
    currentResetTriggered = resetTriggers[i].process(inputs[RESET_INPUT+i].value / 2.f);
    currentManualResetClicked = manualResetTriggers[i].process(params[INDIVIDUAL_RESET_PARAM + i].value);

    currentTriggerIsHigh = clockTriggers[i].isHigh();
    currentTriggerClocked = clockTriggers[i].process(inputs[CLOCK_INPUT + i].value);

    if(true) {
      if (inputs[CLOCK_INPUT + i].active) {
        if(currentTriggerClocked) {
          incrementInternalStep(i);
          activeKnobIndex[i] = newABS[i].peekWorkingStep();
        }
      }
      else {
        if ((inputs[GLOBAL_CLOCK_INPUT].active && globalTriggerClocked) || globalManualClockClicked) {
          incrementInternalStep(i);
          activeKnobIndex[i] = newABS[i].peekWorkingStep();
        }
      }
      if((currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {
        resetOneOfThem(i);
      }

      atFirstStep = (newABS[i].readHead == 0);

      if(globalManualResetClicked || currentManualResetClicked) {
        setChangeImminent(i,true);
        resetOneOfThem(i);
      }
      if( (currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {
        resetOneOfThem(i);
        setChangeImminent(i,false);
      }
      else {
        if(atFirstStep && !currentResetActive && !inputs[GLOBAL_RESET_INPUT].active) {
          //checkIfShouldChange(i);
        }
      }
    }
		if(activeKnobIndex[i] < 0) {
			outputs[TRG_OUTPUT + i].value = 0.f;
		}
		else if(activeKnobIndex[i] < 26) {
      knobRawValue = params[activeKnobIndex[i]].value;
      outputs[TRG_OUTPUT + i].value = mapKnobValue(knobRawValue,i); 
    }
    else if(activeKnobIndex[i] < 52) {
      knobRawValue = inputs[SIGNAL_INPUT + activeKnobIndex[i] - 26].value;
      outputs[TRG_OUTPUT + i].value = knobRawValue;
    }
    else if(activeKnobIndex[i] < 78) {
      outputs[TRG_OUTPUT + i].value = newABS[i].exactFloats[activeKnobIndex[i] - 52];
    }
    else if(activeKnobIndex[i] < 104) {
      outputs[TRG_OUTPUT + i].value = 2.22;
    }
    else {
      outputs[TRG_OUTPUT+i].value = 0.f;
    }
    if(inputs[CLOCK_INPUT + i].active) {
      outputs[FIRST_STEP_OUTPUT + i].value = (currentTriggerIsHigh && atFirstStep) ? 10.f : 0.0f;
    }
    else {
      outputs[FIRST_STEP_OUTPUT + i].value = (globalGateIn && atFirstStep) ? 10.f : 0.0f;
    }
  }
}

void MyTextFieldCookie::onTextChange() {
  module->checkLength(this->rowIndex);
  std::string value = module->textFields[this->rowIndex]->text;
  AbsoluteSequence abs = AbsoluteSequence(value,knobandinputlookup);
  if((!abs.inError) && matchParens(value)) {
    module->setNextAbsoluteSequence(this->rowIndex);
    module->updateDisplayBlink(this->rowIndex);
    module->textFields[this->rowIndex]->inError=false;
  }
  else {
    module->textFields[this->rowIndex]->inError=true;
  }
}
struct WiggleKnobsMenuItem : MenuItem {
  ComputerscareILoveCookies *cookies;
  void onAction(EventAction &e) override {
   cookies->wiggleKnobs();
  }
};
struct RandomizeTextFieldsMenuItem : MenuItem {
  ComputerscareILoveCookies *cookies;
  void onAction(EventAction &e) override {
   srand(time(0));
   cookies->randomizeTextFields();

  }
};


struct ComputerscareILoveCookiesWidget : ModuleWidget {

  double verticalSpacing = 18.4;
  int verticalStart = 24;
  double xStart = 41;
  int index=0;
  int inputindex=0;
  double knobPosX=0.0;
  double knobPosY=0.0;
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

  ComputerscareILoveCookiesWidget(ComputerscareILoveCookies *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareILoveCookiesPanel.svg")));

    for(int i = 0; i < numKnobRows; i++) {
      for(int j = 0; j < numKnobColumns; j++) {
        knobPosX = knobXStart + j*knobRowWidth;
        knobPosY = knobYStart + i*knobColumnHeight + j*2.0;
        index = numKnobColumns*i + j;

        smallLetterDisplay = new SmallLetterDisplay();
        smallLetterDisplay->box.pos = mm2px(Vec(knobPosX+6,knobPosY-2));
        smallLetterDisplay->box.size = Vec(20, 20);
        smallLetterDisplay->value = knoblookup[index];

        addChild(smallLetterDisplay);

        ParamWidget* knob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(knobPosX,knobPosY)), module, ComputerscareILoveCookies::KNOB_PARAM +index,  0.f, 10.0f, 0.0f);   

        module->smallLetterKnobs.push_back(knob);
        addParam(knob);
        
      }
    }
    for(int k = 0; k < numInputRows; k++) {
      for(int m=0; m<numInputColumns; m++) {
        inputPosX = inputXStart + m*inputRowWidth;
        inputPosY = inputYStart + k*inputColumnHeight + m * 2.0;
        inputindex = numInputColumns*k + m;

        if(m%2) {
          addInput(Port::create<InPort>(mm2px(Vec(inputPosX , inputPosY)), Port::INPUT, module, ComputerscareILoveCookies::SIGNAL_INPUT + inputindex));
        }
        else {
          addInput(Port::create<PointingUpPentagonPort>(mm2px(Vec(inputPosX , inputPosY)), Port::INPUT, module, ComputerscareILoveCookies::SIGNAL_INPUT + inputindex));
        }
        smallLetterDisplay = new SmallLetterDisplay();
        smallLetterDisplay->box.pos = mm2px(Vec(inputPosX+6,inputPosY-1));
        smallLetterDisplay->box.size = Vec(20, 20);
        smallLetterDisplay->value = inputlookup[inputindex];

        addChild(smallLetterDisplay);
        //module->smallLetterDisplays[i] = smallLetterDisplay;
      }
    }

    //global clock input
    addInput(Port::create<InPort>(mm2px(Vec(2+xStart , 0)), Port::INPUT, module, ComputerscareILoveCookies::GLOBAL_CLOCK_INPUT));

    //global reset input
    addInput(Port::create<InPort>(mm2px(Vec(12+xStart , 0)), Port::INPUT, module, ComputerscareILoveCookies::GLOBAL_RESET_INPUT));
    addParam(ParamWidget::create<ComputerscareResetButton>(mm2px(Vec(12+xStart , 9)), module, ComputerscareILoveCookies::MANUAL_RESET_PARAM, 0.0, 1.0, 0.0));
    addParam(ParamWidget::create<ComputerscareClockButton>(mm2px(Vec(2+xStart , 9)), module, ComputerscareILoveCookies::MANUAL_CLOCK_PARAM, 0.0, 1.0, 0.0));
 

    for(int i = 0; i < numFields; i++) {
      //first-step output
      addOutput(Port::create<OutPort>(mm2px(Vec(42+xStart , verticalStart + verticalSpacing*i - 11)), Port::OUTPUT, module, ComputerscareILoveCookies::FIRST_STEP_OUTPUT + i));

      //individual output
      addOutput(Port::create<OutPort>(mm2px(Vec(54+xStart , verticalStart + verticalSpacing*i - 11)), Port::OUTPUT, module, ComputerscareILoveCookies::TRG_OUTPUT + i));

      //individual clock input
      addInput(Port::create<InPort>(mm2px(Vec(2+xStart, verticalStart + verticalSpacing*i-10)), Port::INPUT, module, ComputerscareILoveCookies::CLOCK_INPUT + i));

      //individual reset input
      addInput(Port::create<InPort>(mm2px(Vec(12+xStart, verticalStart + verticalSpacing*i-10)), Port::INPUT, module, ComputerscareILoveCookies::RESET_INPUT + i));

      //sequence input field
      textField = Widget::create<MyTextFieldCookie>(mm2px(Vec(1+xStart, verticalStart + verticalSpacing*i)));
      textField->setModule(module);
      textField->box.size = mm2px(Vec(63, 7));
      textField->multiline = false;
      textField->color = nvgRGB(0xC0, 0xE7, 0xDE);
      textField->rowIndex = i;
      addChild(textField);
      module->textFields[i] = textField;

      //active/total steps display
      smallLetterDisplay = new SmallLetterDisplay();
      smallLetterDisplay->box.pos = mm2px(Vec(21+xStart,verticalStart - 9.2 +verticalSpacing*i));
      smallLetterDisplay->box.size = Vec(60, 30);
      smallLetterDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
      smallLetterDisplay->value = "?\n?";
      addChild(smallLetterDisplay);
      module->smallLetterDisplays[i] = smallLetterDisplay;

      //active/total steps display
      currentWorkingStepDisplay = new SmallLetterDisplay();
      currentWorkingStepDisplay->box.pos = mm2px(Vec(11+xStart,verticalStart - 7.0 +verticalSpacing*i));
      currentWorkingStepDisplay->box.size = mm2px(Vec(2,10));
      currentWorkingStepDisplay->fontSize = 26;
      currentWorkingStepDisplay->textAlign = 4;
      currentWorkingStepDisplay->value = "?";
      addChild(currentWorkingStepDisplay);
      module->currentWorkingStepDisplays[i] = currentWorkingStepDisplay;

      addParam(ParamWidget::create<ComputerscareInvisibleButton>(mm2px(Vec(21+xStart,verticalStart - 9.9 +verticalSpacing*i)), module, ComputerscareILoveCookies::INDIVIDUAL_RESET_PARAM + i, 0.0, 1.0, 0.0));
    }
    module->onCreate();
  }
  MyTextFieldCookie* textField;
  SmallLetterDisplay* smallLetterDisplay;
  SmallLetterDisplay* currentWorkingStepDisplay;
  Menu *createContextMenu() override;
};
Menu *ComputerscareILoveCookiesWidget::createContextMenu() {
  Menu *menu = ModuleWidget::createContextMenu();
  ComputerscareILoveCookies *cookies = dynamic_cast<ComputerscareILoveCookies*>(module);
  assert(cookies);

  MenuLabel *spacerLabel = new MenuLabel();
  menu->addChild(spacerLabel);
  
  MenuLabel *modeLabel = new MenuLabel();
  modeLabel->text = "Horseman Optinos";
  menu->addChild(modeLabel);
  
  WiggleKnobsMenuItem *wiggleKnobsMenuItem = new WiggleKnobsMenuItem();
  wiggleKnobsMenuItem->text = "Wiggle Knobs";
  wiggleKnobsMenuItem->cookies = cookies;
  menu->addChild(wiggleKnobsMenuItem);

  RandomizeTextFieldsMenuItem *randomizeTextFieldsMenuItem = new RandomizeTextFieldsMenuItem();
  randomizeTextFieldsMenuItem->text = "Randomize Text Fields";
  randomizeTextFieldsMenuItem->cookies = cookies;
  menu->addChild(randomizeTextFieldsMenuItem);

  

  return menu;
}
Model *modelComputerscareILoveCookies = Model::create<ComputerscareILoveCookies, ComputerscareILoveCookiesWidget>("computerscare", "computerscare-i-love-cookies", "I Love Cookies", SEQUENCER_TAG);
