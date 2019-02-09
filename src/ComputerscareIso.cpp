#include "Computerscare.hpp"
#include "dtpulse.hpp"
#include "dsp/digital.hpp"
#include "window.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>


struct ComputerscareIso;


struct ComputerscareIso : Module {
	enum ParamIds {

		NUM_PARAMS
		
	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
	
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareIso() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    printf("ujje\n");
	}

};

struct ComputerscareIsoWidget : ModuleWidget {
  float randAmt = 1.f;
	ComputerscareIsoWidget(ComputerscareIso *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareIsoPanel.svg")));

  }
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareIso = Model::create<ComputerscareIso, ComputerscareIsoWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
