#pragma once

using namespace rack;

struct AutoParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		std::string disp = Quantity::getDisplayValueString();
		return disp == "0" ? "Auto" : disp;
	}
};

struct ComputerscarePolyModule : Module {
	int polyChannels = 16;
	int polyChannelsKnobSetting = 0;
	int counterPeriod = 64;
	int counter = counterPeriod + 1;

	virtual void checkCounter() {
		counter++;
		if (counter > counterPeriod) {
			checkPoly();
			counter = 0;
		}
	}

	virtual void checkPoly() {};
};
struct TinyChannelsSnapKnob: RoundKnob {
	std::shared_ptr<Svg> manualChannelsSetSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-channels-empty-knob.svg"));
	std::shared_ptr<Svg> autoChannelsSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-channels-empty-knob-auto-mode.svg"));
	int prevSetting = -1;
	int paramId = -1;

	ComputerscarePolyModule *module;

	TinyChannelsSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-channels-empty-knob.svg")));
		shadow->opacity = 0.f;
		snap = true;
		RoundKnob();
	}
	void draw(const DrawArgs& args) override {
		if (module) {
			int currentSetting = module->params[paramId].getValue();;
			if (currentSetting != prevSetting) {
				setSvg(currentSetting == 0 ? autoChannelsSvg : manualChannelsSetSvg);
				prevSetting = currentSetting;
			}
		}
		else {

		}
		RoundKnob::draw(args);
	}
};

struct PolyChannelsDisplay : SmallLetterDisplay
{
	ComputerscarePolyModule *module;
	bool controlled = false;
	int prevChannels = -1;
	int paramId = -1;

	PolyChannelsDisplay(math::Vec pos)
	{
		box.pos = pos;
		fontSize = 14;
		letterSpacing = 1.f;
		textAlign = 18;
		textColor = BLACK;
		breakRowWidth = 20;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		if (module)
		{
			int newChannels = module->polyChannels;
			if (newChannels != prevChannels) {
				std::string str = std::to_string(newChannels);
				value = str;
				prevChannels = newChannels;
			}

		}
		else {
			value = std::to_string((random::u32() % 16) + 1);
		}
		SmallLetterDisplay::draw(args);
	}
};
struct PolyOutputChannelsWidget : Widget {
	ComputerscarePolyModule *module;
	PolyChannelsDisplay *channelCountDisplay;
	TinyChannelsSnapKnob *channelsKnob;
	PolyOutputChannelsWidget(math::Vec pos, ComputerscarePolyModule *mod, int paramId) {
		module = mod;

		channelsKnob = createParam<TinyChannelsSnapKnob>(pos.plus(Vec(7, 3)), module, paramId);
		channelsKnob->module = module;
		channelsKnob->paramId = paramId;

		channelCountDisplay = new PolyChannelsDisplay(pos);

		channelCountDisplay->module = module;

		addChild(channelsKnob);
		addChild(channelCountDisplay);
	}
};