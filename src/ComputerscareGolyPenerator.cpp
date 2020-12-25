#include "Computerscare.hpp"

#include "dtpulse.hpp"
#include "golyFunctions.hpp"

struct ComputerscareGolyPenerator;


/*
	knob1: number of channels output 1-16
	knob2: algorithm
	knob3: param 1
*/

struct ComputerscareGolyPenerator : ComputerscarePolyModule {
	int counter = 0;
	int numChannels = 16;
	ComputerscareSVGPanel* panelRef;
	Goly goly;
	float currentValues[16] = {0.f};
	enum ParamIds {
		ALGORITHM,
		IN_OFFSET,
		IN_SCALE,
		OUT_SCALE,
		OUT_OFFSET,
		POLY_CHANNELS,
		NUM_PARAMS
	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareGolyPenerator()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(ALGORITHM , 1.f, 4.f, 1.f, "Algorithm");
		configParam(IN_OFFSET, -1.f, 1.f, 0.f, "Channel Center");

		configParam(IN_SCALE, -2.f, 2.f, 1.f, "Channel Spread");

		configParam(OUT_SCALE, -20.f, 20.f, 10.f, "Output Scale");
		configParam(OUT_OFFSET, -10.f, 10.f, 0.f, "Output Offset");
		configParam<AutoParamQuantity>(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels");

		goly = Goly();

	}
	void updateCurrents() {
		std::vector<float> golyParams = {params[IN_OFFSET].getValue(), params[IN_SCALE].getValue(), params[OUT_SCALE].getValue(), params[OUT_OFFSET].getValue()};
		goly.invoke((int) std::floor(params[ALGORITHM].getValue()), golyParams, params[POLY_CHANNELS].getValue());
	}
	void checkPoly() override {
		int knobSetting = params[POLY_CHANNELS].getValue();
		polyChannels = knobSetting == 0 ? 16 : knobSetting;
		outputs[POLY_OUTPUT].setChannels(polyChannels);
	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();
		counter++;
		if (counter > 13) {
			counter = 0;
			updateCurrents();
			//numChannels=(int) (std::floor(params[POLY_CHANNELS].getValue()));
		}


		//outputs[POLY_OUTPUT].setChannels(numChannels);
		for (int i = 0; i < polyChannels; i++) {
			outputs[POLY_OUTPUT].setVoltage(goly.currentValues[i], i);
		}
	}

};
struct PeneratorDisplay : TransparentWidget {
	ComputerscareGolyPenerator *module;

	PeneratorDisplay() {

	}
	void draw(const DrawArgs &args) override {
		float valsToDraw[16] = {1.f};
		int ch = 16;
		if (module) {
			ch = module->polyChannels;
			for (int i = 0; i < ch; i++) {
				valsToDraw[i] = module->goly.currentValues[i];
			}
		}
		else {
			for (int i = 0; i < ch; i++) {
				valsToDraw[i] = random::uniform() * 10;
			}
		}
		DrawHelper draw = DrawHelper(args.vg);
		Points pts = Points();

		nvgTranslate(args.vg, box.size.x / 2, box.size.y/2+5);
		pts.linear(ch, Vec(0, -box.size.y/2), Vec(0, 150));
		std::vector<Vec> polyVals;
		std::vector<NVGcolor> colors;
		std::vector<Vec> thicknesses;

		for (int i = 0; i < 16; i++) {
			polyVals.push_back(Vec(valsToDraw[i] * 2,0.f));
			colors.push_back(draw.sincolor(0,{1,1,0}));

			thicknesses.push_back(Vec(160/(1+ch), 0));
		}
		draw.drawLines(pts.get(), polyVals, colors, thicknesses);
	}
};
struct ComputerscareGolyPeneratorWidget : ModuleWidget {
	ComputerscareGolyPeneratorWidget(ComputerscareGolyPenerator *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareGolyPeneratorPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareGolyPeneratorPanel.svg")));
			addChild(panel);
		}

		PeneratorDisplay *display = new PeneratorDisplay();
		display->module = module;
		display->box.pos = Vec(0, 120);
		display->box.size = Vec(box.size.x, 400);
		addChild(display);

		float xx;
		float yy;
		addLabeledKnob<SmoothKnob>("Algo", 5, 30, module, ComputerscareGolyPenerator::ALGORITHM, 0, 0, true);
		addLabeledKnob<SmoothKnob>("center", 28, 80, module, ComputerscareGolyPenerator::IN_OFFSET, 0, 0);
		addLabeledKnob<SmallKnob>("spread", 5, 86, module, ComputerscareGolyPenerator::IN_SCALE, 0, 0);
		addLabeledKnob<SmallKnob>("scale", 33, 290, module, ComputerscareGolyPenerator::OUT_SCALE, 0, 0);
		addLabeledKnob<SmoothKnob>("offset", 2, 284, module, ComputerscareGolyPenerator::OUT_OFFSET, 0, 0);

		//addLabeledKnob("ch out",5,90,module,ComputerscareGolyPenerator::POLY_CHANNELS,-2,0);

		channelWidget = new PolyOutputChannelsWidget(Vec(0, 312), module, ComputerscareGolyPenerator::POLY_CHANNELS);
		addChild(channelWidget);

		addOutput(createOutput<PointingUpPentagonPort>(Vec(28, 318), module, ComputerscareGolyPenerator::POLY_OUTPUT));

	}


	template <typename BASE>
	void addLabeledKnob(std::string label, int x, int y, ComputerscareGolyPenerator *module, int paramIndex, float labelDx, float labelDy, bool snap = false) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 14;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		if (snap) {
			addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, paramIndex));
		}
		else {
			addParam(createParam<BASE>(Vec(x, y), module, paramIndex));

		}
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		//addChild(smallLetterDisplay);

	}
	PolyOutputChannelsWidget* channelWidget;

	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareGolyPenerator = createModel<ComputerscareGolyPenerator, ComputerscareGolyPeneratorWidget>("computerscare-goly-penerator");
