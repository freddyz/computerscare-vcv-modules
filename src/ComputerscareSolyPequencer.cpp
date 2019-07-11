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
		}
		for (int c = 0; c < numOutputChannels; c++) {
			outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(currentStep[c]), c);
		}
		if (globalManualResetTrigger.process(params[MANUAL_RESET_BUTTON].getValue())) {
			resetAll();
		}
		//}
		// Run
		/*
		if (runningTrigger.process(params[RUN_PARAM].getValue())) {
			running = !running;
		}

		bool gateIn = false;
		if (running) {
			if (inputs[EXT_CLOCK_INPUT].isConnected()) {
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage())) {
					setIndex(index + 1);
				}
				gateIn = clockTrigger.isHigh();
			}
			else {
				// Internal clock
				float clockTime = std::pow(2.f, params[CLOCK_PARAM].getValue() + inputs[CLOCK_INPUT].getVoltage());
				phase += clockTime * args.sampleTime;
				if (phase >= 1.f) {
					setIndex(index + 1);
				}
				gateIn = (phase < 0.5f);
			}
		}

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			setIndex(0);
		}

		// Gate buttons
		for (int i = 0; i < 8; i++) {
			if (gateTriggers[i].process(params[GATE_PARAM + i].getValue())) {
				gates[i] = !gates[i];
			}
			outputs[GATE_OUTPUT + i].setVoltage((running && gateIn && i == index && gates[i]) ? 10.f : 0.f);
			lights[GATE_LIGHTS + i].setSmoothBrightness((gateIn && i == index) ? (gates[i] ? 1.f : 0.33) : (gates[i] ? 0.66 : 0.0), args.sampleTime);
		}

		// Outputs
		outputs[ROW1_OUTPUT].setVoltage(params[ROW1_PARAM + index].getValue());
		outputs[ROW2_OUTPUT].setVoltage(params[ROW2_PARAM + index].getValue());
		outputs[ROW3_OUTPUT].setVoltage(params[ROW3_PARAM + index].getValue());
		outputs[GATES_OUTPUT].setVoltage((gateIn && gates[index]) ? 10.f : 0.f);
		lights[RUNNING_LIGHT].value = (running);
		lights[RESET_LIGHT].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime);
		lights[GATES_LIGHT].setSmoothBrightness(gateIn, args.sampleTime);
		lights[ROW_LIGHTS].value = outputs[ROW1_OUTPUT].value / 10.f;
		lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.f;
		lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.f;
		}*/
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


			//std::string str = std::to_string(module->routing[ch]);
			value = "pig";



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

		addLabeledKnob("Steps", 10, 124, module, 0, 0, 0);



		addInput(createInput<InPort>(Vec(14, 84), module, ComputerscareSolyPequencer::POLY_INPUT));


		addParam(createParam<ComputerscareClockButton>(Vec(14, 150), module, ComputerscareSolyPequencer::MANUAL_CLOCK_BUTTON));
		addInput(createInput<InPort>(Vec(14, 164), module, ComputerscareSolyPequencer::CLOCK_INPUT));

		addParam(createParam<ComputerscareResetButton>(Vec(14, 210), module, ComputerscareSolyPequencer::MANUAL_RESET_BUTTON));
		addInput(createInput<InPort>(Vec(14, 224), module, ComputerscareSolyPequencer::RESET_INPUT));




		addOutput(createOutput<PointingUpPentagonPort>(Vec(21, 304), module, ComputerscareSolyPequencer::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareSolyPequencer *module, int index, float labelDx, float labelDy) {

		pouterSmallDisplay = new PouterSmallDisplay(index);
		pouterSmallDisplay->box.size = Vec(20, 20);
		pouterSmallDisplay->box.pos = Vec(x - 2.5 , y + 1.f);
		pouterSmallDisplay->fontSize = 26;
		pouterSmallDisplay->textAlign = 18;
		pouterSmallDisplay->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
		pouterSmallDisplay->breakRowWidth = 20;
		pouterSmallDisplay->module = module;


		outputChannelLabel = new SmallLetterDisplay();
		outputChannelLabel->box.size = Vec(5, 5);
		outputChannelLabel->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		outputChannelLabel->fontSize = 14;
		outputChannelLabel->textAlign = index < 8 ? 1 : 4;
		outputChannelLabel->breakRowWidth = 15;

		outputChannelLabel->value = "hogman";

		//addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, ComputerscareSolyPequencer::KNOB + index));
		addChild(pouterSmallDisplay);
		addChild(outputChannelLabel);

	}
	PouterSmallDisplay* pouterSmallDisplay;
	SmallLetterDisplay* outputChannelLabel;
};


Model *modelComputerscareSolyPequencer = createModel<ComputerscareSolyPequencer, ComputerscareSolyPequencerWidget>("computerscare-soly-pequencer");
