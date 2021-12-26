#include "Computerscare.hpp"
#include "CustomBlankFunctions.hpp"

struct ComputerscareBlankExpander;

const std::string clockModeDescriptions[3] = {"Sync\nAnimation will synchronize to a steady clock signal", "Scan\nAnimation will linearly follow a 0-10v CV.  0v → frame 1, 10v → last frame", "Frame Advance\nClock signal will advance the animation by 1 frame" };


struct FrameOffsetParam : ParamQuantity {
	int numFrames = -1;
	void setNumFrames(int num) { numFrames = num; }
	std::string getDisplayValueString() override {
		float val = getValue();
		return string::f("%i", 1 + mapBlankFrameOffset(val, numFrames));
	}
};


//template <const std::string& options>
struct ClockModeParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int val = getValue();
		return clockModeDescriptions[val];
	}
};



struct ComputerscareBlankExpander : Module {
	float rightMessages[2][11] = {};
	bool motherConnected = false;
	float lastFrame = -1;
	int numFrames = 1;
	bool scrubbing = false;
	int lastTick = -1;


	enum ParamIds {
		CLOCK_MODE,
		MANUAL_RESET_BUTTON,
		ZERO_OFFSET,
		MANUAL_NEXT_FILE_BUTTON,
		NUM_PARAMS
	};
	enum InputIds {
		SYNC_INPUT,
		RESET_INPUT,
		NEXT_FILE_INPUT,
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

	FrameOffsetParam* frameOffsetQuantity;

	ComputerscareBlankExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<ClockModeParamQuantity>(CLOCK_MODE, 0.f, 2.f, 0.f, "Clock Mode");
		configParam(MANUAL_RESET_BUTTON, 0.f, 1.f, 0.f, "Manual Reset");
		configParam<FrameOffsetParam>(ZERO_OFFSET, 0.f, 0.999f, 0.f, "EOC / Reset Frame #");
		configParam(MANUAL_NEXT_FILE_BUTTON, 0.f, 1.f, 0.f, "Next File (see right click menu of mother for options)");

		configInput(SYNC_INPUT, "Sync");
		configInput(RESET_INPUT, "Reset");
		configInput(NEXT_FILE_INPUT, "Next Slideshow File");
		configOutput(EOC_OUTPUT, "End of Animation");
		configOutput(EACH_FRAME_OUTPUT, "Frame Change");

		frameOffsetQuantity = dynamic_cast<FrameOffsetParam*>(paramQuantities[ZERO_OFFSET]);

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
	}
	void process(const ProcessArgs &args) override {
		if (rightExpander.module && rightExpander.module->model == modelComputerscareBlank) {
			// Get consumer message
			float *messageFromMother = (float*) rightExpander.consumerMessage;
			motherConnected = true;

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



			if (eocMessageReadTrigger.process(currentFrame == 0 ? 10.f : 0.f)) {
				eocPulse.trigger(1e-3f);
			}
			if (eachFrameReadTrigger.process(lastTick != tick ? 10.f : 0.f)) {
				eachFramePulse.trigger(1e-3f);
			}


			messageToSendToMother[0] = params[CLOCK_MODE].getValue();

			messageToSendToMother[1] = inputs[SYNC_INPUT].isConnected();
			messageToSendToMother[2] = inputs[SYNC_INPUT].getVoltage();

			messageToSendToMother[3] = inputs[RESET_INPUT].isConnected();
			messageToSendToMother[4] = inputs[RESET_INPUT].getVoltage();

			messageToSendToMother[5] = inputs[NEXT_FILE_INPUT].isConnected();
			messageToSendToMother[6] = inputs[NEXT_FILE_INPUT].getVoltage();;

			messageToSendToMother[7] = params[ZERO_OFFSET].getValue();

			messageToSendToMother[8] = scrubbing;

			messageToSendToMother[9] = params[MANUAL_RESET_BUTTON].getValue() * 10;

			messageToSendToMother[10] = params[MANUAL_NEXT_FILE_BUTTON].getValue() * 10;


			outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);
			outputs[EACH_FRAME_OUTPUT].setVoltage(eachFramePulse.process(args.sampleTime) ? 10.f : 0.f);

			rightExpander.module->leftExpander.messageFlipRequested = true;
			lastFrame = currentFrame;
			lastTick = tick;
		}
		else {
			motherConnected = false;
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
		shadow->opacity = 0.f;
		//momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blank-clock-mode-sync.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blank-clock-mode-scan.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blank-clock-mode-frame.svg")));
	}
};
struct LogoWidget : SvgWidget {
	ComputerscareBlankExpander *module;
	int motherConnected = -1;
	LogoWidget() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-logo-normal.svg")));
		SvgWidget();
	}
	void step() override {
		if (module) {
			if (module->motherConnected != motherConnected) {
				if (module->motherConnected) {
					setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-logo-normal.svg")));
				} else {
					setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-logo-sad.svg")));
				}
			}
			motherConnected = module->motherConnected;
		}
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

		LogoWidget *logo = new LogoWidget();
		logo->module = module;
		addChild(logo);

		float inStartY = 20;
		float dY = 40;

		float outStartY = 250;

		addParam(createParam<ClockModeButton>(Vec(0.5, inStartY + .25 * dY), module, ComputerscareBlankExpander::CLOCK_MODE));
		addInput(createInput<InPort>(Vec(2, inStartY + 0.75 * dY), module, ComputerscareBlankExpander::SYNC_INPUT));

		addParam(createParam<ComputerscareResetButton>(Vec(0, inStartY + 1.75 * dY), module, ComputerscareBlankExpander::MANUAL_RESET_BUTTON));
		addInput(createInput<InPort>(Vec(2, inStartY + 2.25 * dY), module, ComputerscareBlankExpander::RESET_INPUT));

		addParam(createParam<ComputerscareNextButton>(Vec(0, inStartY + 3.25 * dY), module, ComputerscareBlankExpander::MANUAL_NEXT_FILE_BUTTON));
		addInput(createInput<InPort>(Vec(2, inStartY + 3.75 * dY), module, ComputerscareBlankExpander::NEXT_FILE_INPUT));

		addOutput(createOutput<PointingUpPentagonPort>(Vec(2, 236), module, ComputerscareBlankExpander::EACH_FRAME_OUTPUT));

		frameOffsetKnob = createParam<FrameScrubKnob>(Vec(6, 294), module, ComputerscareBlankExpander::ZERO_OFFSET);
		frameOffsetKnob->module = module;

		addParam(frameOffsetKnob);
		addOutput(createOutput<PointingUpPentagonPort>(Vec(2, 320), module, ComputerscareBlankExpander::EOC_OUTPUT));
	}
	void step() {

		ModuleWidget::step();
	}
	FrameScrubKnob* frameOffsetKnob;
};
Model *modelComputerscareBlankExpander = createModel<ComputerscareBlankExpander, ComputerscareBlankExpanderWidget>("computerscare-blank-expander");
