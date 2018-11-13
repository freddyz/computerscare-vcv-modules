#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "window.hpp"
#include "dtpulse.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareILoveCookies;

const int numFields = 3;
const int numKnobRows = 13;
const int numKnobColumns = 2;
const int numInputRows = 13;
const int numInputColumns = 2;

const int numKnobs = numKnobRows * numKnobColumns;
const int numInputs = numInputRows * numInputColumns;

const std::string knoblookup = "abcdefghijklmnopqrstuvwxyz";
const std::string inputlookup= "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const std::string knobandinputlookup = knoblookup + inputlookup;
const std::vector<NVGcolor> outlineColorMap = {COLOR_COMPUTERSCARE_RED,COLOR_COMPUTERSCARE_YELLOW,COLOR_COMPUTERSCARE_BLUE};

class MyTextFieldCookie : public LedDisplayTextField {

public:
  int fontSize = 15;
  int rowIndex=0;
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
    nvgFillColor(vg, nvgRGB(0x00, 0x00, 0x00));
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
	   NUM_PARAMS = KNOB_PARAM + numKnobs
	};  
	enum InputIds {
    GLOBAL_CLOCK_INPUT,
    GLOBAL_RESET_INPUT,
    CLOCK_INPUT,
    RESET_INPUT = CLOCK_INPUT + numFields,
    SIGNAL_INPUT = RESET_INPUT + numFields ,
		NUM_INPUTS = RESET_INPUT+ numInputs
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

  SchmittTrigger clockTriggers[numFields];
  SchmittTrigger resetTriggers[numFields];

  MyTextFieldCookie* textFields[numFields];

  std::vector<int> absoluteSequences[numFields];
  std::vector<int> nextAbsoluteSequences[numFields];

  bool shouldChange[numFields] = {false};

  int absoluteStep[numFields] = {0};
  int activeKnobIndex[numFields] = {0};
  int numSteps[numFields] = {0};

 // SmallLetterDisplay* smallLetterDisplays[numKnobs + numInputs];

 
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
    randomizeAllFields();
  }

  void randomizeAllFields() {
    std::string mainlookup = knoblookup;
    std::string string = "";
    std::string randchar = "";
    int length = 0;

    for (int i = 0; i < numFields; i++) {
      length = rand() % 12 + 2;
      string = "";
      for(int j = 0; j < length; j++) {
        randchar = mainlookup[rand() % mainlookup.size()];
        string = string + randchar;
      }
      textFields[i]->text = string;
      setNextAbsoluteSequence(i);
    }
  }

  void setNextAbsoluteSequence(int index) {
    shouldChange[index] = true;
    nextAbsoluteSequences[index].resize(0);
    nextAbsoluteSequences[index]  = parseStringAsValues(textFields[index]->text,knobandinputlookup);
    printf("index:%i,val[0]:%i\n",index,nextAbsoluteSequences[index][0]);
  }
  void setAbsoluteSequenceFromQueue(int index) {
    absoluteSequences[index].resize(0);
    absoluteSequences[index] = nextAbsoluteSequences[index];
    numSteps[index] = nextAbsoluteSequences[index].size() > 0 ? nextAbsoluteSequences[index].size() : 1;
  }
  void checkIfShouldChange(int index) {
    if(shouldChange[index]) {
      setAbsoluteSequenceFromQueue(index);
      shouldChange[index] = false;
    }
  }
  int getAbsoluteStep(int index) {
    return this->absoluteStep[index];
  }
  int getCurrentStep(int index) {
    return absoluteSequences[index][getAbsoluteStep(index)];
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

  /*
  lets say the sequence "332" is entered in the 0th (first)
  numSteps[0] would then be 8 (3 + 3 + 2)
  absoluteSequences[0] would be the vector (1,0,0,1,0,0,1,0)
  absoluteStep[0] would run from 0 to 7

  332-4 (332 offset by 4)

  */
  void incrementInternalStep(int i) {
    this->absoluteStep[i] +=1;
    this->absoluteStep[i] %= this->numSteps[i];
    printf("row:%i, step:%i, val:%i\n",i,this->absoluteStep[i],this->absoluteSequences[i][this->absoluteStep[i]]);
  }

  void resetOneOfThem(int i) {
    this->absoluteStep[i] = 0;
  }
	float mapKnobValue(float rawValue, int rowIndex) {
		// raw value is between -10 and +10
		/*
		0:	-10,10
		1:	-5,5
		2:	0,10
		3:	0,5
		4:	0,1
		*/
		float mappedValue = 0.f;
		int mapEnum = 0;
		switch(mapEnum) {
			case 0: mappedValue = rawValue; break;
			case 1: mappedValue = rawValue / 2.f; break;
			case 2: mappedValue = (rawValue + 10.f) / 2.f; break;
			case 3: mappedValue = (rawValue + 10.f) / 4.f; break;
			case 4: mappedValue = (rawValue + 10.f) / 20.f; break;
		}
		return mappedValue;
	}
};


