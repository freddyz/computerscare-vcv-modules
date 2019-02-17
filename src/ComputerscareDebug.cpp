#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>

#define NUM_LINES 16
struct ComputerscareDebug;


struct ComputerscareDebug : Module {
	enum ParamIds {
		MANUAL_TRIGGER,
		MANUAL_CLEAR_TRIGGER,
		CHANNEL_FOCUS,
		SWITCH_VIEW,
		NUM_PARAMS
	};
	enum InputIds {
		VAL_INPUT,
		TRG_INPUT,
		CLR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	std::string defaultStrValue = "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n";
	std::string strValue = "0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n";

	float logLines[NUM_LINES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	int lineCounter = 0;

	int inputChannel = 0;

	int stepCounter = 0;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger clearTrigger;
	dsp::SchmittTrigger manualClockTrigger;
  	dsp::SchmittTrigger manualClearTrigger;

  	//StringDisplayWidget3* textDisplay;

	ComputerscareDebug() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[MANUAL_TRIGGER].config(0.0f, 1.0f, 0.0f);
		params[MANUAL_CLEAR_TRIGGER].config(0.0f, 1.0f, 0.0f);
		params[SWITCH_VIEW].config(0.0f, 1.0f, 0.0f);
		params[CHANNEL_FOCUS].config(0.f,15.f,0.f);
		outputs[POLY_OUTPUT].setChannels(16);

	}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu



};

void ComputerscareDebug::step() {
	std::string thisVal;
	bool polyViewMode = params[SWITCH_VIEW].value < 0.5;
		if (clockTrigger.process(inputs[TRG_INPUT].getVoltage() / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].value) || polyViewMode) {
			if(polyViewMode) {
				for(int i = 0; i < 16; i++) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
			}
			else {
				 for( unsigned int a = NUM_LINES-1; a > 0; a = a - 1 )
				 {
				 	logLines[a] = logLines[a-1];
				 }

			inputChannel = floor(params[CHANNEL_FOCUS].value);
			logLines[0] = inputs[VAL_INPUT].getVoltage(inputChannel);
			}
			//thisVal = std::to_string(logLines[0]).substr(0,10);
			//outputs[POLY_OUTPUT].setVoltage(logLines[0],0);
			
			
		}
	if(clearTrigger.process(inputs[CLR_INPUT].getVoltage() / 2.f) || manualClearTrigger.process(params[MANUAL_CLEAR_TRIGGER].value)) {
		for( unsigned int a = 0; a < NUM_LINES; a++ )
		 {
		 	logLines[a] = 0;
		 }
		strValue = defaultStrValue;
	}
	stepCounter++;
	if(stepCounter > 1025) {
		stepCounter = 0;

	thisVal = "";
			for( unsigned int a = 0; a < NUM_LINES; a = a + 1 )
			 {
			 	thisVal +=  a > 0 ? "\n" : "";
			 	thisVal+=logLines[a] >=0 ? "+" : "";
			 	thisVal+= std::to_string(logLines[a]).substr(0,10);
			 	outputs[POLY_OUTPUT].setVoltage(logLines[a],a);
			 }
			strValue = thisVal;
			}


}
////////////////////////////////////
struct StringDisplayWidget3 : Widget {

  std::string value;
  std::shared_ptr<Font> font;
  ComputerscareDebug *module;

  StringDisplayWidget3() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Oswald-Regular.ttf"));
  };

  void draw(const DrawArgs &ctx) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x10, 0x00, 0x10);
    NVGcolor StrokeColor = nvgRGB(0xC0, 0xC7, 0xDE);
    nvgBeginPath(ctx.vg);
    nvgRoundedRect(ctx.vg, -1.0, -1.0, box.size.x+2, box.size.y+2, 4.0);
    nvgFillColor(ctx.vg, StrokeColor);
    nvgFill(ctx.vg);
    nvgBeginPath(ctx.vg);
    nvgRoundedRect(ctx.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(ctx.vg, backgroundColor);
    nvgFill(ctx.vg);    

    
    nvgFontSize(ctx.vg, 15);
    nvgFontFaceId(ctx.vg, font->handle);
    nvgTextLetterSpacing(ctx.vg, 2.5);

    std::string textToDraw = module ? module->strValue : "";
    Vec textPos = Vec(6.0f, 12.0f);   
    NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
    nvgFillColor(ctx.vg, textColor);

 	nvgTextBox(ctx.vg, textPos.x, textPos.y,80,textToDraw.c_str(), NULL);

  }
};

struct ComputerscareDebugWidget : ModuleWidget {

	ComputerscareDebugWidget(ComputerscareDebug *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareDebugPanel.svg")));

		addInput(createInput<InPort>(Vec(3, 330), module, ComputerscareDebug::TRG_INPUT));
		addInput(createInput<InPort>(Vec(33, 330),  module, ComputerscareDebug::VAL_INPUT));
		addInput(createInput<InPort>(Vec(63, 330), module, ComputerscareDebug::CLR_INPUT));
	
		

		addParam(createParam<ComputerscareClockButton>(Vec(2, 315), module, ComputerscareDebug::MANUAL_TRIGGER));
		

		addParam(createParam<ComputerscareResetButton>(Vec(62, 315), module, ComputerscareDebug::MANUAL_CLEAR_TRIGGER));
  		


  		addParam(createParam<MediumSnapKnob>(Vec(36,290),module,ComputerscareDebug::CHANNEL_FOCUS));
		addOutput(createOutput<OutPort>(Vec(57, 1), module, ComputerscareDebug::POLY_OUTPUT));
		
		StringDisplayWidget3 *stringDisplay = createWidget<StringDisplayWidget3>(Vec(11,34));
		stringDisplay->box.size = Vec(77, 245);
		stringDisplay->module = module;
		addChild(stringDisplay);

		addParam(createParam<IsoButton>(Vec(4,279),module,ComputerscareDebug::SWITCH_VIEW));
	

	}
};


Model *modelComputerscareDebug = createModel<ComputerscareDebug, ComputerscareDebugWidget>("Debug");
