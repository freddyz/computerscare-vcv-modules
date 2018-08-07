#include "rack.hpp"


using namespace rack;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *plugin;

#ifndef COLOR_MAGENTA
	#define COLOR_MAGENTA nvgRGB(240, 50, 230)
#endif
#ifndef COLOR_LIME
	#define COLOR_LIME nvgRGB(210, 245, 60)
#endif
#ifndef COLOR_PINK
	#define COLOR_PINK nvgRGB(250, 190, 190)
#endif

// Forward-declare each Model, defined in each module source file
extern Model *modelComputerscareDebug;
extern Model *modelComputerscarePatchSequencer;

struct OutPort : SVGPort {
	OutPort() {
		background->svg = SVG::load(assetPlugin(plugin, "res/09 Output Plug.svg"));
		background->wrap();
		box.size = background->box.size;
	}
};

struct InPort : SVGPort {
	InPort() {
		background->svg = SVG::load(assetPlugin(plugin, "res/computerscare-pentagon-jack-1-outline.svg"));
		background->wrap();
		box.size = background->box.size;
	}
};



 // Knobs

struct LrgKnob : RoundBlackSnapKnob {
	LrgKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/computerscare-big-knob-effed.svg")));
	//void randomize() override;	
		box.size = Vec(32,32);
	}
	void randomize() override { return; }	
};
