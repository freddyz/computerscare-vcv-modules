#include "Computerscare.hpp"

struct ComputerscareIso;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareIso : Module {
	enum ParamIds {
		KNOB,
		TOGGLES = KNOB + numKnobs,
		NUM_PARAMS = TOGGLES+numToggles
		
	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS=POLY_OUTPUT + numOutputs
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareIso()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		

	    printf("ujje\n");
	    for (int i = 0; i < numKnobs; i++) {

				params[KNOB + i].config(0.0f, 10.0f, 0.0f);
				params[KNOB+i].config(0.f, 10.f, 0.f, "Channel "+std::to_string(i) + " Voltage", " Volts");
		}
		params[TOGGLES].config(0.0f, 1.0f, 0.0f);
		outputs[POLY_OUTPUT].setChannels(16);
	}
	void step() override {
		for (int i = 0; i < numKnobs; i++) {
			outputs[POLY_OUTPUT].setVoltage(params[KNOB+i].getValue(),i);
		}

	}

};

struct ComputerscareIsoWidget : ModuleWidget {
	ComputerscareIsoWidget(ComputerscareIso *module) {
		
		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareIsoPanel.svg")));

		float outputY = 334;
		box.size = Vec(15*9, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
				panel->box.size = box.size;
			 	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComputerscareIsoPanel.svg")));
	    		


	    		addChild(panel);
		}

		addParam(createParam<IsoButton>(Vec(10, 5), module, ComputerscareIso::TOGGLES));
		addParam(createParam<ComputerscareClockButton>(Vec(10,40),module,ComputerscareIso::TOGGLES+2));
		addParam(createParam<ComputerscareResetButton>(Vec(55,40),module,ComputerscareIso::TOGGLES+1));


		smallLetterDisplay = new SmallLetterDisplay();
      smallLetterDisplay->box.pos = Vec(20,77);
      smallLetterDisplay->box.size = Vec(60, 30);
      smallLetterDisplay->value = "1";
      smallLetterDisplay->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      addChild(smallLetterDisplay);


		addParam(createParam<Davies1900hBlackKnob>(Vec(28, 87), module, ComputerscareIso::KNOB));
		addParam(createParam<MediumSnapKnob>(Vec(68, 97), module, ComputerscareIso::KNOB+1));
		

		addParam(createParam<SmoothKnob>(Vec(30, 147), module, ComputerscareIso::KNOB+2));
		addParam(createParam<SmallKnob>(Vec(62, 147), module, ComputerscareIso::KNOB+3));
		addParam(createParam<BigSmoothKnob>(Vec(68, 187), module, ComputerscareIso::KNOB+4));
		addParam(createParam<MediumSnapKnob>(Vec(68, 267), module, ComputerscareIso::KNOB+5));


		addOutput(createOutput<OutPort>(Vec(33, outputY), module, ComputerscareIso::POLY_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(63, outputY), module, ComputerscareIso::POLY_OUTPUT+1));
		addOutput(createOutput<InPort>(Vec(93, outputY), module, ComputerscareIso::POLY_OUTPUT+2));

}
SmallLetterDisplay* smallLetterDisplay;
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscareIso = Model::create<ComputerscareIso, ComputerscareIsoWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscareIso = createModel<ComputerscareIso, ComputerscareIsoWidget>("Isopig");