void ComputerscareILoveCookies::step() {

  bool globalGateIn = globalClockTrigger.isHigh();
  bool activeStep = 0;
  bool atFirstStep = false;
  bool clocked = globalClockTrigger.process(inputs[GLOBAL_CLOCK_INPUT].value);
  bool currentTriggerIsHigh;
  bool currentTriggerClocked;
  bool globalResetTriggered = globalResetTriggerInput.process(inputs[GLOBAL_RESET_INPUT].value / 2.f);
  bool currentResetActive;
  bool currentResetTriggered;
	float knobRawValue = 0.f;
  for(int i = 0; i < numFields; i++) {
    activeStep = false;
    currentResetActive = inputs[RESET_INPUT + i].active;
    currentResetTriggered = resetTriggers[i].process(inputs[RESET_INPUT+i].value / 2.f);
    currentTriggerIsHigh = clockTriggers[i].isHigh();
    currentTriggerClocked = clockTriggers[i].process(inputs[CLOCK_INPUT + i].value);

    if(this->numSteps[i] > 0) {
      if (inputs[CLOCK_INPUT + i].active) {
        if(currentTriggerClocked) {
          incrementInternalStep(i);
        }
      }
      else {
        if (inputs[GLOBAL_CLOCK_INPUT].active && clocked) {
          incrementInternalStep(i);   
        }
      }
      if((currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {
        resetOneOfThem(i);
      }

      atFirstStep = (this->absoluteStep[i] == 0);

      if((currentResetActive && currentResetTriggered) || (!currentResetActive && globalResetTriggered)) {
        checkIfShouldChange(i);
        resetOneOfThem(i);
      }
      else {
        if(atFirstStep && !currentResetActive && !inputs[GLOBAL_RESET_INPUT].active) {
          checkIfShouldChange(i);
        }
      }

      activeKnobIndex[i] = absoluteSequences[i][this->absoluteStep[i]];

      atFirstStep = (this->absoluteStep[i] == 0);

      for(int k = 0; k < (numKnobs + numInputs); k++) {
        //params[SIGNAL_INPUT + k].backgroundColor = (k==2) ? COLOR_COMPUTERSCARE_LIGHT_GREEN : COLOR_COMPUTERSCARE_TRANSPARENT;
       // lights[SWITCH_LIGHTS + i].value  = (k==activeKnob) ? 1.0 : 0.0;
      //currentActives[]
      }
    }
    //outputs[TRG_OUTPUT + i].value = params[KNOB_PARAM + activeKnob].value;
    if(activeKnobIndex[i] < 25) {
      knobRawValue = params[SIGNAL_INPUT + activeKnobIndex[i]].value;
    }
    else {
      knobRawValue = inputs[SIGNAL_INPUT + activeKnobIndex[i] - 25].value;
    }

    outputs[TRG_OUTPUT + i].value =	mapKnobValue(knobRawValue,i); 
    if(inputs[CLOCK_INPUT + i].active) {
      outputs[FIRST_STEP_OUTPUT + i].value = (currentTriggerIsHigh && atFirstStep) ? 10.f : 0.0f;
    }
    else {
      outputs[FIRST_STEP_OUTPUT + i].value = (globalGateIn && atFirstStep) ? 10.f : 0.0f;
    }
  }
}

/////////////////////////////////////////////////
struct NumberDisplayWidget3cookie : TransparentWidget {

  int *value;
  std::shared_ptr<Font> font;
  NVGcolor outlineColor;
  //NVGcolor circleColor;

  NumberDisplayWidget3cookie() {
    font = Font::load(assetPlugin(plugin, "res/digital-7.ttf"));
  };

  void draw(NVGcontext *vg) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x00, 0x00, 0x00);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, -2, -4, box.size.x+4, box.size.y+8, 4.0);
    nvgFillColor(vg, outlineColor);
    nvgFill(vg);    

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 8.0);
    nvgFillColor(vg, backgroundColor);
    nvgFill(vg);    
    
    // text 
    nvgFontSize(vg, 13);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    std::stringstream to_display;   
    to_display << std::setw(3) << *value;

    Vec textPos = Vec(6.0f, 17.0f);   
    NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
    nvgFillColor(vg, textColor);
    nvgText(vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
  }
};
////////////////////////////////////
struct SmallLetterDisplay : TransparentWidget {

