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


extern Model *modelComputerscarePatchSequencer;
extern Model *modelComputerscareLaundrySoup;
extern Model *modelComputerscareILoveCookies;
extern Model *modelComputerscareOhPeas;
//extern Model *modelComputerscareIso;
extern Model *modelComputerscareKnolyPobs;
extern Model *modelComputerscareBolyPuttons;
extern Model *modelComputerscareRolyPouter;
extern Model *modelComputerscareTolyPools;
extern Model *modelComputerscareSolyPequencer;
extern Model *modelComputerscareFolyPace;
extern Model *modelComputerscareStolyFickPigure;
extern Model *modelComputerscareBlank;

static const NVGcolor COLOR_COMPUTERSCARE_LIGHT_GREEN = nvgRGB(0xC0, 0xE7, 0xDE);
static const NVGcolor COLOR_COMPUTERSCARE_GREEN = nvgRGB(0x24, 0xc9, 0xa6);
static const NVGcolor COLOR_COMPUTERSCARE_RED = nvgRGB(0xC4, 0x34, 0x21);
static const NVGcolor COLOR_COMPUTERSCARE_YELLOW = nvgRGB(0xE4, 0xC4, 0x21);
static const NVGcolor COLOR_COMPUTERSCARE_BLUE = nvgRGB(0x24, 0x44, 0xC1);
static const NVGcolor COLOR_COMPUTERSCARE_PINK = nvgRGB(0xAA, 0x18, 0x31);
static const NVGcolor COLOR_COMPUTERSCARE_TRANSPARENT = nvgRGBA(0x00, 0x00, 0x00, 0x00);


namespace rack {
namespace app {



struct ComputerscareSVGPanel;

struct ComputerscareSVGPanel : widget::FramebufferWidget {
	NVGcolor backgroundColor;
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
struct BGPanel : Widget {
	NVGcolor color;

	BGPanel(NVGcolor _color) {
		color = _color;
	}

	void step() override {
		Widget::step();
	}

	void draw(const DrawArgs &args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
		Widget::draw(args);
	}
};

struct IsoButton : SvgSwitch {
	IsoButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-iso-button-down.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-iso-button-up.svg")));
	}
};
struct SmallIsoButton : app::SvgSwitch {
	SmallIsoButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-iso-button-small-up.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-iso-button-small-down.svg")));
		shadow->opacity = 0.f;
	}
};
struct ComputerscareIsoThree : app::SvgSwitch {
	ComputerscareIsoThree() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/iso-3way-1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/iso-3way-2.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/iso-3way-3.svg")));
	}
};
struct ThreeVerticalXSwitch : app::SvgSwitch {
	ThreeVerticalXSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vertical-x-1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vertical-x-2.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vertical-x-3.svg")));
		shadow->opacity = 0.f;
	}
};
struct ComputerscareDebugFour : app::SvgSwitch {
	ComputerscareDebugFour() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/debug-clock-selector-4way-template.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/debug-clock-selector-4way-template.svg")));

		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/debug-clock-selector-4way-template.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/debug-clock-selector-4way-template.svg")));
	}
};
struct ComputerscareResetButton : app::SvgSwitch {
	ComputerscareResetButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-rst-text.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-rst-text-red.svg")));
		//APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-pentagon-jack-1-outline-flipped.svg"));
	}
};

struct ComputerscareClockButton : app::SvgSwitch {
	ComputerscareClockButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-clk-text.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-clk-text-red.svg")));
	}
};
struct ComputerscareInvisibleButton : app::SvgSwitch {
	ComputerscareInvisibleButton() {
		momentary = true;


		fb = new widget::FramebufferWidget;
		addChild(fb);

		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-invisible-button.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-invisible-button-frame2.svg")));



		sw = new widget::SvgWidget;
		fb->addChild(sw);


	}
	void addFrame(std::shared_ptr<Svg> svg) {
		frames.push_back(svg);
		// If this is our first frame, automatically set SVG and size
		if (!sw->svg) {
			sw->setSvg(svg);
			box.size = sw->box.size;
			fb->box.size = sw->box.size;
		}
	}
};

struct ComputerscareGreenLight : app::ModuleLightWidget {
	ComputerscareGreenLight() {
		addBaseColor(COLOR_COMPUTERSCARE_GREEN);
	}
};

struct ComputerscareRedLight : app::ModuleLightWidget {
	ComputerscareRedLight() {
		bgColor = nvgRGBA(0x5a, 0x5a, 0x5a, 0x00);
		borderColor = nvgRGBA(0, 0, 0, 0x00);
		addBaseColor(COLOR_COMPUTERSCARE_RED);
	}
};
/*
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




*/
template <typename BASE>
struct MuteLight : BASE {
	MuteLight() {
		this->box.size = mm2px(Vec(6.f, 6.f));
	}
};

