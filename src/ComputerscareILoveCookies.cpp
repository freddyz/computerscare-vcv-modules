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
const int numKnobRows = 5;
const int numKnobColumns = 5;
const std::string knoblookup = "abcdefghijklmnopqrstuvwxy";

class MyTextFieldCookie : public LedDisplayTextField {

public:
  int fontSize = 18;
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
	   NUM_PARAMS = KNOB_PARAM + numKnobRows * numKnobColumns
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

  SchmittTrigger globalClockTrigger;
  SchmittTrigger globalResetTriggerInput;

  SchmittTrigger clockTriggers[numFields];
  SchmittTrigger resetTriggers[numFields];

  MyTextFieldCookie* textFields[numFields];

  std::vector<int> absoluteSequences[numFields];

  int absoluteStep[numFields] = {0};
  int numSteps[numFields] = {0};
  
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
    std::string mainlookup =knoblookup;
    std::string string = "";
    std::string randchar = "";
    int length = 0;

    for (int i = 0; i < numFields; i++) {
      length = rand() % 12 + 1;
      string = "";
      for(int j = 0; j < length; j++) {
        randchar = mainlookup[rand() % mainlookup.size()];
        string = string + randchar;
      }
      textFields[i]->text = string;
    }
    onCreate();

  }

  void parseFormula(std::string input, int index) {
    std::vector<int> absoluteSequence;
    int currentVal;
    absoluteSequence.resize(0);

    for(unsigned int i = 0; i < input.length(); i++) {
      currentVal = knoblookup.find(input[i]);
      absoluteSequence.push_back(currentVal);
    }

    numSteps[index] = absoluteSequence.size();
    absoluteSequences[index] = absoluteSequence;
  }

void onCreate () override
  {
    for(int i = 0; i < numFields; i++) {
      if(textFields[i]->text.size() > 0) {
        parseFormula(textFields[i]->text,i);
      }
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
  }

  void resetOneOfThem(int i) {
    this->absoluteStep[i] = 0;
  }
};


void ComputerscareILoveCookies::step() {

  bool globalGateIn = globalClockTrigger.isHigh();
  bool activeStep = 0;
  int activeKnob;
  bool atFirstStep = false;
  bool clocked = globalClockTrigger.process(inputs[GLOBAL_CLOCK_INPUT].value);
  bool currentTriggerIsHigh;
  bool currentTriggerClocked;
  bool globalResetTriggered = globalResetTriggerInput.process(inputs[GLOBAL_RESET_INPUT].value / 2.f);
  bool currentResetActive;
  bool currentResetTriggered;

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

      activeKnob = absoluteSequences[i][this->absoluteStep[i]];
      //printf("%i, %f",i,activeKnob);

      atFirstStep = (this->absoluteStep[i] == 0);
    }
    if(inputs[CLOCK_INPUT + i].active) {
      outputs[TRG_OUTPUT + i].value = params[KNOB_PARAM + activeKnob].value;
      
      outputs[FIRST_STEP_OUTPUT + i].value = (currentTriggerIsHigh && atFirstStep) ? 10.f : 0.0f;
    }
    else {
      outputs[TRG_OUTPUT + i].value = params[KNOB_PARAM + activeKnob].value;
      outputs[FIRST_STEP_OUTPUT + i].value = (globalGateIn && atFirstStep) ? 10.f : 0.0f;
    }
  }
}

/////////////////////////////////////////////////
struct NumberDisplayWidget3 : TransparentWidget {

  int *value;
  std::shared_ptr<Font> font;

  NumberDisplayWidget3() {
    font = Font::load(assetPlugin(plugin, "res/digital-7.ttf"));
  };

  void draw(NVGcontext *vg) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x00, 0x00, 0x00);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
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

void MyTextFieldCookie::onTextChange() {
  module->onCreate();
}

struct ComputerscareILoveCookiesWidget : ModuleWidget {

  double verticalSpacing = 18.4;
  int verticalStart = 80;

  double knobXStart = 6;
  double knobYStart = 8;
  double knobRowWidth = 12;
  double knobColumnHeight = 9;
  ComputerscareILoveCookiesWidget(ComputerscareILoveCookies *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareILoveCookiesPanel.svg")));


    for(int i = 0; i < numKnobRows; i++) {
      for(int j = 0; j < numKnobColumns; j++) {
        ParamWidget* knob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(knobXStart + j*knobRowWidth,knobYStart + i*knobColumnHeight)), module, ComputerscareILoveCookies::KNOB_PARAM +numKnobColumns*i + j,  -10.0f, 10.0f, 0.0f);
        addParam(knob);
      }
    }

    //global clock input
    addInput(Port::create<InPort>(mm2px(Vec(2 , 59)), Port::INPUT, module, ComputerscareILoveCookies::GLOBAL_CLOCK_INPUT));

    //global reset input
    addInput(Port::create<InPort>(mm2px(Vec(12 , 59)), Port::INPUT, module, ComputerscareILoveCookies::GLOBAL_RESET_INPUT));
    
    for(int i = 0; i < numFields; i++) {
      //first-step output
      addOutput(Port::create<OutPort>(mm2px(Vec(42 , verticalStart + verticalSpacing*i - 11)), Port::OUTPUT, module, ComputerscareILoveCookies::FIRST_STEP_OUTPUT + i));

      //individual output
      addOutput(Port::create<OutPort>(mm2px(Vec(54 , verticalStart + verticalSpacing*i - 11)), Port::OUTPUT, module, ComputerscareILoveCookies::TRG_OUTPUT + i));

      //individual clock input
      addInput(Port::create<InPort>(mm2px(Vec(2, verticalStart + verticalSpacing*i-10)), Port::INPUT, module, ComputerscareILoveCookies::CLOCK_INPUT + i));

      //individual reset input
      addInput(Port::create<InPort>(mm2px(Vec(12, verticalStart + verticalSpacing*i-10)), Port::INPUT, module, ComputerscareILoveCookies::RESET_INPUT + i));

      //sequence input field
      textField = Widget::create<MyTextFieldCookie>(mm2px(Vec(1, verticalStart + verticalSpacing*i)));
      textField->setModule(module);
      textField->box.size = mm2px(Vec(63, 7));
      textField->multiline = false;
      textField->color = nvgRGB(0xC0, 0xE7, 0xDE);
      addChild(textField);
      module->textFields[i] = textField;

      //active step display
      NumberDisplayWidget3 *display = new NumberDisplayWidget3();
      display->box.pos = mm2px(Vec(24,verticalStart - 9.2 +verticalSpacing*i));
      display->box.size = Vec(50, 20);
      if(&module->numSteps[i]) {
        display->value = &module->absoluteStep[i];
      }
      else {
        display->value = 0;
      }
      addChild(display);
    }

  }
  MyTextFieldCookie* textField;
};

Model *modelComputerscareILoveCookies = Model::create<ComputerscareILoveCookies, ComputerscareILoveCookiesWidget>("computerscare", "computerscare-i-love-cookies", "I Love Cookies", SEQUENCER_TAG);
