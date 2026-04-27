#include "Computerscare.hpp"
#include "complex/ComplexWidgets.hpp"

#include <array>

struct ComputerscareComplexGenerator;

	const int numComplexGeneratorKnobs = 16;


struct ComputerscareComplexGenerator : ComputerscareComplexBase {
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		COMPLEX_XY,

		POLY_CHANNELS=COMPLEX_XY + 2*numComplexGeneratorKnobs,
		
		SCALE_VAL_AB,
		SCALE_TRIM_AB = SCALE_VAL_AB+2,

		OFFSET_VAL_AB = SCALE_TRIM_AB+2,
		OFFSET_TRIM_AB = OFFSET_VAL_AB+2,

		DELTA_SCALE_AB = OFFSET_TRIM_AB+2,
		DELTA_OFFSET_AB = DELTA_SCALE_AB+2,
		

		MAIN_OUTPUT_MODE,

		NEXT,
		GLOBAL_SCALE,
		GLOBAL_OFFSET,
		NUM_PARAMS

	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		COMPOLY_MAIN_OUT_A,
		COMPOLY_MAIN_OUT_B,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};




	ComputerscareComplexGenerator()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numComplexGeneratorKnobs; i++) {
			configParam(COMPLEX_XY + 2*i, -10.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
			configParam(COMPLEX_XY + 2*i+1, -10.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
		}

		configParam(SCALE_VAL_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(SCALE_VAL_AB + 1, -10.f, 10.f, 0.f, "Channel ");
		getParamQuantity(SCALE_VAL_AB)->randomizeEnabled = false;
		getParamQuantity(SCALE_VAL_AB+1)->randomizeEnabled = false;

		configParam(OFFSET_VAL_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(OFFSET_VAL_AB + 1, -10.f, 10.f, 0.f, "Channel ");
		getParamQuantity(OFFSET_VAL_AB)->randomizeEnabled = false;
		getParamQuantity(OFFSET_VAL_AB+1)->randomizeEnabled = false;


		configParam(DELTA_OFFSET_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(DELTA_OFFSET_AB + 1, -10.f, 10.f, 0.f, "Channel ");


		configParam(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels");
		configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
		configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");
		configParam(MAIN_OUTPUT_MODE,0.f,3.f,0.f);

		getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
		getParamQuantity(POLY_CHANNELS)->resetEnabled = false;
		getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
		getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;

		configOutput(COMPOLY_MAIN_OUT_A, "Main A");
		configOutput(COMPOLY_MAIN_OUT_B, "Main B");

	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();
		float trim = params[GLOBAL_SCALE].getValue();
		
		float offsetX = params[OFFSET_VAL_AB].getValue();
		float offsetY = params[OFFSET_VAL_AB+1].getValue();

		float scaleX = params[SCALE_VAL_AB].getValue();
		float scaleY = params[SCALE_VAL_AB+1].getValue();

		math::Vec scaleRect = Vec(scaleX,scaleY);




		for (int i = 0; i < polyChannels; i++) {
			if(i < 8) {

				float x0 = params[COMPLEX_XY + 2*i].getValue();
				float y0 = params[COMPLEX_XY + 2*i+1].getValue();

				float x1 = x0*scaleRect.x - y0*scaleRect.y;
				float y1 = x0*scaleRect.y + y0*scaleRect.x;

				outputs[COMPOLY_MAIN_OUT_A].setVoltage(x1 + offsetX, 2*i);
				outputs[COMPOLY_MAIN_OUT_A].setVoltage(y1 + offsetY, 2*i+1);
			} 
			
			//outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue()*trim + offset, i);
		}
	}
	void checkPoly() override {
		polyChannels = params[POLY_CHANNELS].getValue();
		if (polyChannels == 0) {
			polyChannels = 16;
			params[POLY_CHANNELS].setValue(16);
		}
		outputs[COMPOLY_MAIN_OUT_A].setChannels(polyChannels);
	}
	json_t *dataToJson() override {
    json_t *rootJ = json_object();
    return rootJ;
  }

    void dataFromJson(json_t *rootJ) override {
    }
};

struct NoRandomSmallKnob : SmallKnob {
	NoRandomSmallKnob() {
		SmallKnob();
	};
};
struct NoRandomMediumSmallKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/computerscare-medium-small-knob.svg"));

	NoRandomMediumSmallKnob() {
		setSvg(enabledSvg);
		RoundKnob();
	};
};

struct DisableableSmoothKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
	std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/computerscare-medium-small-knob-disabled.svg"));

	int channel = 0;
	bool disabled = false;
	ComputerscarePolyModule *module;

	DisableableSmoothKnob() {
		setSvg(enabledSvg);
		shadow->box.size = math::Vec(0, 0);
		shadow->opacity = 0.f;
	}
	void step() override {
		if (module) {
			bool candidate = channel > module->polyChannels - 1;
			if (disabled != candidate) {
				setSvg(candidate ? disabledSvg : enabledSvg);
				onChange(*(new event::Change()));
				fb->dirty = true;
				disabled = candidate;
			}
		}
		else {
		}
		RoundKnob::step();
	}
};

struct ComputerscareComplexGeneratorWidget : ModuleWidget {
	ComputerscareComplexGeneratorWidget(ComputerscareComplexGenerator *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareComplexGeneratorPanel.svg")));
		box.size = Vec(8 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareComplexGeneratorPanel.svg")));
			addChild(panel);
		}
		channelWidget = new PolyOutputChannelsWidget(Vec(92, 4), module, ComputerscareComplexGenerator::POLY_CHANNELS);
	

		//addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module, ComputerscareComplexGenerator::POLY_OUTPUT));

		addChild(channelWidget);

		cpx::CompolyPortsWidget* mainOutput = new cpx::CompolyPortsWidget(Vec(60, 350),module,ComputerscareComplexGenerator::COMPOLY_MAIN_OUT_A,ComputerscareComplexGenerator::MAIN_OUTPUT_MODE,0.6);
    addChild(mainOutput);



    cpx::ComplexXY* offsetValAB = new cpx::ComplexXY(module,ComputerscareComplexGenerator::OFFSET_VAL_AB);
    offsetValAB->box.size=Vec(25,25);
    offsetValAB->box.pos=Vec(32, 27);
    addChild(offsetValAB);

    cpx::ComplexXY* scaleValAB = new cpx::ComplexXY(module,ComputerscareComplexGenerator::SCALE_VAL_AB);
    scaleValAB->box.size=Vec(25,25);
    scaleValAB->box.pos=Vec(5, 27);
    addChild(scaleValAB);

		//addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module, ComputerscareComplexGenerator::GLOBAL_SCALE));
		//addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscareComplexGenerator::GLOBAL_OFFSET));


		float xx;
		float yy;
		float yInitial = 56;
		float ySpacing =  34;
		float yRightColumnOffset = 14.3 / 8;
		for (int i = 0; i < numComplexGeneratorKnobs; i++) {
			xx = 1.4f + 32.3 * (i - i % 8) / 8;
			yy = yInitial + ySpacing * (i % 8) +  yRightColumnOffset * (i - i % 8) ;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i*2, (i - i % 8) * 1.2 - 3 + (i == 8 ? 5 : 0), 2);
		}

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareComplexGenerator *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 18;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

	/*	ParamWidget* pob =  createParam<DisableableSmoothKnob>(Vec(x, y), module, ComputerscareComplexGenerator::KNOB + index);

		DisableableSmoothKnob* fader = dynamic_cast<DisableableSmoothKnob*>(pob);

		fader->module = module;
		fader->channel = index;

		addParam(fader);*/

		 cpx::ComplexXY* xy = new cpx::ComplexXY(module,ComputerscareComplexGenerator::COMPLEX_XY+index);
     xy->box.size=Vec(25,25);
     xy->box.pos=Vec(x,y);
     addChild(xy);


		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	PolyOutputChannelsWidget* channelWidget;
	PolyChannelsDisplay* channelDisplay;
	DisableableSmoothKnob* fader;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareComplexGenerator = createModel<ComputerscareComplexGenerator, ComputerscareComplexGeneratorWidget>("computerscare-complex-generator");
