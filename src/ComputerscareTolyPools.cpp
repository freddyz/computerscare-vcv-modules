#include "Computerscare.hpp"
#include "dtpulse.hpp"

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
	int counter = 83910;
	int numChannels = 16;
	int rotation = 0;
	int numInputChannels = 1;
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
		NUM_CHANNELS_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareTolyPools()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(ROTATE_KNOB, 0.f, 15.f, 0.f, "Rotate input", " channels");
		configParam(NUM_CHANNELS_KNOB, 1.f, 16.f, 16.f, "Number of Output Channels");


	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 982) {
			counter = 0;
			numChannels = params[NUM_CHANNELS_KNOB].getValue();
			rotation = params[ROTATE_KNOB].getValue();
			numInputChannels = inputs[POLY_INPUT].getChannels();
		}
		if (inputs[NUM_CHANNELS_CV].isConnected()) {
			numChannels = mapVoltageToChannelCount(inputs[NUM_CHANNELS_CV].getVoltage(0));
		}
		if (inputs[ROTATE_CV].isConnected()) {
			rotation = mapVoltageToChannelCount(inputs[ROTATE_CV].getVoltage(0));
		}
		outputs[POLY_OUTPUT].setChannels(numChannels);
		outputs[NUM_CHANNELS_OUTPUT].setVoltage(mapChannelCountToVoltage(numInputChannels));

		for (int i = 0; i < numChannels; i++) {
			outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage((i + rotation + 16) % 16), i);
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
			else if (type == 1) {
				value = std::to_string(module->rotation);
			}
			else if (type == 2) {
				value = std::to_string(module->numInputChannels);
			}

		}
		else {
			value = std::to_string((random::u32() % 16) + 1);
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

		addInput(createInput<InPort>(Vec(1	, 50), module, ComputerscareTolyPools::POLY_INPUT));
		poolsSmallDisplay = new PoolsSmallDisplay(2);
		poolsSmallDisplay->box.size = Vec(14, 20);
		poolsSmallDisplay->box.pos = Vec(-3 , 80);
		poolsSmallDisplay->fontSize = 22;
		poolsSmallDisplay->textAlign = 18;
		poolsSmallDisplay->breakRowWidth = 20;
		poolsSmallDisplay->module = module;
		addChild(poolsSmallDisplay);


		addLabeledKnob("Num Output Channels", 10, 156, module, ComputerscareTolyPools::NUM_CHANNELS_KNOB, -14, -24, 0);
		addInput(createInput<InPort>(Vec(10, 186), module, ComputerscareTolyPools::NUM_CHANNELS_CV));

		addLabeledKnob("Rotation", 10, 256, module, ComputerscareTolyPools::ROTATE_KNOB, -13, -5, 1);
		addInput(createInput<InPort>(Vec(10, 286), module, ComputerscareTolyPools::ROTATE_CV));


		addOutput(createOutput<OutPort>(Vec(28, 30), module, ComputerscareTolyPools::POLY_OUTPUT));

		addOutput(createOutput<PointingUpPentagonPort>(Vec(31, 76), module, ComputerscareTolyPools::NUM_CHANNELS_OUTPUT));
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
		outputChannelLabel->breakRowWidth = 55;

		outputChannelLabel->value = label;

		addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, index));
		addChild(poolsSmallDisplay);
		//addChild(outputChannelLabel);

	}
	PoolsSmallDisplay* poolsSmallDisplay;
	SmallLetterDisplay* outputChannelLabel;
};


Model *modelComputerscareTolyPools = createModel<ComputerscareTolyPools, ComputerscareTolyPoolsWidget>("computerscare-toly-pools");
