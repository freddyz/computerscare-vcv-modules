#include "Computerscare.hpp"
#include <patch.hpp>

struct ComputerscarePatchVersioning;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscarePatchVersioning : Module {
	int counter = 0;
	ComputerscareSVGPanel* panelRef;
	rack::dsp::SchmittTrigger saveTrigger;
	enum ParamIds {
		KNOB,
		SAVE_BUTTON,
		NUM_PARAMS

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


	ComputerscarePatchVersioning()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		/* for (int i = 0; i < numKnobs; i++) {
				configParam(KNOB + i, 0.0f, 10.0f, 0.0f);
				configParam(KNOB+i, 0.f, 10.f, 0.f, "Channel "+std::to_string(i+1) + " Voltage", " Volts");
		}
		configParam(TOGGLES, 0.0f, 1.0f, 0.0f);
		outputs[POLY_OUTPUT].setChannels(16);*/
	}
	void process(const ProcessArgs &args) override {
		 bool saveClicked = saveTrigger.process(params[SAVE_BUTTON].getValue());
		 if(saveClicked) {
		 	std::string patchName = APP->patch->path+".poopoo";
		 	DEBUG("patch name %s",patchName.c_str());
		 	APP->patch->save(patchName);
		 }
	}

};

struct ComputerscarePatchVersioningWidget : ModuleWidget {
	ComputerscarePatchVersioningWidget(ComputerscarePatchVersioning *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePatchVersioningPanel.svg")));

		float outputY = 334;
		box.size = Vec(15 * 10, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareIsoPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}

		addParam(createParam<IsoButton>(Vec(50, 100), module, ComputerscarePatchVersioning::SAVE_BUTTON));





	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscarePatchVersioning *module, int index, int type) {
		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(60, 30);
		smallLetterDisplay->value = label;
		if (type == 0)  {
			addParam(createParam<SmoothKnob>(Vec(x, y), module, ComputerscarePatchVersioning::KNOB + index));
			smallLetterDisplay->box.pos = Vec(x + 22, y + 2);
		}
		else if (type == 1) {
			addParam(createParam<SmallKnob>(Vec(x, y), module, ComputerscarePatchVersioning::KNOB + index));
			smallLetterDisplay->box.pos = Vec(x + 12, y - 10);
		}
		else if (type == 2) {
			addParam(createParam<BigSmoothKnob>(Vec(x, y), module, ComputerscarePatchVersioning::KNOB + index));
			smallLetterDisplay->box.pos = Vec(x + 22, y - 12);
		}
		else if (type == 3) {
			addParam(createParam<LrgKnob>(Vec(x, y), module, ComputerscarePatchVersioning::KNOB + index));
			smallLetterDisplay->box.pos = Vec(x + 22, y - 12);
		}
		else if (type == 4) {
			addParam(createParam<BigSmoothKnob>(Vec(x, y), module, ComputerscarePatchVersioning::KNOB + index));
			smallLetterDisplay->box.pos = Vec(x + 22, y - 12);
		}

		else  {
			addParam(createParam<MediumSnapKnob>(Vec(x, y), module, ComputerscarePatchVersioning::KNOB + index));
			smallLetterDisplay->box.pos = Vec(x + 12, y - 10);
		}
		addChild(smallLetterDisplay);

	}
	SmallLetterDisplay* smallLetterDisplay;
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscarePatchVersioning = Model::create<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscarePatchVersioning = createModel<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare-patch-versioning");
