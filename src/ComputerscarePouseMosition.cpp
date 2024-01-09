#include "Computerscare.hpp"

struct ComputerscarePouseMosition;


struct ComputerscarePouseMosition : Module {
	enum ParamIds {
		MANUAL_TRIGGER,
		MANUAL_CLEAR_TRIGGER,
		CLOCK_CHANNEL_FOCUS,
		INPUT_CHANNEL_FOCUS,
		SWITCH_VIEW,
		WHICH_CLOCK,
		NUM_PARAMS
	};
	enum InputIds {
		VAL_INPUT,
		TRG_INPUT,
		CLR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		X_MOUSE_POSITION,
		Y_MOUSE_POSITION,
		MOUSE_LEFT_CLICK,
		MOUSE_RIGHT_CLICK,
		MOUSE_WHEEL,
		MOUSE_BACK,
		MOUSE_FORWARD,
		MOUSE_WHEEL_CLICK,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	int stepCounter = 0;

	ComputerscarePouseMosition() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configOutput(X_MOUSE_POSITION, "X Mouse Position");
		configOutput(Y_MOUSE_POSITION, "Y Mouse Position");

	}
	void process(const ProcessArgs &args) override {
		stepCounter++;
		if (stepCounter > 25) {
			stepCounter = 0;

			math::Vec mousePos = APP->scene->getMousePos();

			math::Vec windowSize = APP->window->getSize();

			float xOut = 10*mousePos.x/windowSize.x;
			float yOut = 10*mousePos.y/windowSize.y;

			outputs[X_MOUSE_POSITION].setVoltage(xOut);
			outputs[Y_MOUSE_POSITION].setVoltage(yOut);
		
		}
	}

	
};



struct ComputerscarePouseMositionWidget : ModuleWidget {

	ComputerscarePouseMositionWidget(ComputerscarePouseMosition *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePouseMositionPanel.svg")));

		addOutput(createOutput<OutPort>(Vec(10, 320), module, ComputerscarePouseMosition::X_MOUSE_POSITION));
		addOutput(createOutput<OutPort>(Vec(50, 320), module, ComputerscarePouseMosition::Y_MOUSE_POSITION));

		
	}
	
	ComputerscarePouseMosition *debug;
};

Model *modelComputerscarePouseMosition = createModel<ComputerscarePouseMosition, ComputerscarePouseMositionWidget>("computerscare-pouse-mosition");
