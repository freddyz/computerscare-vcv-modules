#include "Computerscare.hpp"

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


	ComputerscareIso()  {

	config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    printf("ujje\n");
	}

};

struct ComputerscareIsoWidget : ModuleWidget {
	ComputerscareIsoWidget(ComputerscareIso *module) {
		
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(computerscarePluginInstance, "res/ComputerscareIsoPanel.svg")));


		/*box.size = Vec(15*9, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
				panel->box.size = box.size;
			 	panel->setBackground(SVG::load(assetPlugin(plugin,"res/ComputerscareIsoPanel.svg")));
	    		addChild(panel);
		}
  */

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
