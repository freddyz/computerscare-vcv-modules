#include "Computerscare.hpp"

struct ComputerscareSolyPequencer;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareSolyPequencer : Module {
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


	ComputerscareSolyPequencer()  {

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
	ComputerscareSolyPequencer *module;
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


			std::string str = std::to_string(module->routing[ch]);
			value = str;



		}
		SmallLetterDisplay::draw(args);
	}

};

struct ComputerscareSolyPequencerWidget : ModuleWidget {
	ComputerscareSolyPequencerWidget(ComputerscareSolyPequencer *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareSolyPequencerPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareSolyPequencerPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}
		float xx;
		float yy;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i - i % 8) / 8;
			yy = 66 + 36.5 * (i % 8) + 14.3 * (i - i % 8) / 8;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 1.3 - 5, i<8 ? 4 : 0);
		}


		addInput(createInput<InPort>(Vec(4, 24), module, ComputerscareSolyPequencer::POLY_INPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 24), module, ComputerscareSolyPequencer::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareSolyPequencer *module, int index, float labelDx, float labelDy) {

		pouterSmallDisplay = new PouterSmallDisplay(index);
		pouterSmallDisplay->box.size = Vec(20, 20);
		pouterSmallDisplay->box.pos = Vec(x-2.5 ,y+1.f);
		pouterSmallDisplay->fontSize = 26;
		pouterSmallDisplay->textAlign = 18;
		pouterSmallDisplay->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
		pouterSmallDisplay->breakRowWidth=20;
		pouterSmallDisplay->module = module;


		outputChannelLabel = new SmallLetterDisplay();
		outputChannelLabel->box.size = Vec(5, 5);
		outputChannelLabel->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		outputChannelLabel->fontSize = 14;
		outputChannelLabel->textAlign = index < 8 ? 1 : 4;
		outputChannelLabel->breakRowWidth=15;

		outputChannelLabel->value = std::to_string(index + 1);

		addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, ComputerscareSolyPequencer::KNOB + index));
		addChild(pouterSmallDisplay);
		addChild(outputChannelLabel);

	}
	PouterSmallDisplay* pouterSmallDisplay;
	SmallLetterDisplay* outputChannelLabel;
};


Model *modelComputerscareSolyPequencer = createModel<ComputerscareSolyPequencer, ComputerscareSolyPequencerWidget>("computerscare-soly-pequencer");
