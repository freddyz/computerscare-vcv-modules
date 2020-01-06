#include "Computerscare.hpp"

#include "dtpulse.hpp"
#include "golyFunctions.hpp"

struct ComputerscareGolyPenerator;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;


/*
	knob1: number of channels output 1-16
	knob2: algorithm
	knob3: param 1
*/

struct ComputerscareGolyPenerator : Module {
	int counter = 0;
	int numChannels=16;
	ComputerscareSVGPanel* panelRef;
	float currentValues[16]={0.f};
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


	ComputerscareGolyPenerator()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(KNOB,1.f,16.f,16.f,"Number of Output Channels");
		configParam(KNOB +1,0.f,10.f,1.f,"Algorithm");
		

		for (int i = 2; i < numKnobs; i++) {
			configParam(KNOB + i, 0.0f, 10.0f, 0.0f);
			configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1) + " Voltage", " Volts");
		}

	}
	void updateCurrents() {
		for(int i = 0; i < numChannels; i++) {
			currentValues[i]=(float)i;
		}
	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 5012) {
			//printf("%f \n",random::uniform());
			counter = 0;
			//rect4032
			//south facing high wall
			updateCurrents();
			numChannels=(params[KNOB].getValue());
		}


		outputs[POLY_OUTPUT].setChannels(numChannels);
		for (int i = 0; i < 16; i++) {
			outputs[POLY_OUTPUT].setVoltage(currentValues[i], i);
		}
	}

};

struct ComputerscareGolyPeneratorWidget : ModuleWidget {
	ComputerscareGolyPeneratorWidget(ComputerscareGolyPenerator *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareGolyPeneratorPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareGolyPeneratorPanel.svg")));

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



		addOutput(createOutput<PointingUpPentagonPort>(Vec(28, 24), module, ComputerscareGolyPenerator::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareGolyPenerator *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 16;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		addParam(createParam<SmoothKnob>(Vec(x, y), module, ComputerscareGolyPenerator::KNOB + index));
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareGolyPenerator = createModel<ComputerscareGolyPenerator, ComputerscareGolyPeneratorWidget>("computerscare-goly-penerator");
