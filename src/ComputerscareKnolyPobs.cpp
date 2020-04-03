#include "Computerscare.hpp"

struct ComputerscareKnolyPobs;

const int numKnobs = 16;

struct ComputerscareKnolyPobs : Module {
	int counterPeriod = 64;
	int counter = counterPeriod + 1;
	int polyChannels = 16;

	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		KNOB,
		POLY_CHANNELS = KNOB + numKnobs,
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
	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > counterPeriod) {
			checkPoly();
			counter = 0;
		}

		for (int i = 0; i < polyChannels; i++) {
			outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue(), i);
		}
	}
	void checkPoly() {
		float candidate = params[POLY_CHANNELS].getValue();
		if (polyChannels != candidate) {
			polyChannels = candidate;
			outputs[POLY_OUTPUT].setChannels(polyChannels);
		}
	}
};

struct DisableableSmoothKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-effed.svg"));
	std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-disabled.svg"));

	int channel = 0;
	bool disabled = false;
	ComputerscareKnolyPobs *module;

	DisableableSmoothKnob() {
		setSvg(enabledSvg);
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

			//module->panelRef = panel;
			addChild(panel);
		}

		addParam(createParam<TinyChannelsSnapKnob>(Vec(8, 26), module, ComputerscareKnolyPobs::POLY_CHANNELS));

		float xx;
		float yy;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i - i % 8) / 8;
			yy = 64 + 37.5 * (i % 8) + 14.3 * (i - i % 8) / 8;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 1.2 - 2, 0);
		}
		addOutput(createOutput<PointingUpPentagonPort>(Vec(28, 24), module, ComputerscareKnolyPobs::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareKnolyPobs *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 16;
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
	DisableableSmoothKnob* fader;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareKnolyPobs = createModel<ComputerscareKnolyPobs, ComputerscareKnolyPobsWidget>("computerscare-knolypobs");