  std::string value;
  std::shared_ptr<Font> font;
  bool active = false;

  SmallLetterDisplay() {
    font = Font::load(assetPlugin(plugin, "res/Oswald-Regular.ttf"));
  };

  void draw(NVGcontext *vg) override
  {  
    // Background
    NVGcolor backgroundColor = nvgRGB(0xC0, 0xE7, 0xDE);

    if(active) {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, -1.0, -1.0, box.size.x-3, box.size.y-3, 8.0);
      nvgFillColor(vg, backgroundColor);
      nvgFill(vg);
    }

    // text 
    nvgFontSize(vg, 19);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    Vec textPos = Vec(6.0f, 12.0f);   
    NVGcolor textColor = nvgRGB(0x10, 0x10, 0x00);
    nvgFillColor(vg, textColor);
    nvgTextBox(vg, textPos.x, textPos.y,80,value.c_str(), NULL);

  }
};
void MyTextFieldCookie::onTextChange() {
  module->setNextAbsoluteSequence(this->rowIndex);
}

struct ComputerscareILoveCookiesWidget : ModuleWidget {

  double verticalSpacing = 18.4;
  int verticalStart = 80;
  double xStart = 41;
  int index=0;
  int inputindex=0;
  double knobPosX=0.0;
  double knobPosY=0.0;
  double knobXStart = 20;
  double knobYStart = 2;
  double knobRowWidth = 11;
  double knobColumnHeight = 9.2;

  double inputPosX = 0.0;
  double inputPosY = 0.0;
  double inputXStart = 0;
  double inputYStart = 0;
  double inputRowWidth = 8;
  double inputColumnHeight = 9.7;

  //SmallLetterDisplay smallLetterDisplays[numKnobs + numInputs];
 
  ComputerscareILoveCookiesWidget(ComputerscareILoveCookies *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareILoveCookiesPanel.svg")));


    for(int i = 0; i < numKnobRows; i++) {
      for(int j = 0; j < numKnobColumns; j++) {
        knobPosX = knobXStart + j*knobRowWidth;
        knobPosY = knobYStart + i*knobColumnHeight + j*2.0;
        index = numKnobColumns*i + j;

       // addChild(ModuleLightWidget::create<ComputerscareMediumLight<ComputerscareRedLight>>(mm2px(Vec(knobPosX-3, knobPosY - 2)), module, ComputerscareILoveCookies::SWITCH_LIGHTS  + index));
        //addChild(ModuleLightWidget::create<ComputerscareMediumLight<ComputerscareYellowLight>>(mm2px(Vec(knobPosX-3, knobPosY )), module, ComputerscareILoveCookies::SWITCH_LIGHTS  + index + numKnobColumns*numKnobRows));
        //addChild(ModuleLightWidget::create<ComputerscareMediumLight<ComputerscareBlueLight>>(mm2px(Vec(knobPosX-3, knobPosY +2)), module, ComputerscareILoveCookies::SWITCH_LIGHTS  + index + numKnobColumns*numKnobRows*2));

        smallLetterDisplay = new SmallLetterDisplay();
        smallLetterDisplay->box.pos = mm2px(Vec(knobPosX+6,knobPosY-2));
        smallLetterDisplay->box.size = Vec(20, 20);
        smallLetterDisplay->value = knoblookup[index];

        addChild(smallLetterDisplay);
        //smallLetterDisplays.push_back(letterDisplay);


        ParamWidget* knob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(knobPosX,knobPosY)), module, ComputerscareILoveCookies::KNOB_PARAM +index,  0.f, 10.0f, 0.0f);
        

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

      //active step display
      NumberDisplayWidget3cookie *display = new NumberDisplayWidget3cookie();
      display->box.pos = mm2px(Vec(23+xStart,verticalStart - 9.2 +verticalSpacing*i));
      display->box.size = Vec(50, 20);

      display->outlineColor = outlineColorMap[i];
      if(&module->numSteps[i]) {
        display->value = &module->absoluteStep[i];
      }
      else {
        display->value = 0;
      }
      addChild(display);
    }
    module->onCreate();
  }
  MyTextFieldCookie* textField;
  SmallLetterDisplay* smallLetterDisplay;

};

Model *modelComputerscareILoveCookies = Model::create<ComputerscareILoveCookies, ComputerscareILoveCookiesWidget>("computerscare", "computerscare-i-love-cookies", "I Love Cookies", SEQUENCER_TAG);
