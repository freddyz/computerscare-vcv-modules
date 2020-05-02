#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

struct ComputerscareKnolyPobs;

const int numKnobs = 16;


struct ComputerscareKnolyPobs : ComputerscarePolyModule {
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		KNOB,
		POLY_CHANNELS = KNOB + numKnobs,
		GLOBAL_SCALE,
		GLOBAL_OFFSET,
		NUM_PARAMS

	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareKnolyPobs()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numKnobs; i++) {
			configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
		}
		configParam(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels");
		configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
		configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset"," volts");
	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();
		float trim = params[GLOBAL_SCALE].getValue();
		float offset = params[GLOBAL_OFFSET].getValue();
		for (int i = 0; i < polyChannels; i++) {
			outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue()*trim+offset, i);
		}
	}
	void checkPoly() override {
			polyChannels = params[POLY_CHANNELS].getValue();
			if(polyChannels == 0) {
				polyChannels=16;
				params[POLY_CHANNELS].setValue(16);
			}
			outputs[POLY_OUTPUT].setChannels(polyChannels);
	}
};

struct NoRandomSmallKnob : SmallKnob {
	NoRandomSmallKnob() {
		SmallKnob();
	};
	void randomize() override {
		return;
	}
};
struct NoRandomMediumSmallKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));

	NoRandomMediumSmallKnob() {
		setSvg(enabledSvg);
		RoundKnob();
	};
	void randomize() override {
		return;
	}
};

struct DisableableSmoothKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob.svg"));
	std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-small-knob-disabled.svg"));

	int channel = 0;
	bool disabled = false;
	ComputerscarePolyModule *module;

	DisableableSmoothKnob() {
		setSvg(enabledSvg);
		shadow->box.size = math::Vec(0, 0);
		shadow->opacity = 0.f;
	}

	void draw(const DrawArgs& args) override {
		if (module) {
			bool candidate = channel > module->polyChannels - 1;
			if (disabled != candidate) {
				setSvg(candidate ? disabledSvg : enabledSvg);
				dirtyValue = -20.f;
				disabled = candidate;
			}
		}
		else {
		}
		RoundKnob::draw(args);
	}
};

struct ComputerscareKnolyPobsWidget : ModuleWidget {
	ComputerscareKnolyPobsWidget(ComputerscareKnolyPobs *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
			addChild(panel);
		}
		channelWidget = new PolyOutputChannelsWidget(Vec(1,24),module,ComputerscareKnolyPobs::POLY_CHANNELS);
		addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module, ComputerscareKnolyPobs::POLY_OUTPUT));

		addChild(channelWidget);



		addParam(createParam<NoRandomSmallKnob>(Vec(11,54), module, ComputerscareKnolyPobs::GLOBAL_SCALE));
		addParam(createParam<NoRandomMediumSmallKnob>(Vec(32,57), module, ComputerscareKnolyPobs::GLOBAL_OFFSET));


		float xx;
		float yy;
		float yInitial=86;
		float ySpacing =  34;
		float yRightColumnOffset=14.3/8;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i - i % 8) / 8;
			yy = yInitial + ySpacing* (i % 8) +  yRightColumnOffset* (i - i % 8) ;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 1.2 - 3 +(i==8 ? 5 : 0), 2);
		}

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareKnolyPobs *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 18;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		ParamWidget* pob =  createParam<DisableableSmoothKnob>(Vec(x, y), module, ComputerscareKnolyPobs::KNOB + index);

		DisableableSmoothKnob* fader = dynamic_cast<DisableableSmoothKnob*>(pob);

		fader->module = module;
		fader->channel = index;

		addParam(fader);


		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	PolyOutputChannelsWidget* channelWidget;
	PolyChannelsDisplay* channelDisplay;
	DisableableSmoothKnob* fader;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareKnolyPobs = createModel<ComputerscareKnolyPobs, ComputerscareKnolyPobsWidget>("computerscare-knolypobs");
