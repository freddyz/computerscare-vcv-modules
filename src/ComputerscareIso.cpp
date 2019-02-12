#include "Computerscare.hpp"

struct ComputerscareIso;

const int numKnobs = 16;

const int numToggles = 16;

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
		NUM_OUTPUTS
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


		box.size = Vec(15*9, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
				panel->box.size = box.size;
			 	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComputerscareIsoPanel.svg")));
	    		


	    		addChild(panel);
		}
		addParam(createParam<Davies1900hBlackKnob>(Vec(28, 87), module, ComputerscareIso::KNOB));
		addParam(createParam<MediumSnapKnob>(Vec(68, 97), module, ComputerscareIso::KNOB+1));
		addParam(createParam<SmoothKnob>(Vec(68, 127), module, ComputerscareIso::KNOB+2));
		addParam(createParam<SmallKnob>(Vec(68, 157), module, ComputerscareIso::KNOB+3));
		addParam(createParam<BigSmoothKnob>(Vec(68, 187), module, ComputerscareIso::KNOB+4));
		addParam(createParam<MediumSnapKnob>(Vec(68, 267), module, ComputerscareIso::KNOB+5));

		addParam(createParam<IsoButton>(Vec(20, 44), module, ComputerscareIso::TOGGLES));

		//addInput(createInput<PJ301MPort>(Vec(33, 186), module, MyModule::PITCH_INPUT));

		addOutput(createOutput<OutPort>(Vec(33, 275), module, ComputerscareIso::POLY_OUTPUT));

		//addChild(createLight<MediumLight<RedLight>>(Vec(41, 59), module, MyModule::BLINK_LIGHT));
  

}

  /*void drawShadow(NVGcontext *vg) {
	nvgBeginPath(vg);
	float r = 20; // Blur radius
	float c = 20; // Corner radius
	Vec b = Vec(-10, 30); // Offset from each corner
	nvgRect(vg, b.x - r, b.y - r, box.size.x - 2*b.x + 2*r, box.size.y - 2*b.y + 2*r);
	NVGcolor shadowColor = nvgRGBAf(0, 220, 0, 0.2);
	NVGcolor transparentColor = nvgRGBAf(0, 0, 0, 0);
	nvgFillPaint(vg, nvgBoxGradient(vg, b.x, b.y, box.size.x - 2*b.x, box.size.y - 2*b.y, c, r, shadowColor, transparentColor));
	nvgFill(vg);
}*/

};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscareIso = Model::create<ComputerscareIso, ComputerscareIsoWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscareIso = createModel<ComputerscareIso, ComputerscareIsoWidget>("Isopig");
