#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "window.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareLaundrySoup;

const int numFields = 5;

class MyTextField : public LedDisplayTextField {
public:
  MyTextField() : LedDisplayTextField() {}
  void setModule(ComputerscareLaundrySoup* _module) {
    module = _module;
  }
  virtual void onTextChange() override;
  void draw(NVGcontext *vg) override {
    nvgScissor(vg, 0, 0, box.size.x, box.size.y);

    // Background
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
      bndIconLabelCaret(vg, textOffset.x, textOffset.y+2,
        box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
        -1, color, 22, text.c_str(), highlightColor, begin, end);

      bndSetFont(gGuiFont->handle);
    }

    nvgResetScissor(vg);
  };

private:
  ComputerscareLaundrySoup* module;
};

struct ComputerscareLaundrySoup : Module {
	enum ParamIds {
	   NUM_PARAMS
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
		NUM_OUTPUTS = TRG_OUTPUT + numFields
	};
  enum LightIds {
		SWITCH_LIGHTS,
    NUM_LIGHTS
	};

  SchmittTrigger globalClockTrigger;
  SchmittTrigger globalResetTriggerInput;

  SchmittTrigger clockTriggers[numFields];
  SchmittTrigger resetTriggers[numFields];

  MyTextField* textFields[numFields];

  std::vector<int> sequences[numFields];
  std::vector<int> sequenceSums[numFields];

  int stepCity[numFields] = {0};
  int stepState[numFields] = {0};
  int stepCounty[numFields] = {0};
  int numStepStates[numFields] = {0};
  int currentChar = 0;
  int numStepBlocks[numFields] = {0};
  int offsets[numFields] = {0};
  
  bool compiled = false;

ComputerscareLaundrySoup() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
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
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

  void parseFormula(std::string expr, int index) {
    int numSteps = 0;
    
    std::stringstream test(expr);
    std::string segment;
    std::vector<std::string> seglist;

    while(std::getline(test, segment, ','))
    {
       seglist.push_back(segment);
    }

    sequences[index].resize(0);
    sequenceSums[index].resize(0);
    sequenceSums[index].push_back(0);

      for(char& c : seglist[0]) {
        
        currentChar = c - '0';
        numSteps += currentChar;
        sequenceSums[index].push_back(numSteps);
        sequences[index].push_back(currentChar);
        if(seglist.size() > 1) {
          offsets[index] = std::stoi( seglist[1] );
        }
        else {
          offsets[index] = 0;
        }
      }
      numStepStates[index] = numSteps;
      numStepBlocks[index] = sequences[index].size();
  }

void onCreate () override
  {
    for(int i = 0; i < numFields; i++) {
      if(textFields[i]->text.size() > 0) {
        parseFormula(textFields[i]->text,i);
      }
    }
    compiled = true;
  }

  void onReset () override
  {
    for(int i = 0; i < numFields; i++) {
      resetOneOfThem(i);
    }
    onCreate();
  }

  /*
  lets say the sequence "332" is entered in the 0th (first)
  numStepBlocks[0] would then be 8 (3 + 3 + 2)
  sequences[0] would be the vector (3,3,2)
  stepState[0] will go from 0 to 7
  stepCounty[0] will go from 0 to 2
  stepCity[0] will count for each "inner" step ie: 0 to 2, 0 to 2, and then 0 to 1

  */
  void incrementInternalStep(int i) {
    this->stepCity[i] += 1;
    this->stepState[i] += 1;
    if(this->stepCity[i] >= sequences[i][this->stepCounty[i]]) {
      this->stepCity[i] = 0;
      this->stepCounty[i] += 1;
    }
    if(this->stepCounty[i] >= this->numStepBlocks[i]) {
      this->stepCounty[i] = 0;
      this->stepCity[i] = 0;
      this->stepState[i] = 0;
    }
  }

  void resetOneOfThem(int i) {
    this->stepCity[i] = 0;
    this->stepState[i] = 0;
    this->stepCounty[i] = 0;
  }
};



void ComputerscareLaundrySoup::step() {

  bool globalGateIn = globalClockTrigger.isHigh();
  bool activeStep = false;
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

    if(this->numStepBlocks[i] > 0) {
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

      activeStep = (sequenceSums[i][this->stepCounty[i]] == (this->stepState[i] + this->offsets[i]) % this->numStepStates[i]);
    }
    if(inputs[CLOCK_INPUT + i].active) {
      outputs[TRG_OUTPUT + i].value = (currentTriggerIsHigh && activeStep) ? 10.0f : 0.0f;
    }
    else {
      outputs[TRG_OUTPUT + i].value = (globalGateIn && activeStep) ? 10.0f : 0.0f;
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

void MyTextField::onTextChange() {
  module->onCreate();
}

struct ComputerscareLaundrySoupWidget : ModuleWidget {

  int verticalSpacing = 22;
  int verticalStart = 23;
  ComputerscareLaundrySoupWidget(ComputerscareLaundrySoup *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareLaundrySoupPanel.svg")));

    //clock input
    addInput(Port::create<InPort>(mm2px(Vec(2 , 2)), Port::INPUT, module, ComputerscareLaundrySoup::GLOBAL_CLOCK_INPUT));

    //reset input
    addInput(Port::create<InPort>(mm2px(Vec(12 , 2)), Port::INPUT, module, ComputerscareLaundrySoup::GLOBAL_RESET_INPUT));
    
    for(int i = 0; i < numFields; i++) {
      addOutput(Port::create<InPort>(mm2px(Vec(55 , verticalStart + verticalSpacing*i - 10)), Port::OUTPUT, module, ComputerscareLaundrySoup::TRG_OUTPUT + i));

      addInput(Port::create<InPort>(mm2px(Vec(2, verticalStart + verticalSpacing*i-10)), Port::INPUT, module, ComputerscareLaundrySoup::CLOCK_INPUT + i));

      addInput(Port::create<InPort>(mm2px(Vec(12, verticalStart + verticalSpacing*i-10)), Port::INPUT, module, ComputerscareLaundrySoup::RESET_INPUT + i));


      textField = Widget::create<MyTextField>(mm2px(Vec(1, verticalStart + verticalSpacing*i)));
      textField->setModule(module);
      textField->box.size = mm2px(Vec(63, 10));
      textField->multiline = false;
      textField->color = nvgRGB(0xC0, 0xE7, 0xDE);
      addChild(textField);
      module->textFields[i] = textField;

        //active step display
      NumberDisplayWidget3 *display = new NumberDisplayWidget3();
      display->box.pos = mm2px(Vec(25,verticalStart - 9.2 +verticalSpacing*i));
      display->box.size = Vec(50, 20);
      if(&module->numStepBlocks[i]) {
        display->value = &module->stepState[i];
      }
      else {
        display->value = 0;
      }
      addChild(display);
    }

  }
  MyTextField* textField;
};

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareLaundrySoup = Model::create<ComputerscareLaundrySoup, ComputerscareLaundrySoupWidget>("computerscare", "computerscare-laundry-soup", "Laundry Soup", SEQUENCER_TAG);
