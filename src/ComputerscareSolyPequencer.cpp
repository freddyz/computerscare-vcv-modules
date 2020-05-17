#include "Computerscare.hpp"

struct ComputerscareSolyPequencer;

struct ComputerscareSolyPequencer : ComputerscarePolyModule {
	int currentStep[16] = {0};
	int numSteps[16] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
	int clockChannels[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	int resetChannels[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

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
		POLY_CHANNELS,
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
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareSolyPequencer()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MANUAL_CLOCK_BUTTON, 0.f, 1.f, 0.f);
		configParam(MANUAL_RESET_BUTTON, 0.f, 1.f, 0.f);
		configParam<AutoParamQuantity>(POLY_CHANNELS, 0.f, 16.f, 16.f, "Poly Channels");
	}
	void resetAll() {
		for (int i = 0; i < 16; i++) {
			currentStep[i] = 0;
		}
	}
	void checkPoly() override {
		int resetNum = inputs[RESET_INPUT].getChannels();
		int clockNum = inputs[CLOCK_INPUT].getChannels();
		int knobSetting = params[POLY_CHANNELS].getValue();
		if (knobSetting == 0) {
			polyChannels = inputs[CLOCK_INPUT].isConnected() ? std::max(clockNum, resetNum) : 16;
		}
		else {
			polyChannels = knobSetting;
		}
		for (int i = 0; i < 16; i++) {
			clockChannels[i] = std::max(1, std::min(i + 1, clockNum));
			resetChannels[i] = std::max(1, std::min(i + 1, resetNum));
		}
		outputs[POLY_OUTPUT].setChannels(polyChannels);
		outputs[EOC_OUTPUT].setChannels(polyChannels);
	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();
		int numInputChannels = inputs[POLY_INPUT].getChannels();
		int numReset = inputs[RESET_INPUT].getChannels();
		int numClock = inputs[CLOCK_INPUT].getChannels();
		//int numOutputChannels = numClock > 0 ? numClock : 1;
		bool globalClocked = globalManualClockTrigger.process(params[MANUAL_CLOCK_BUTTON].getValue());
		bool manualReset = globalManualResetTrigger.process(params[MANUAL_RESET_BUTTON].getValue());

		if (inputs[POLY_INPUT].isConnected()) {
			bool currentClock[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			bool currentReset[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			bool isHigh[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			bool eoc[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			for (int i = 0; i < 16; i++) {
				currentClock[i] = clockTriggers[i].process(inputs[CLOCK_INPUT].getVoltage(i));
				currentReset[i] = resetTriggers[i].process(inputs[RESET_INPUT].getVoltage(i));
				isHigh[i] = clockTriggers[i].isHigh();
			}
			for(int i =0; i < 16; i++) {
				eoc[i] = (currentClock[clockChannels[i]-1] && isHigh[clockChannels[i]-1]) || (currentReset[resetChannels[i]-1] && resetTriggers[resetChannels[i]-1].isHigh());
			}
			for (int j = 0; j < polyChannels; j++) {
				if (globalClocked || currentClock[clockChannels[j] - 1]) {
					currentStep[j]++;
					if (autoNumSteps) {
						currentStep[j] = currentStep[j] % numInputChannels;
					}
					else {
						currentStep[j] = currentStep[j] % numSteps[j];
					}


				}
				if (j <= numReset) {
					if (resetTriggers[j].process(inputs[RESET_INPUT].getVoltage(j))) {
						currentStep[j] = 0;
					}
				}
			}
			for (int c = 0; c < polyChannels; c++) {
				outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(currentStep[c]), c);
				outputs[EOC_OUTPUT].setVoltage(currentStep[c]==0 && eoc[c] ? 10.f : 0.f,c);
			}
			
		}
		if (manualReset) {
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
			addChild(panel);

		}

		addOutput(createOutput<PointingUpPentagonPort>(Vec(4, 56), module, ComputerscareSolyPequencer::POLY_OUTPUT));
		addOutput(createOutput<TinyJack>(Vec(40, 84), module, ComputerscareSolyPequencer::EOC_OUTPUT));


		channelWidget = new PolyOutputChannelsWidget(Vec(26, 56), module, ComputerscareSolyPequencer::POLY_CHANNELS);
		addChild(channelWidget);

		addLabeledKnob("Steps", 10, 124, module, 0, 0, 0);
		stepNumberGrid(1, 230, 30, 15, module);


		addInput(createInput<InPort>(Vec(20, 114), module, ComputerscareSolyPequencer::POLY_INPUT));


		addParam(createParam<ComputerscareClockButton>(Vec(10, 150), module, ComputerscareSolyPequencer::MANUAL_CLOCK_BUTTON));
		addInput(createInput<PointingUpPentagonPort>(Vec(8, 169), module, ComputerscareSolyPequencer::CLOCK_INPUT));

		addParam(createParam<ComputerscareResetButton>(Vec(30, 168), module, ComputerscareSolyPequencer::MANUAL_RESET_BUTTON));
		addInput(createInput<InPort>(Vec(30, 182), module, ComputerscareSolyPequencer::RESET_INPUT));


	}
	void stepNumberGrid(int x, int y, int xspacing, int yspacing, ComputerscareSolyPequencer *module) {
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 8; j++) {
				psd = new PequencerSmallDisplay(i * 8 + j);
				psd->box.size = Vec(10, 10);
				psd->box.pos = Vec(x + i * xspacing , y + j * yspacing);
				psd->fontSize = 18;
				psd->textAlign = 18;
				psd->textColor = nvgRGB(0x24, 0x44, 0x31);
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

	PolyOutputChannelsWidget* channelWidget;
	PequencerSmallDisplay* psd;
	SmallLetterDisplay* outputChannelLabel;
};


Model *modelComputerscareSolyPequencer = createModel<ComputerscareSolyPequencer, ComputerscareSolyPequencerWidget>("computerscare-soly-pequencer");
