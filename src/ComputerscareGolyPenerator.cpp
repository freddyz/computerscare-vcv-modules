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
	Goly goly;
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
		

		for (int i = 2; i < 8; i++) {
			configParam(KNOB + i, -10.f, 10.f, 0.f, "Parameter " + std::to_string(i - 1));
		}

		goly = Goly();

	}
	void updateCurrents() {
		std::vector<float> golyParams = {params[KNOB+2].getValue(),params[KNOB+3].getValue(),params[KNOB+4].getValue(),params[KNOB+5].getValue(),params[KNOB+6].getValue(),params[KNOB+7].getValue()};
		goly.invoke((int) std::floor(params[KNOB+1].getValue()),golyParams);
	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 5012) {
			counter = 0;
			updateCurrents();
			numChannels=(int) (std::floor(params[KNOB].getValue()));
		}


		outputs[POLY_OUTPUT].setChannels(numChannels);
		for (int i = 0; i < 16; i++) {
			outputs[POLY_OUTPUT].setVoltage(goly.currentValues[i], i);
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

		addLabeledKnob("ch out",5,90,module,0,-2,0);
		addLabeledKnob("Algo",5,140,module,1,0,0);
		addLabeledKnob("A",10,250,module,2,0,0);
		addLabeledKnob("B",20,300,module,3,0,0);
		addLabeledKnob("C",30,260,module,4,0,0);
		addLabeledKnob("D",30,310,module,5,0,0);

		addOutput(createOutput<PointingUpPentagonPort>(Vec(18, 184), module, ComputerscareGolyPenerator::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareGolyPenerator *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 21;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		addParam(createParam<SmoothKnob>(Vec(x, y), module, ComputerscareGolyPenerator::KNOB + index));
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareGolyPenerator = createModel<ComputerscareGolyPenerator, ComputerscareGolyPeneratorWidget>("computerscare-goly-penerator");
