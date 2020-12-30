#include "Computerscare.hpp"

struct ComputerscareBlankExpander : Module {
	float leftMessages[2][1] = {};
	bool isConnected = false;
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		SCAN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::SchmittTrigger eocMessageReadTrigger;
	dsp::PulseGenerator eocPulse;
	dsp::PulseGenerator eachFramePulse;


	ComputerscareBlankExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
	}

	void process(const ProcessArgs &args) override {
		if (leftExpander.module && leftExpander.module->model == modelComputerscareBlank) {
			// Get consumer message
			float *message = (float*) leftExpander.consumerMessage;
			isConnected = true;

			//eocMessageReadTrigger.process(message[0]);
			if (eocMessageReadTrigger.process(message[0])) {
				eocPulse.trigger(1e-3);
			}
			outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

			leftExpander.module->rightExpander.messageFlipRequested = true;
		}
		else {
		//	isConnected = false;
			// No mother module is connected.
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

		DEBUG("clock_input:%i", ComputerscareBlankExpander::CLOCK_INPUT);

		float inStartY = 30;
		float dY = 40;

		float outStartY = 250;

		addInput(createInput<InPort>(Vec(2, inStartY), module, ComputerscareBlankExpander::CLOCK_INPUT));
		addInput(createInput<InPort>(Vec(2, inStartY + dY), module, ComputerscareBlankExpander::SCAN_INPUT));
		addInput(createInput<InPort>(Vec(2, inStartY + 2 * dY), module, ComputerscareBlankExpander::RESET_INPUT));

		addOutput(createOutput<PointingUpPentagonPort>(Vec(2, outStartY), module, ComputerscareBlankExpander::EOC_OUTPUT));


	}
};
Model *modelComputerscareBlankExpander = createModel<ComputerscareBlankExpander, ComputerscareBlankExpanderWidget>("computerscare-blank-expander");
