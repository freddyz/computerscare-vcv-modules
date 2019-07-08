#include "Computerscare.hpp"

struct ComputerscareTolyPools;

/*
Input:

first rotate
knob, CV

numChannels select (auto)
knob,cv


input:
0123456789abcdef

want:
3456

rotate 4,clip 4

*/


struct ComputerscareTolyPools : Module {
	int counter = 0;
	int numChannels = 16;
	int rotation = 0;
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		ROTATE_KNOB,
		NUM_CHANNELS_KNOB,
		AUTO_CHANNELS_SWITCH,
		NUM_PARAMS

	};
	enum InputIds {
		POLY_INPUT,
		ROTATE_CV,
		NUM_CHANNELS_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareTolyPools()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(ROTATE_KNOB, 0.f, 15.f, 0.f, "Rotate input", "channels");
		configParam(NUM_CHANNELS_KNOB, 0.f, 16.f, 16.f, "Number of Output Channels", "channels");


	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 1025) {
			counter = 0;
			numChannels = params[NUM_CHANNELS_KNOB].getValue();
			rotation = params[ROTATE_KNOB].getValue();

		}

		outputs[POLY_OUTPUT].setChannels(numChannels);


		for (int i = 0; i < numChannels; i++) {
			outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage((i + rotation + 16) % 16), i);
			                                //outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(params[KNOB + i].getValue() - 1), i);
		}
	}

};
struct PoolsSmallDisplay : SmallLetterDisplay
{
	ComputerscareTolyPools *module;
	int ch;
	int type = 0;
	PoolsSmallDisplay(int someType)
	{
		type = someType;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		//this->setNumDivisionsString();
		if (module)
		{

			if (type == 0) {
				value = std::to_string(module->numChannels);
			}
			else {
				value = std::to_string(module->rotation);
			}

		}
		SmallLetterDisplay::draw(args);
	}

};

struct ComputerscareTolyPoolsWidget : ModuleWidget {
	ComputerscareTolyPoolsWidget(ComputerscareTolyPools *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareTolyPoolsPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareTolyPoolsPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}
		/*float xx;
		float yy;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i - i % 8) / 8;
			yy = 66 + 36.5 * (i % 8) + 14.3 * (i - i % 8) / 8;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 1.3 - 5, i<8 ? 4 : 0);
		}*/

		//addParam

		addLabeledKnob("Num Output Channels", 2, 86, module, ComputerscareTolyPools::NUM_CHANNELS_KNOB, -5, -30, 0);
		addLabeledKnob("Rotation", 2, 156, module, ComputerscareTolyPools::ROTATE_KNOB, -5, -10, 1);

		addInput(createInput<InPort>(Vec(4, 24), module, ComputerscareTolyPools::POLY_INPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 24), module, ComputerscareTolyPools::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareTolyPools *module, int index, float labelDx, float labelDy, int type) {

		poolsSmallDisplay = new PoolsSmallDisplay(type);
		poolsSmallDisplay->box.size = Vec(20, 20);
		poolsSmallDisplay->box.pos = Vec(x - 2.5 , y + 1.f);
		poolsSmallDisplay->fontSize = 26;
		poolsSmallDisplay->textAlign = 18;
		poolsSmallDisplay->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
		poolsSmallDisplay->breakRowWidth = 20;
		poolsSmallDisplay->module = module;


		outputChannelLabel = new SmallLetterDisplay();
		outputChannelLabel->box.size = Vec(5, 5);
		outputChannelLabel->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		outputChannelLabel->fontSize = 15;
		outputChannelLabel->textAlign = 1;
		outputChannelLabel->breakRowWidth = 50;

		outputChannelLabel->value = label;

		addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, index));
		addChild(poolsSmallDisplay);
		addChild(outputChannelLabel);

	}
	PoolsSmallDisplay* poolsSmallDisplay;
	SmallLetterDisplay* outputChannelLabel;
};


Model *modelComputerscareTolyPools = createModel<ComputerscareTolyPools, ComputerscareTolyPoolsWidget>("computerscare-toly-pools");
