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
extern Model *modelComputerscareRouter;

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
struct CS_Knob : RoundKnob {
	 CS_Knob() {
		 //box.size = Vec(20, 20);		//TS_RoundBlackKnob_20
		 //setSVG(SVG::load(assetPlugin(plugin, "res/ComponentLibrary/TS_RoundBlackKnob_20.svg")));
		 ///// TODO: Make small SVG. Make all original SVGs (no more reliance on built-in controls except for base class for behavior).
		 //this->sw->svg = SVG::load(assetGlobal("res/ComponentLibrary/RoundSmallBlackKnob.svg"));
		 ////sw->setSVG(svg);
		 //sw->box.size = box.size;
		 //tw->box.size = sw->box.size;
		 ////box.size = sw->box.size;
		 //shadow->box.size = sw->box.size;
		 //shadow->box.pos = Vec(0, sw->box.size.y * 0.1);

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

/*struct MedKnob : RoundKnob {
	MedKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/components/MedKnob.svg")));
		box.size = Vec(24,24);

	}
};

struct SmlKnob : RoundKnob {
	SmlKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/components/SmlKnob.svg")));
		box.size = Vec(20,20);
	}
};*/