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
		CLOCK_CHANNEL_FOCUS,
		INPUT_CHANNEL_FOCUS,
		SWITCH_VIEW,
		WHICH_CLOCK,
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

	float logLines[NUM_LINES] = {0.f};

	int lineCounter = 0;

	int clockChannel = 0;
	int inputChannel = 0;

	int clockMode = 0;
	int inputMode = 0;

	int stepCounter = 0;
	dsp::SchmittTrigger clockTriggers[NUM_LINES];
	dsp::SchmittTrigger clearTrigger;
	dsp::SchmittTrigger manualClockTrigger;
  	dsp::SchmittTrigger manualClearTrigger;

  	enum clockAndInputModes {
  		SINGLE_MODE,
  		INTERNAL_MODE,
  		POLY_MODE
  	};
  	//StringDisplayWidget3* textDisplay;

	ComputerscareDebug() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		params[MANUAL_TRIGGER].config(0.0f, 1.0f, 0.0f, "Manual Trigger");
		params[MANUAL_CLEAR_TRIGGER].config(0.0f, 1.0f, 0.0f, "Clear");
		params[SWITCH_VIEW].config(0.0f, 2.0f, 0.0f, "Input Mode");
		params[WHICH_CLOCK].config(0.0f, 2.0f, 0.0f, "Clock Mode");
		params[CLOCK_CHANNEL_FOCUS].config(0.f,15.f,0.f,"Clock Channel Selector");
		params[INPUT_CHANNEL_FOCUS].config(0.f,15.f,0.f,"Input Channel Selector");
		outputs[POLY_OUTPUT].setChannels(16);

		params[MANUAL_TRIGGER].randomizable=false;
		params[MANUAL_CLEAR_TRIGGER].randomizable=false;

	}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu



};

void ComputerscareDebug::step() {
	std::string thisVal;
	
	clockMode = floor(params[WHICH_CLOCK].value);

	inputMode = floor(params[SWITCH_VIEW].value);

	inputChannel = floor(params[INPUT_CHANNEL_FOCUS].value);
	clockChannel = floor(params[CLOCK_CHANNEL_FOCUS].value);
	if(clockMode == SINGLE_MODE) {
		if (clockTriggers[clockChannel].process(inputs[TRG_INPUT].getVoltage(clockChannel) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].value) ) {
			if(inputMode == POLY_MODE) {
				for(int i = 0; i < 16; i++) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
			}
			else if(inputMode == SINGLE_MODE) {
				 for( unsigned int a = NUM_LINES-1; a > 0; a = a - 1 )
				 {
				 	logLines[a] = logLines[a-1];
				 }

				
				logLines[0] = inputs[VAL_INPUT].getVoltage(inputChannel);
			}
			else if(inputMode == INTERNAL_MODE) {
				for(int i = 0; i < 16; i++) {
					logLines[i] = random::uniform();
				}
			}
			//thisVal = std::to_string(logLines[0]).substr(0,10);
			//outputs[POLY_OUTPUT].setVoltage(logLines[0],0);
			
			
		}
	}
	else if(clockMode == INTERNAL_MODE) {
		if(inputMode == POLY_MODE) {
			for(int i = 0; i < 16; i++) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
		}
		else if(inputMode == SINGLE_MODE) {
			logLines[inputChannel] = inputs[VAL_INPUT].getVoltage(inputChannel);
		}
		else if(inputMode == INTERNAL_MODE) {
			for(int i = 0; i < 16; i++) {
					logLines[i] = random::uniform();
				}
		}
	}
	else if(clockMode == POLY_MODE) {
		if(inputMode == POLY_MODE) {
			for(int i = 0; i < 16; i++) {
				if (clockTriggers[i].process(inputs[TRG_INPUT].getVoltage(i) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].value) ) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
			}
		}
		else if(inputMode == SINGLE_MODE) {
			for(int i = 0; i < 16; i++) {
				if (clockTriggers[i].process(inputs[TRG_INPUT].getVoltage(i) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].value) ) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(inputChannel);
				}
			}
		}
		else if(inputMode == INTERNAL_MODE) {
			for(int i = 0; i < 16; i++) {
				if (clockTriggers[i].process(inputs[TRG_INPUT].getVoltage(i) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].value) ) {
					logLines[i] = random::uniform();
				}
			}
		}
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
struct HidableSmallSnapKnob : SmallSnapKnob {
	bool visible = true;
	int hackIndex = 0;
	ComputerscareDebug *module;
	HidableSmallSnapKnob() {
		SmallSnapKnob();
	}
	void draw(const DrawArgs &args) {
		if(module ? (hackIndex == 0 ? module->clockMode == 0 : module->inputMode == 0) : true) {
			Widget::draw(args);
		}
	};
};
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
struct ConnectedSmallLetter : SmallLetterDisplay {
	ComputerscareDebug *module;
	int index;
	ConnectedSmallLetter(int dex) {
		index = dex;
		value = std::to_string(dex+1);
	}
	void draw(const DrawArgs &ctx) override {
		if(module) {
			int cm = module->clockMode;
			int im = module->inputMode;
			int cc = module->clockChannel;
			int ic = module->inputChannel;

			// both:pink
			// clock: green
			// input:yellow
			baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
			if(cm == 0 && im == 0 && cc == index && ic == index)  {
				baseColor = COLOR_COMPUTERSCARE_PINK;
			} 
			else {
				if(cm == 0 && cc == index) {
					baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
				}
				if(im == 0 && ic == index) {
					baseColor = COLOR_COMPUTERSCARE_YELLOW;
				}
			}
		}
		SmallLetterDisplay::draw(ctx);
	}
};
struct ComputerscareDebugWidget : ModuleWidget {

