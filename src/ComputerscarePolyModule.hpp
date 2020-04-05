#pragma once

using namespace rack;

struct ComputerscarePolyModule : Module {
	int polyChannels = 16;
	int polyChannelsKnobSetting=0;
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
struct TinyChannelsSnapKnob: RoundBlackSnapKnob {
	std::shared_ptr<Svg> manualChannelsSetSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-channels-empty-knob.svg"));
	std::shared_ptr<Svg> autoChannelsSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-channels-empty-knob-auto-mode.svg"));
	int prevSetting=-1;
	int paramId=-1;

	ComputerscarePolyModule *module;

	TinyChannelsSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/computerscare-channels-empty-knob.svg")));
	}
	void randomize() override {return;}
	void draw(const DrawArgs& args) override {
		if (module) {
			int currentSetting = module->params[paramId].getValue();;
			if (currentSetting != prevSetting) {
				setSvg(currentSetting == 0 ? autoChannelsSvg : manualChannelsSetSvg);
				dirtyValue = -20.f;
				prevSetting = currentSetting;
			}
		}
		else {
		}
		RoundBlackSnapKnob::draw(args);
	}
};

struct PolyChannelsDisplay : SmallLetterDisplay
{
	ComputerscarePolyModule *module;
	bool controlled=false;
	int prevChannels=-1;
	int paramId=-1;

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
			if(newChannels != prevChannels) {
				std::string str = std::to_string(newChannels);
				value = str;
				prevChannels=newChannels;
			}
			
		}
		SmallLetterDisplay::draw(args);
	}
};
struct PolyOutputChannelsWidget : Widget {
	ComputerscarePolyModule *module;
	PolyChannelsDisplay *channelCountDisplay;
	TinyChannelsSnapKnob *channelsKnob;
	PolyOutputChannelsWidget(math::Vec pos,ComputerscarePolyModule *mod,int paramId) {
		//Vec(8, 26) +7,+3
		//		//addParam(createParam<TinyChannelsSnapKnob>(Vec(8, 26), module, ComputerscareKnolyPobs::POLY_CHANNELS));
		module = mod;


		channelsKnob = createParam<TinyChannelsSnapKnob>(pos.plus(Vec(7,3)),module,paramId);
		channelsKnob->module=module;
		channelsKnob->paramId=paramId;


		channelCountDisplay = new PolyChannelsDisplay(pos);
		
		channelCountDisplay->module = module;

		addChild(channelsKnob);
		addChild(channelCountDisplay);
	}
};