#pragma once
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

static const NVGcolor COLOR_COMPUTERSCARE_LIGHT_GREEN = nvgRGB(0xC0, 0xE7, 0xDE);
static const NVGcolor COLOR_COMPUTERSCARE_GREEN = nvgRGB(0x24, 0xc9, 0xa6);
static const NVGcolor COLOR_COMPUTERSCARE_RED = nvgRGB(0xC4, 0x34, 0x21);
static const NVGcolor COLOR_COMPUTERSCARE_YELLOW = nvgRGB(0xE4, 0xC4, 0x21);
static const NVGcolor COLOR_COMPUTERSCARE_BLUE = nvgRGB(0x24, 0x44, 0xC1);
static const NVGcolor COLOR_COMPUTERSCARE_TRANSPARENT = nvgRGBA(0x00, 0x00,0x00,0x00);


// Forward-declare each Model, defined in each module source file
extern Model *modelComputerscareDebug;
extern Model *modelComputerscarePatchSequencer;
extern Model *modelComputerscareLaundrySoup;
extern Model *modelComputerscareILoveCookies;


struct ComputerscareResetButton : SVGSwitch,MomentarySwitch {
	ComputerscareResetButton() {
		addFrame(SVG::load(assetPlugin(plugin,"res/computerscare-rst-text.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/computerscare-rst-text-red.svg")));
		//SVG::load(assetPlugin(plugin, "res/computerscare-pentagon-jack-1-outline-flipped.svg"));
	}
};
struct ComputerscareClockButton : SVGSwitch,MomentarySwitch {
	ComputerscareClockButton() {
		addFrame(SVG::load(assetPlugin(plugin,"res/computerscare-clk-text.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/computerscare-clk-text-red.svg")));
	}
};
struct ComputerscareInvisibleButton : SVGSwitch,MomentarySwitch {
	ComputerscareInvisibleButton() {
		addFrame(SVG::load(assetPlugin(plugin,"res/computerscare-invisible-button.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/computerscare-invisible-button-frame2.svg")));
		//SVG::load(assetPlugin(plugin, "res/computerscare-pentagon-jack-1-outline-flipped.svg"));
	}
};

struct ComputerscareGreenLight : GrayModuleLightWidget {
	ComputerscareGreenLight() {
		addBaseColor(COLOR_COMPUTERSCARE_GREEN);
	}
};

struct ComputerscareRedLight : ModuleLightWidget {
	ComputerscareRedLight() {
		bgColor = nvgRGBA(0x5a, 0x5a, 0x5a, 0x00);
		borderColor = nvgRGBA(0, 0, 0, 0x00);
		addBaseColor(COLOR_COMPUTERSCARE_RED);
	}	
};
struct ComputerscareYellowLight : ModuleLightWidget {
	ComputerscareYellowLight() {
		bgColor = nvgRGBA(0x5a, 0x5a, 0x5a, 0x00);
		borderColor = nvgRGBA(0, 0, 0, 0x00);
		addBaseColor(COLOR_COMPUTERSCARE_YELLOW);
	}	
};
struct ComputerscareBlueLight : ModuleLightWidget {
	ComputerscareBlueLight() {
		bgColor = nvgRGBA(0x5a, 0x5a, 0x5a, 0x00);
		borderColor = nvgRGBA(0, 0, 0, 0x00);
		addBaseColor(COLOR_COMPUTERSCARE_BLUE);
	}	
};


template <typename BASE>
struct ComputerscareHugeLight : BASE {
	ComputerscareHugeLight() {
		this->box.size = mm2px(Vec(8.179, 8.179));
	}
};
template <typename BASE>
struct ComputerscareMediumLight : BASE {
	ComputerscareMediumLight() {
		this->box.size = mm2px(Vec(6,6));
	}
};
template <typename BASE>
struct ComputerscareSmallLight : BASE {
	ComputerscareSmallLight() {
		this->box.size = mm2px(Vec(3,3));
	}
};


struct OutPort : SVGPort {
	OutPort() {
		background->svg = SVG::load(assetPlugin(plugin, "res/computerscare-pentagon-jack-1-outline-flipped.svg"));
		background->wrap();
		box.size = background->box.size;
	}
};

struct PointingUpPentagonPort : SVGPort {
	PointingUpPentagonPort() {
		background->svg = SVG::load(assetPlugin(plugin, "res/computerscare-pentagon-jack-pointing-up.svg"));
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
		box.size = Vec(32,32);
	}
	void randomize() override { return; }	
};
struct SmoothKnob : RoundKnob {
	SmoothKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/computerscare-medium-knob-effed.svg")));
	}
};


////////////////////////////////////
struct SmallLetterDisplay : TransparentWidget {

  std::string value;
  std::shared_ptr<Font> font;
  int fontSize = 19;
  std::string defaultFontPath = "res/Oswald-Regular.ttf";

  float letterSpacing = 2.5;
  int textAlign = 1;
  bool active = false;
  bool blink = false;
  bool doubleblink = false;

  SmallLetterDisplay() {
    font = Font::load(assetPlugin(plugin,defaultFontPath));
  };
  SmallLetterDisplay(std::string fontPath) {
    font = Font::load(assetPlugin(plugin,fontPath));
  };

  void draw(NVGcontext *vg) override
  {  
    // Background
    NVGcolor backgroundColor = COLOR_COMPUTERSCARE_RED;
    NVGcolor doubleblinkColor = COLOR_COMPUTERSCARE_YELLOW;

    
    if(doubleblink) {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, 1.0, -1.0, box.size.x-3, box.size.y-3, 8.0);
      nvgFillColor(vg, doubleblinkColor);
      nvgFill(vg);
    }
    else {
        if(blink) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 1.0, -1.0, box.size.x-3, box.size.y-3, 8.0);
        nvgFillColor(vg, backgroundColor);
        nvgFill(vg);
      }
    }

    // text 
    nvgFontSize(vg, fontSize);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, letterSpacing);
    nvgTextLineHeight(vg, 0.7);
    nvgTextAlign(vg,textAlign);

    Vec textPos = Vec(6.0f, 12.0f);   
    NVGcolor textColor = (!blink || doubleblink) ? nvgRGB(0x10, 0x10, 0x00) : COLOR_COMPUTERSCARE_YELLOW;
    nvgFillColor(vg, textColor);
    nvgTextBox(vg, textPos.x, textPos.y,80,value.c_str(), NULL);

  }
};