template <typename BASE>
struct ComputerscareHugeLight : BASE  {
	ComputerscareHugeLight() {
		this->box.size = mm2px(Vec(8.179, 8.179));
	}
};

template <typename BASE>
struct ComputerscareMediumLight : BASE {
	ComputerscareMediumLight() {
		this->box.size = mm2px(Vec(6, 6));
	}
};

template <typename BASE>
struct ComputerscareSmallLight :  BASE {
	ComputerscareSmallLight() {
		this->box.size = mm2px(Vec(3, 3));
	}
};

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
	void randomize() override { return; }
};


struct MediumSnapKnob : RoundBlackSnapKnob {
	MediumSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-effed.svg")));
	}
};
struct MediumDotSnapKnob : RoundBlackSnapKnob {
	MediumDotSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-medium-knob-dot-indicator.svg")));
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

	//CircularShadow *shadow;
	SmallSnapKnob() {

		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-small-knob-effed.svg")));
		shadow->box.size = math::Vec(0, 0);
		shadow->opacity = 0.f;
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

struct ComputerscareTextField : ui::TextField {
	std::shared_ptr<Font> font;
	math::Vec textOffset;
	NVGcolor color = COLOR_COMPUTERSCARE_LIGHT_GREEN;
	int fontSize = 16;
	bool inError = false;
	ComputerscareTextField() {

		font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		color = nvgRGB(0xff, 0xd7, 0x14);
		textOffset = math::Vec(1, 2);
	}


	void draw(const DrawArgs &args) override {
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));

		// Background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 5.0);
		if (inError) {
			nvgFillColor(args.vg, COLOR_COMPUTERSCARE_PINK);
		}
		else {
			nvgFillColor(args.vg, nvgRGB(0x00, 0x00, 0x00));
		}
		nvgFill(args.vg);

		// Text
		if (font->handle >= 0) {
			bndSetFont(font->handle);

			NVGcolor highlightColor = color;
			highlightColor.a = 0.5;
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y,
			                  box.size.x - 2 * textOffset.x, box.size.y - 2 * textOffset.y,
			                  -1, color, fontSize, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}

		nvgResetScissor(args.vg);
	}
	int getTextPosition(Vec mousePos) override {
		bndSetFont(font->handle);
		int textPos = bndIconLabelTextPosition(APP->window->vg, textOffset.x, textOffset.y,
		                                       box.size.x - 2 * textOffset.x, box.size.y - 2 * textOffset.y,
		                                       -1, fontSize, text.c_str(), mousePos.x, mousePos.y);
		bndSetFont(APP->window->uiFont->handle);
		return textPos;
	}
};
////////////////////////////////////
struct SmallLetterDisplay : Widget {

	std::string value;
	std::shared_ptr<Font> font;
	int fontSize = 19;
	std::string defaultFontPath = "res/Oswald-Regular.ttf";
	NVGcolor baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
	NVGcolor textColor = nvgRGB(0x10, 0x10, 0x00);

	float letterSpacing = 2.5;
	int textAlign = 1;
	bool active = false;
	bool blink = false;
	bool doubleblink = false;
	float breakRowWidth = 80.f;

	SmallLetterDisplay() {
		value = "";
		font = APP->window->loadFont(asset::plugin(pluginInstance, defaultFontPath));
	};
	SmallLetterDisplay(std::string fontPath) {
		value = "";
		font = APP->window->loadFont(asset::plugin(pluginInstance, fontPath));
	};

	void draw(const DrawArgs &ctx) override
	{
		// Background
		NVGcolor backgroundColor = COLOR_COMPUTERSCARE_RED;
		NVGcolor doubleblinkColor = COLOR_COMPUTERSCARE_YELLOW;

		nvgBeginPath(ctx.vg);
		nvgRoundedRect(ctx.vg, 1.0, -1.0, box.size.x - 3, box.size.y - 3, 4.0);
		if (doubleblink) {
			nvgFillColor(ctx.vg, doubleblinkColor);
		}
		else {
			if (blink) {
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
		nvgTextAlign(ctx.vg, textAlign);

		Vec textPos = Vec(6.0f, 12.0f);
		NVGcolor color = (!blink || doubleblink) ? textColor : COLOR_COMPUTERSCARE_YELLOW;
		nvgFillColor(ctx.vg, color);
		nvgTextBox(ctx.vg, textPos.x, textPos.y, breakRowWidth, value.c_str(), NULL);

	}
};
