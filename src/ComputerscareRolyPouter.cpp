#include "Computerscare.hpp"

struct ComputerscareRolyPouter;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareRolyPouter : Module {
	int counter = 0;
	int routing[numKnobs];
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		KNOB,
		TOGGLES = KNOB + numKnobs,
		NUM_PARAMS = TOGGLES + numToggles

	};
	enum InputIds {
		POLY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS = POLY_OUTPUT + numOutputs
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareRolyPouter()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numKnobs; i++) {
			configParam(KNOB + i, 1.f, 16.f, (i + 1), "output ch:" + std::to_string(i + 1) + " = input ch");
			routing[i] = i;
		}

	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 5012) {
			//printf("%f \n",random::uniform());
			counter = 0;
			for (int i = 0; i < numKnobs; i++) {
				routing[i] = (int)params[KNOB + i].getValue();
			}

		}
		outputs[POLY_OUTPUT].setChannels(16);
		for (int i = 0; i < numKnobs; i++) {
			outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(params[KNOB + i].getValue() - 1), i);
		}
	}

};
struct PouterSmallDisplay : SmallLetterDisplay
{
	ComputerscareRolyPouter *module;
	int ch;
	PouterSmallDisplay(int outputChannelNumber)
	{

		ch = outputChannelNumber;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		//this->setNumDivisionsString();
		if (module)
		{


			std::string str = std::to_string(module->routing[ch]) + "->"+ std::to_string(ch+1);
			value = str;



		}
		SmallLetterDisplay::draw(args);
	}

};
struct ComputerscareRolyPouterWidget : ModuleWidget {
	ComputerscareRolyPouterWidget(ComputerscareRolyPouter *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareRolyPouterPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareRolyPouterPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}
		float xx;
		float yy;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i - i % 8) / 8;
			yy = 66 + 36.5 * (i % 8) + 14.3 * (i - i % 8) / 8;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 0.2 - 3, 0);
		}


		addInput(createInput<InPort>(Vec(4, 24), module, ComputerscareRolyPouter::POLY_INPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 24), module, ComputerscareRolyPouter::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareRolyPouter *module, int index, float labelDx, float labelDy) {

		pouterSmallDisplay = new PouterSmallDisplay(index);
		pouterSmallDisplay->box.size = Vec(5, 10);
		pouterSmallDisplay->fontSize = 12;
		pouterSmallDisplay->textAlign = 1;
		pouterSmallDisplay->module = module;

		addParam(createParam<MediumSnapKnob>(Vec(x, y), module, ComputerscareRolyPouter::KNOB + index));
		pouterSmallDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(pouterSmallDisplay);

	}
	PouterSmallDisplay* pouterSmallDisplay;
};

Model *modelComputerscareRolyPouter = createModel<ComputerscareRolyPouter, ComputerscareRolyPouterWidget>("computerscare-roly-pouter");
