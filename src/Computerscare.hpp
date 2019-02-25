#pragma once

#include "rack.hpp"

#include "app/common.hpp"
#include "widget/TransparentWidget.hpp"
#include "widget/FramebufferWidget.hpp"
#include "widget/SvgWidget.hpp"
#include "app/PortWidget.hpp"
#include "app/CircularShadow.hpp"
#include "app.hpp"



using namespace rack;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *pluginInstance;


/*
	Toly Poolkit
	Wally Porkshop
*/
// Forward-declare each Model, defined in each module source file
extern Model *modelComputerscareDebug;

//extern Model *modelComputerscarePatchSequencer;
//extern Model *modelComputerscareLaundrySoup;
//extern Model *modelComputerscareILoveCookies;
//extern Model *modelComputerscareOhPeas;
extern Model *modelComputerscareIso;
extern Model *modelComputerscareKnolyPobs;

static const NVGcolor COLOR_COMPUTERSCARE_LIGHT_GREEN = nvgRGB(0xC0, 0xE7, 0xDE);
static const NVGcolor COLOR_COMPUTERSCARE_GREEN = nvgRGB(0x24, 0xc9, 0xa6);
static const NVGcolor COLOR_COMPUTERSCARE_RED = nvgRGB(0xC4, 0x34, 0x21);
static const NVGcolor COLOR_COMPUTERSCARE_YELLOW = nvgRGB(0xE4, 0xC4, 0x21);
static const NVGcolor COLOR_COMPUTERSCARE_BLUE = nvgRGB(0x24, 0x44, 0xC1);
static const NVGcolor COLOR_COMPUTERSCARE_PINK = nvgRGB(0xAA, 0x18, 0x31);
static const NVGcolor COLOR_COMPUTERSCARE_TRANSPARENT = nvgRGBA(0x00, 0x00,0x00,0x00);


namespace rack {
namespace app {



struct ComputerscareSVGPanel;

struct ComputerscareSVGPanel : widget::FramebufferWidget {
	void step() override;
	void setBackground(std::shared_ptr<Svg> svg);
};


struct ComputerscareSvgPort : PortWidget {
	widget::FramebufferWidget *fb;
	widget::SvgWidget *sw;
	CircularShadow *shadow;

	ComputerscareSvgPort();
	void setSvg(std::shared_ptr<Svg> svg);
	DEPRECATED void setSVG(std::shared_ptr<Svg> svg) {setSvg(svg);}
};



} // namespace app
} // namespace rack


struct IsoButton : SvgSwitch {
	IsoButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-iso-button-down.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-iso-button-up.svg")));
	}
};
struct ComputerscareIsoThree : app::SvgSwitch {
	ComputerscareIsoThree() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/iso-3way-1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/iso-3way-2.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/iso-3way-3.svg")));
	}
};
struct ThreeVerticalXSwitch : app::SvgSwitch {
	ThreeVerticalXSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/vertical-x-1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/vertical-x-2.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/vertical-x-3.svg")));
	}
};
struct ComputerscareDebugFour : app::SvgSwitch {
	ComputerscareDebugFour() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/debug-clock-selector-4way-template.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/debug-clock-selector-4way-template.svg")));

		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/debug-clock-selector-4way-template.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/debug-clock-selector-4way-template.svg")));	
	}
};
struct ComputerscareResetButton : SvgSwitch {
	ComputerscareResetButton() {
		momentary=true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-rst-text.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-rst-text-red.svg")));
		//APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-pentagon-jack-1-outline-flipped.svg"));
	}
};

struct ComputerscareClockButton : SvgSwitch {
	ComputerscareClockButton() {
		momentary=true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-clk-text.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-clk-text-red.svg")));
	}
};
struct ComputerscareInvisibleButton : SvgSwitch {
	ComputerscareInvisibleButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-invisible-button.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/computerscare-invisible-button-frame2.svg")));
	}
};

struct ComputerscareGreenLight : GrayModuleLightWidget {
	ComputerscareGreenLight() {
		addBaseColor(COLOR_COMPUTERSCARE_GREEN);
	}
};
/*
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

*/


struct OutPort : ComputerscareSvgPort {
	OutPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-pentagon-jack-1-outline-flipped.svg")));
		//background->wrap();
	}
};

struct PointingUpPentagonPort : ComputerscareSvgPort {
	PointingUpPentagonPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-pentagon-jack-pointing-up.svg")));
		//background->wrap();
		//box.size = background->box.size;
	}
};

struct InPort : ComputerscareSvgPort {
	InPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-pentagon-jack-1-outline.svg")));
		//background->wrap();
		//box.size = background->box.size;
	}
};



 // Knobs


struct LrgKnob : RoundBlackSnapKnob {
	LrgKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-big-knob-effed.svg")));	
	}
	//void randomize() override { return; }	
};


struct MediumSnapKnob : RoundBlackSnapKnob {
	MediumSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-effed.svg")));	
	}	
};

struct SmoothKnob : RoundKnob {
	SmoothKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-effed.svg")));
	}
};
struct SmallKnob : RoundKnob {
	SmallKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg")));
	}
};
struct SmallSnapKnob : RoundBlackSnapKnob {
	//bool visible = true;
	SmallSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg")));
	}

};
struct BigSmoothKnob : RoundKnob {
	BigSmoothKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-big-knob-effed.svg")));
	}
};
struct ComputerscareDotKnob : SmallKnob {
	ComputerscareDotKnob() {
		
	}
};


////////////////////////////////////
struct SmallLetterDisplay : TransparentWidget {

  std::string value;
  std::shared_ptr<Font> font;
  int fontSize = 19;
  std::string defaultFontPath = "res/Oswald-Regular.ttf";
  NVGcolor baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;

  float letterSpacing = 2.5;
  int textAlign = 1;
  bool active = false;
  bool blink = false;
  bool doubleblink = false;

  SmallLetterDisplay() {
    font = APP->window->loadFont(asset::plugin(pluginInstance,defaultFontPath));
  };
  SmallLetterDisplay(std::string fontPath) {
    font = APP->window->loadFont(asset::plugin(pluginInstance,fontPath));
  };

  void draw(const DrawArgs &ctx) override
  {  
    // Background
    NVGcolor backgroundColor = COLOR_COMPUTERSCARE_RED;
    NVGcolor doubleblinkColor = COLOR_COMPUTERSCARE_YELLOW;

    nvgBeginPath(ctx.vg);
    nvgRoundedRect(ctx.vg, 1.0, -1.0, box.size.x-3, box.size.y-3, 4.0);
    if(doubleblink) {
      nvgFillColor(ctx.vg, doubleblinkColor);
    }
    else {
      if(blink) {
        nvgFillColor(ctx.vg, backgroundColor);
      }
      else {
      	nvgFillColor(ctx.vg, baseColor);
      }
    }
    nvgFill(ctx.vg);

    // text 
    nvgFontSize(ctx.vg, fontSize);
    nvgFontFaceId(ctx.vg, font->handle);
    nvgTextLetterSpacing(ctx.vg, letterSpacing);
    nvgTextLineHeight(ctx.vg, 0.7);
    nvgTextAlign(ctx.vg,textAlign);

    Vec textPos = Vec(6.0f, 12.0f);   
    NVGcolor textColor = (!blink || doubleblink) ? nvgRGB(0x10, 0x10, 0x00) : COLOR_COMPUTERSCARE_YELLOW;
    nvgFillColor(ctx.vg, textColor);
    nvgTextBox(ctx.vg, textPos.x, textPos.y,80,value.c_str(), NULL);

  }
};
