#include "Computerscare.hpp"
#include "CustomBlankFunctions.hpp"

struct ComputerscareBlankExpander;


struct FrameOffsetParam : ParamQuantity {
	ComputerscareBlankExpander* module;
	int numFrames = -1;
	void setNumFrames(int num) { numFrames = num; }
	std::string getDisplayValueString() override {
		//return &module->params[paramId];
		float val = getValue();
		return string::f("%i", 1 + mapBlankFrameOffset(val, numFrames));
	}
};



struct ComputerscareBlankExpander : Module {
	float rightMessages[2][10] = {};
	bool isConnected = false;
	float lastFrame = -1;
	int numFrames = 1;
	bool scrubbing = false;
	int lastTick = -1;


	enum ParamIds {
		CLOCK_MODE,
		MANUAL_RESET_BUTTON,
		ZERO_OFFSET,
		NUM_PARAMS
	};
	enum InputIds {
		SYNC_INPUT,
		RESET_INPUT,
		SPEED_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		EOC_OUTPUT,
		EACH_FRAME_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::SchmittTrigger eocMessageReadTrigger;
	dsp::SchmittTrigger eachFrameReadTrigger;

	dsp::SchmittTrigger syncTrigger;

	dsp::PulseGenerator eocPulse;
	dsp::PulseGenerator eachFramePulse;

	dsp::Timer syncTimer;

	std::vector<std::string> clockModeDescriptions;

	FrameOffsetParam* frameOffsetQuantity;

	ComputerscareBlankExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CLOCK_MODE, 0.f, 2.f, 0.f, "Clock Mode");
		configParam(MANUAL_RESET_BUTTON, 0.f, 1.f, 0.f, "Manual Reset");
		configParam<FrameOffsetParam>(ZERO_OFFSET, 0.f, 0.999f, 0.f, "EOC / Reset Frame #");

		frameOffsetQuantity = dynamic_cast<FrameOffsetParam*>(paramQuantities[ZERO_OFFSET]);

		clockModeDescriptions.push_back("Sync");
		clockModeDescriptions.push_back("Scan");
		clockModeDescriptions.push_back("Frame Advance");

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
	}
	void process(const ProcessArgs &args) override {
		if (rightExpander.module && rightExpander.module->model == modelComputerscareBlank) {
			// Get consumer message
			float *messageFromMother = (float*) rightExpander.consumerMessage;
			isConnected = true;

			float *messageToSendToMother = (float*) rightExpander.module->leftExpander.producerMessage;


			float currentFrame = messageFromMother[0];
			int newNumFrames = messageFromMother[1];
			int mappedFrame = messageFromMother[2];
			int scrubFrame = messageFromMother[3];
			int tick = messageFromMother[4];


			if (newNumFrames != numFrames) {
				numFrames = newNumFrames;
				frameOffsetQuantity->setNumFrames(numFrames);
			}

			float currentSyncTime = syncTimer.process(args.sampleTime);

			if (eocMessageReadTrigger.process(mappedFrame == scrubFrame ? 10.f : 0.f)) {
				eocPulse.trigger(1e-3);
			}
			if (eachFrameReadTrigger.process(lastTick != tick ? 10.f : 0.f)) {
				eachFramePulse.trigger(1e-3);
			}


			messageToSendToMother[0] = params[CLOCK_MODE].getValue();

			messageToSendToMother[1] = inputs[SYNC_INPUT].isConnected();
			messageToSendToMother[2] = inputs[SYNC_INPUT].getVoltage();

			messageToSendToMother[3] = inputs[RESET_INPUT].isConnected();
			messageToSendToMother[4] = inputs[RESET_INPUT].getVoltage();

			messageToSendToMother[5] = inputs[SPEED_INPUT].isConnected();
			messageToSendToMother[6] = inputs[SPEED_INPUT].getVoltage();

			messageToSendToMother[7] = params[ZERO_OFFSET].getValue();

			messageToSendToMother[8] = scrubbing;

			messageToSendToMother[9] = params[MANUAL_RESET_BUTTON].getValue() * 10;


			outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);
			outputs[EACH_FRAME_OUTPUT].setVoltage(eachFramePulse.process(args.sampleTime) ? 10.f : 0.f);

			rightExpander.module->leftExpander.messageFlipRequested = true;
			lastFrame = currentFrame;
			lastTick = tick;
		}
		else {
			isConnected = false;
			// No mother module is connected.
		}
	}
	void setScrubbing(bool scrub) {
		scrubbing = scrub;
	}
};
struct FrameScrubKnob : SmallKnob {
	ComputerscareBlankExpander* module;
	void onDragStart(const event::DragStart& e) override {
		module->setScrubbing(true);
		SmallKnob::onDragStart(e);
	}
	void onDragEnd(const event::DragEnd& e) override {
		module->setScrubbing(false);
		SmallKnob::onDragEnd(e);
	}
	void onDragMove(const event::DragMove& e) override {
		SmallKnob::onDragMove(e);
	};
};
struct ClockModeButton : app::SvgSwitch {
	ClockModeButton() {
		//momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blank-clock-mode-sync.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blank-clock-mode-scan.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blank-clock-mode-frame.svg")));
	}
};
struct ComputerscareBlankExpanderWidget : ModuleWidget {
	ComputerscareBlankExpanderWidget(ComputerscareBlankExpander *module) {
		setModule(module);
		box.size = Vec(30, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareCustomBlankExpanderPanel.svg")));
			addChild(panel);
		}

		float inStartY = 30;
		float dY = 40;

		float outStartY = 250;

		addParam(createParam<ClockModeButton>(Vec(0.5, inStartY + dY / 2), module, ComputerscareBlankExpander::CLOCK_MODE));

		addInput(createInput<InPort>(Vec(2, inStartY + dY), module, ComputerscareBlankExpander::SYNC_INPUT));

		addParam(createParam<ComputerscareResetButton>(Vec(0, inStartY + 2 * dY), module, ComputerscareBlankExpander::MANUAL_RESET_BUTTON));

		addInput(createInput<InPort>(Vec(2, inStartY + 3 * dY), module, ComputerscareBlankExpander::RESET_INPUT));
		addInput(createInput<InPort>(Vec(2, inStartY + 4 * dY), module, ComputerscareBlankExpander::SPEED_INPUT));

		addOutput(createOutput<PointingUpPentagonPort>(Vec(2, outStartY), module, ComputerscareBlankExpander::EACH_FRAME_OUTPUT));

		frameOffsetKnob = createParam<FrameScrubKnob>(Vec(6, outStartY + dY), module, ComputerscareBlankExpander::ZERO_OFFSET);
		frameOffsetKnob->module = module;

		addParam(frameOffsetKnob);
		addOutput(createOutput<PointingUpPentagonPort>(Vec(2, outStartY + dY * 1.6), module, ComputerscareBlankExpander::EOC_OUTPUT));
	}
	void step() {

		ModuleWidget::step();
	}
	FrameScrubKnob* frameOffsetKnob;
};
Model *modelComputerscareBlankExpander = createModel<ComputerscareBlankExpander, ComputerscareBlankExpanderWidget>("computerscare-blank-expander");
