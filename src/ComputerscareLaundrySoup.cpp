#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareLaundrySoup;

const int numFields = 2;

class MyTextField : public LedDisplayTextField {
public:
  MyTextField() : LedDisplayTextField() {}
  void setModule(ComputerscareLaundrySoup* _module) {
    module = _module;
  }
  virtual void onTextChange() override;

private:
  ComputerscareLaundrySoup* module;
};

struct ComputerscareLaundrySoup : Module {
	enum ParamIds {
	   NUM_PARAMS
	};  
	enum InputIds {
    CLOCK_INPUT,
    RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { 
    TRG_OUTPUT, 
		NUM_OUTPUTS = TRG_OUTPUT + numFields
	};
  enum LightIds {
		SWITCH_LIGHTS,
    NUM_LIGHTS
	};

  SchmittTrigger clockTrigger;
  SchmittTrigger resetTriggerInput;

  MyTextField* textFields[numFields];

  std::vector<int> sequences[numFields];
  std::vector<int> sequenceSums[numFields];

  int stepCity = 0;
  int stepState = 0;
  int stepCounty = 0;
  int currentChar = 0;
  int numSteps = 0;
  int numStepBlocks[numFields];
  
  bool compiled = false;

ComputerscareLaundrySoup() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;



	json_t *toJson() override
  {
		json_t *rootJ = json_object();
    
    return rootJ;
  } 
  
  void fromJson(json_t *rootJ) override
  {

  }
 
	void onRandomize() override {
		
	}
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

  
void parseFormulas(MyTextField* textFields) {
  std::string expr;
  for(int i = 0; i < numFields; i++) {
    sequences[i].resize(0);
    sequenceSums[i].resize(0);
    sequenceSums[i].push_back(0);
    numSteps = 0;
    //expr = textFields[i]->text;
      for(char& c : expr) {
        
        //do_things_with(c);
        currentChar = c - '0';
        numSteps += currentChar;
        sequenceSums[i].push_back(numSteps);
        sequences[i].push_back(currentChar);
      }
      numStepBlocks[i] = sequences[i].size();
    }
  }
  void parseFormula(std::string expr, int index) {
    sequences[index].resize(0);
    sequenceSums[index].resize(0);
    sequenceSums[index].push_back(0);
    numSteps = 0;
    //expr = textFields[i]->text;
      for(char& c : expr) {
        
        //do_things_with(c);
        currentChar = c - '0';
        numSteps += currentChar;
        sequenceSums[index].push_back(numSteps);
        sequences[index].push_back(currentChar);
      }
      numStepBlocks[index] = sequences[index].size();
  }

void onCreate () override
  {
    for(int i = 0; i < numFields; i++) {
      if(textFields[i]->text.size() > 0) {
        parseFormula(textFields[i]->text,i);
      }
    }
    compiled = false;
    /*
    if (textFields[0]->text.size() > 0) {
        parseFormulas(textFields);
        compiled = true;
    }*/
  }

  void onReset () override
  {
    onCreate();
  }
  void incrementInternalStep() {
    this->stepCity += 1;
    this->stepState += 1;
    if(this->stepCity >= sequences[0][this->stepCounty]) {
      this->stepCity = 0;
      this->stepCounty += 1;
    }
    if(this->stepCounty >= this->numStepBlocks[0]) {
      this->stepCounty = 0;
      this->stepCity = 0;
      this->stepState = 0;
    }
  }

};



void ComputerscareLaundrySoup::step() {
  // fun
bool gateIn = false;
bool activeStep = false;

if(this->numStepBlocks[0] > 0) {
      if (inputs[CLOCK_INPUT].active) {
        // External clock
        if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
          incrementInternalStep();
        }
        gateIn = clockTrigger.isHigh();
      }
      activeStep = (sequenceSums[0][this->stepCounty] == this->stepState);

}
  // 112
  // [0,1,2]
  outputs[TRG_OUTPUT].value = (gateIn && activeStep) ? 10.0f : 0.0f;
}

////////////////////////////////////
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

  ComputerscareLaundrySoupWidget(ComputerscareLaundrySoup *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareLaundrySoupPanel.svg")));

    //clock input
  addInput(Port::create<InPort>(Vec(14, 13), Port::INPUT, module, ComputerscareLaundrySoup::CLOCK_INPUT));

  //reset input
  addInput(Port::create<InPort>(Vec(54, 13), Port::INPUT, module, ComputerscareLaundrySoup::RESET_INPUT));
  
  for(int i = 0; i < numFields; i++) {
    addOutput(Port::create<InPort>(Vec(33 , 130 + 100*i), Port::OUTPUT, module, ComputerscareLaundrySoup::TRG_OUTPUT + i));


    textField = Widget::create<MyTextField>(mm2px(Vec(1, 25 + 30*i)));
    textField->setModule(module);
    textField->box.size = mm2px(Vec(63, 20));
    textField->multiline = true;
    addChild(textField);
    module->textFields[i] = textField;
  }
  //active step display
  NumberDisplayWidget3 *display = new NumberDisplayWidget3();
  display->box.pos = Vec(56,40);
  display->box.size = Vec(50, 20);
  display->value = &module->stepState;
  addChild(display);

  }
  MyTextField* textField;
};

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareLaundrySoup = Model::create<ComputerscareLaundrySoup, ComputerscareLaundrySoupWidget>("computerscare", "computerscare-laundry-soup", "Laundry Soup", SEQUENCER_TAG);
