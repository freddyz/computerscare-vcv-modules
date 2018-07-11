#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>

#define NUM_LINES 16

struct ComputerscareDebug : Module {
	enum ParamIds {
		PITCH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VAL_INPUT,
		TRG_INPUT,
		CLR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SINE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	std::string strValue = "inoot";

	float logLines[NUM_LINES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	int lineCounter = 0;

	float phase = 0.0;
	float blinkPhase = 0.0;

	SchmittTrigger clockTrigger;
	SchmittTrigger clearTrigger;

	ComputerscareDebug() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void ComputerscareDebug::step() {
	std::string thisVal;
	if (clockTrigger.process(inputs[TRG_INPUT].value / 2.f)) {
		//textField->text = inputs[VAL_INPUT].value;
		//std::stringstream ss;
		//ss << "Hello, world, " << myInt << niceToSeeYouString;
		//std::string s = ss.str();
		 for( unsigned int a = NUM_LINES-1; a > 0; a = a - 1 )
		 {
		 	logLines[a] = logLines[a-1];
		     //cout << "value of a: " << texts[a] << endl;
		 }
		logLines[0] = inputs[VAL_INPUT].value;

		thisVal = std::to_string(logLines[0]).substr(0,10);
		for( unsigned int a = 1; a < NUM_LINES; a = a + 1 )
		 {
		 	//logLines[a] = logLines[a-1];
		 	thisVal =  thisVal + "\n" + std::to_string(logLines[a]).substr(0,10);
			//strValue = strValue;
		     //cout << "value of a: " << texts[a] << endl;
		 }

			
		//thisVal = std::to_string(inputs[VAL_INPUT].value).substr(0,10);
		strValue = thisVal;
	}
	if(clearTrigger.process(inputs[CLR_INPUT].value / 2.f)) {
		strValue = "";
	}

}

////////////////////////////////////
struct StringDisplayWidget3 : TransparentWidget {

  std::string *value;
  std::shared_ptr<Font> font;

  StringDisplayWidget3() {
    font = Font::load(assetPlugin(plugin, "res/Oswald-Regular.ttf"));
  };

  void draw(NVGcontext *vg) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x10, 0x00, 0x10);
    NVGcolor StrokeColor = nvgRGB(0xC0, 0xC7, 0xDE);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -1.0, -1.0, box.size.x+2, box.size.y+2, 4.0);
    nvgFillColor(vg, StrokeColor);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(vg, backgroundColor);
    nvgFill(vg);    
    
    // text 
    nvgFontSize(vg, 15);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    std::stringstream to_display;   
    to_display << std::setw(8) << *value;

    Vec textPos = Vec(6.0f, 12.0f);   
    NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
    nvgFillColor(vg, textColor);
   // nvgText(vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
 	nvgTextBox(vg, textPos.x, textPos.y,80,to_display.str().c_str(), NULL);

  }
};


struct ComputerscareDebugWidget : ModuleWidget {

	ComputerscareDebugWidget(ComputerscareDebug *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareDebugVectorGradient.svg")));

		//addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(28, 287), module, ComputerscareDebug::PITCH_PARAM, -3.0, 3.0, 0.0));

		addInput(Port::create<PJ301MPort>(Vec(3, 330), Port::INPUT, module, ComputerscareDebug::TRG_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(33, 330), Port::INPUT, module, ComputerscareDebug::VAL_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(63, 330), Port::INPUT, module, ComputerscareDebug::CLR_INPUT));
	
		StringDisplayWidget3 *display = new StringDisplayWidget3();
		  display->box.pos = Vec(1,24);
		  display->box.size = Vec(88, 250);
		  display->value = &module->strValue;
		  addChild(display);

	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareDebug = Model::create<ComputerscareDebug, ComputerscareDebugWidget>("computerscare", "ComputerscareDebug", "Debug", UTILITY_TAG);