	ComputerscareDebugWidget(ComputerscareDebug *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareDebugPanel.svg")));

		addInput(createInput<InPort>(Vec(2, 335), module, ComputerscareDebug::TRG_INPUT));
		addInput(createInput<InPort>(Vec(61, 335),  module, ComputerscareDebug::VAL_INPUT));
		addInput(createInput<InPort>(Vec(31, 335), module, ComputerscareDebug::CLR_INPUT));		

		addParam(createParam<ComputerscareClockButton>(Vec(2, 321), module, ComputerscareDebug::MANUAL_TRIGGER));

		addParam(createParam<ComputerscareResetButton>(Vec(32, 320), module, ComputerscareDebug::MANUAL_CLEAR_TRIGGER));
  		
		addParam(createParam<ThreeVerticalXSwitch>(Vec(2,279),module,ComputerscareDebug::WHICH_CLOCK));
		addParam(createParam<ThreeVerticalXSwitch>(Vec(66,279),module,ComputerscareDebug::SWITCH_VIEW));

		HidableSmallSnapKnob *clockKnob = createParam<HidableSmallSnapKnob>(Vec(6,305),module,ComputerscareDebug::CLOCK_CHANNEL_FOCUS);
		clockKnob->module = module;
		clockKnob->hackIndex = 0;
		addParam(clockKnob);
		
		HidableSmallSnapKnob *inputKnob =createParam<HidableSmallSnapKnob>(Vec(66,305),module,ComputerscareDebug::INPUT_CHANNEL_FOCUS);
		inputKnob->module = module;
		inputKnob->hackIndex = 1;
		addParam(inputKnob);

		addOutput(createOutput<OutPort>(Vec(56, 1), module, ComputerscareDebug::POLY_OUTPUT));

		for(int i = 0; i < 16; i++) { 
			ConnectedSmallLetter *sld = new ConnectedSmallLetter(i);
			sld->fontSize = 15;
			sld->textAlign=1;
			sld->box.pos = Vec(-4,33.8+15.08*i);
			sld->box.size = Vec(28, 20);
			sld->module = module;
			addChild(sld);
		}

		StringDisplayWidget3 *stringDisplay = createWidget<StringDisplayWidget3>(Vec(15,34));
		stringDisplay->box.size = Vec(73, 245);
		stringDisplay->module = module;
		addChild(stringDisplay);

		
	}
};


Model *modelComputerscareDebug = createModel<ComputerscareDebug, ComputerscareDebugWidget>("Debug");
