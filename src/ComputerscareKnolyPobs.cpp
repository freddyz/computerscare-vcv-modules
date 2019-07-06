#include "Computerscare.hpp"

struct ComputerscareKnolyPobs;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareKnolyPobs : Module {
	int counter = 0;
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		KNOB,
		TOGGLES = KNOB + numKnobs,
		NUM_PARAMS = TOGGLES + numToggles

	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS = POLY_OUTPUT + numOutputs
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareKnolyPobs()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numKnobs; i++) {
			configParam(KNOB + i, 0.0f, 10.0f, 0.0f);
			configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1) + " Voltage", " Volts");
		}

	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 5012) {
			//printf("%f \n",random::uniform());
			counter = 0;
			//rect4032
			//south facing high wall
		}
		outputs[POLY_OUTPUT].setChannels(16);
		for (int i = 0; i < numKnobs; i++) {
			outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue(), i);
		}
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
		float xx;
		float yy;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i-i % 8)/8;
			yy = 64 + 37.5 * (i % 8) + 14.3 * (i - i % 8)/8;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i-i%8)*1.2-2, 0);
		}



		addOutput(createOutput<PointingUpPentagonPort>(Vec(28, 24), module, ComputerscareKnolyPobs::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareKnolyPobs *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 16;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		addParam(createParam<SmoothKnob>(Vec(x, y), module, ComputerscareKnolyPobs::KNOB + index));
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareKnolyPobs = createModel<ComputerscareKnolyPobs, ComputerscareKnolyPobsWidget>("computerscare-knolypobs");
