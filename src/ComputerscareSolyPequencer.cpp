#include "Computerscare.hpp"

struct ComputerscareSolyPequencer;

struct ComputerscareSolyPequencer : Module {
	int currentStep[16] = {0};
	int numSteps[16] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
	bool autoNumSteps = true;
	rack::dsp::SchmittTrigger clockTriggers[16];
	rack::dsp::SchmittTrigger resetTriggers[16];

	rack::dsp::SchmittTrigger globalManualClockTrigger;
	rack::dsp::SchmittTrigger globalManualResetTrigger;


	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		MANUAL_RUN_BUTTON,
		MANUAL_CLOCK_BUTTON,
		MANUAL_RESET_BUTTON,
		NUM_STEPS_AUTO_BUTTON,
		NUM_STEPS_KNOB,
		NUM_PARAMS
	};
	enum InputIds {
		POLY_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		RUN_INPUT,
		NUM_STEPS_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareSolyPequencer()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MANUAL_CLOCK_BUTTON, 0.f, 1.f, 0.f);
		configParam(MANUAL_RESET_BUTTON, 0.f, 1.f, 0.f);
		//	configParam(KNOB + i, 1.f, 16.f, (i + 1), "output ch:" + std::to_string(i + 1) + " = input ch");

	}
	void resetAll() {
		for (int i = 0; i < 16; i++) {
			currentStep[i] = 0;
		}
	}
	void process(const ProcessArgs &args) override {
		int numInputChannels = inputs[POLY_INPUT].getChannels();
		int numReset = inputs[RESET_INPUT].getChannels();
		int numClock = inputs[CLOCK_INPUT].getChannels();
		int numNumSteps = inputs[NUM_STEPS_INPUT].getChannels();
		int numOutputChannels = numClock > 0 ? numClock : 1;
		bool globalClocked = globalManualClockTrigger.process(params[MANUAL_CLOCK_BUTTON].getValue());
		outputs[POLY_OUTPUT].setChannels(numOutputChannels);
		if (inputs[POLY_INPUT].isConnected()) {
			for (int j = 0; j < numOutputChannels; j++) {
				if (globalClocked || clockTriggers[j].process(inputs[CLOCK_INPUT].getVoltage(j))) {
					currentStep[j]++;
					if (autoNumSteps) {
						currentStep[j] = currentStep[j] % numInputChannels;
					}
					else {
						currentStep[j] = currentStep[j] % numSteps[j];
					}


				}
				if(j <= numReset) {
					if(resetTriggers[j].process(inputs[RESET_INPUT].getVoltage(j))) {
						currentStep[j] = 0;
					}
				}
			}

			for (int c = 0; c < numOutputChannels; c++) {
				outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(currentStep[c]), c);
			}
		}
		if (globalManualResetTrigger.process(params[MANUAL_RESET_BUTTON].getValue())) {
			resetAll();
		}
	
	}

};
struct PequencerSmallDisplay : SmallLetterDisplay
{
	ComputerscareSolyPequencer *module;
	int ch;
	PequencerSmallDisplay(int outputChannelNumber)
	{

		ch = outputChannelNumber;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		//this->setNumDivisionsString();
		if (module)
		{


			//std::string str = std::to_string(module->routing[ch]);
			value = std::to_string(module->currentStep[ch]);



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

		addOutput(createOutput<PointingUpPentagonPort>(Vec(14, 48), module, ComputerscareSolyPequencer::POLY_OUTPUT));


		addLabeledKnob("Steps", 10, 124, module, 0, 0, 0);
		stepNumberGrid(1,230,30,15,module);


		addInput(createInput<InPort>(Vec(19, 102), module, ComputerscareSolyPequencer::POLY_INPUT));


		addParam(createParam<ComputerscareClockButton>(Vec(10, 130), module, ComputerscareSolyPequencer::MANUAL_CLOCK_BUTTON));
		addInput(createInput<InPort>(Vec(10, 144), module, ComputerscareSolyPequencer::CLOCK_INPUT));

		addParam(createParam<ComputerscareResetButton>(Vec(30, 168), module, ComputerscareSolyPequencer::MANUAL_RESET_BUTTON));
		addInput(createInput<InPort>(Vec(30, 182), module, ComputerscareSolyPequencer::RESET_INPUT));




		
	}
	void stepNumberGrid(int x, int y, int xspacing, int yspacing, ComputerscareSolyPequencer *module) {
		for(int i = 0; i < 2; i++) {
			for(int j = 0; j < 8; j++) {
				psd = new PequencerSmallDisplay(i*8+j);
		psd->box.size = Vec(10, 10);
		psd->box.pos = Vec(x +i*xspacing , y + j*yspacing);
		psd->fontSize = 18;
		psd->textAlign = 18;
		psd->textColor =nvgRGB(0x24, 0x44, 0x31);
		psd->breakRowWidth = 20;
		psd->module = module;
		addChild(psd);
			}
		}
	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareSolyPequencer *module, int index, float labelDx, float labelDy) {

		/*psd = new PequencerSmallDisplay(index);
		psd->box.size = Vec(20, 20);
		psd->box.pos = Vec(x - 2.5 , y + 1.f);
		psd->fontSize = 26;
		psd->textAlign = 18;
		psd->textColor =nvgRGB(0x24, 0x44, 0x31);
		psd->breakRowWidth = 20;
		psd->module = module;*/


		outputChannelLabel = new SmallLetterDisplay();
		outputChannelLabel->box.size = Vec(5, 5);
		outputChannelLabel->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		outputChannelLabel->fontSize = 14;
		outputChannelLabel->textAlign = index < 8 ? 1 : 4;
		outputChannelLabel->breakRowWidth = 15;

		//outputChannelLabel->value = "hogman";

		//addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, ComputerscareSolyPequencer::KNOB + index));
		//addChild(psd);
		addChild(outputChannelLabel);

	}
	PequencerSmallDisplay* psd;
	SmallLetterDisplay* outputChannelLabel;
};


Model *modelComputerscareSolyPequencer = createModel<ComputerscareSolyPequencer, ComputerscareSolyPequencerWidget>("computerscare-soly-pequencer");
