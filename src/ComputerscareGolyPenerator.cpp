#include "Computerscare.hpp"

#include "dtpulse.hpp"
#include "golyFunctions.hpp"

struct ComputerscareGolyPenerator;


/*
	knob1: number of channels output 1-16
	knob2: algorithm
	knob3: param 1
*/
const std::string GolyPeneratorAvailableAlgorithmsArr[5] = {"Linear", "Sigmoid", "Hump", "Sinusoid", "Pseudo-Random"};


//template <const std::string& options>
struct GolyAlgoParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int val = getValue();
		return GolyPeneratorAvailableAlgorithmsArr[val];
	}
};

struct ComputerscareGolyPenerator : ComputerscareMenuParamModule {
	int counter = 0;
	int numChannels = 16;
	ComputerscareSVGPanel* panelRef;
	Goly goly;
	float currentValues[16] = {0.f};
	std::vector<std::string> availableAlgorithms;

	enum ParamIds {
		ALGORITHM,
		IN_OFFSET,
		IN_SCALE,
		OUT_SCALE,
		OUT_OFFSET,
		POLY_CHANNELS,
		COLOR,
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

		configParam<GolyAlgoParamQuantity>(ALGORITHM , 0.f, 4.f, 0.f, "Algorithm");
		configParam(IN_OFFSET, -1.f, 1.f, 0.f, "Channel Center");

		configParam(IN_SCALE, -2.f, 2.f, 1.f, "Channel Spread");

		configParam(OUT_SCALE, -20.f, 20.f, 10.f, "Output Scale");
		configParam(OUT_OFFSET, -10.f, 10.f, 0.f, "Output Offset");
		configParam<AutoParamQuantity>(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels");
		configMenuParam(COLOR, 0.f, 9.f, 0.f, "Display Color", 2);

		getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
		getParamQuantity(POLY_CHANNELS)->resetEnabled = false;

		configOutput(POLY_OUTPUT, "Main");

		availableAlgorithms.push_back("Linear");
		availableAlgorithms.push_back("Sigmoid");
		availableAlgorithms.push_back("Hump");
		availableAlgorithms.push_back("Sinusoid");
		availableAlgorithms.push_back("Pseudo-Random");


		goly = Goly();

	}
	void updateCurrents() {
		std::vector<float> golyParams = {params[IN_OFFSET].getValue(), params[IN_SCALE].getValue(), params[OUT_SCALE].getValue(), params[OUT_OFFSET].getValue()};
		goly.invoke((int) std::floor(params[ALGORITHM].getValue()), golyParams, params[POLY_CHANNELS].getValue());
	}
	void setAlgorithm(int algoVal) {
		params[ALGORITHM].setValue(algoVal);
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
struct setAlgoItem : MenuItem
{
	ComputerscareGolyPenerator *penerator;
	int mySetVal;
	setAlgoItem(int setVal)
	{
		mySetVal = setVal;
	}

	void onAction(const event::Action &e) override
	{
		penerator->setAlgorithm(mySetVal);
	}
	void step() override {
		rightText = CHECKMARK(penerator->params[ComputerscareGolyPenerator::ALGORITHM].getValue() == mySetVal);
		MenuItem::step();
	}
};

struct AlgorithmChildMenu : MenuItem {
	ComputerscareGolyPenerator *penerator;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select an Algorithm... NOW"));

		for (unsigned int i = 0; i < penerator->availableAlgorithms.size(); i++) {
			setAlgoItem *menuItem = new setAlgoItem(i);
			//ParamSettingItem *menuItem = new ParamSettingItem(i,ComputerscareGolyPenerator::ALGORITHM);

			menuItem->text = penerator->availableAlgorithms[i];
			menuItem->penerator = penerator;
			menu->addChild(menuItem);
		}

		return menu;
	}

};
struct PeneratorDisplay : TransparentWidget {
	ComputerscareGolyPenerator *module;

	PeneratorDisplay() {

	}
	void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
		if (layer == 1) {
			float valsToDraw[16] = {1.f};
			int ch = 16;
			float colorArg;

			if (module) {
				ch = module->polyChannels;
				colorArg = module->params[ComputerscareGolyPenerator::COLOR].getValue();
				for (int i = 0; i < ch; i++) {
					valsToDraw[i] = module->goly.currentValues[i];
				}
			}
			else {
				for (int i = 0; i < ch; i++) {
					valsToDraw[i] = random::uniform() * 10;
				}
				colorArg = random::uniform() * 2;
			}
			DrawHelper draw = DrawHelper(args.vg);
			Points pts = Points();

			nvgTranslate(args.vg, box.size.x / 2, box.size.y / 2 + 5);
			pts.linear(ch, Vec(0, -box.size.y / 2), Vec(0, 150));
			std::vector<Vec> polyVals;
			std::vector<NVGcolor> colors;
			std::vector<Vec> thicknesses;

			for (int i = 0; i < 16; i++) {
				polyVals.push_back(Vec(valsToDraw[i] * 2, 0.f));
				colors.push_back(draw.sincolor(colorArg, {1, 1, 0}));

				thicknesses.push_back(Vec(160 / (1 + ch), 0));
			}
			draw.drawLines(pts.get(), polyVals, colors, thicknesses);
		}
	}
};
struct ComputerscareGolyPeneratorWidget : ModuleWidget {
	ComputerscareGolyPeneratorWidget(ComputerscareGolyPenerator *module) {

		setModule(module);
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

		addLabeledKnob<ScrambleSnapKnob>("Algo", 4, 324, module, ComputerscareGolyPenerator::ALGORITHM, 0, 0, true);
		addLabeledKnob<SmoothKnob>("center", 28, 80, module, ComputerscareGolyPenerator::IN_OFFSET, 0, 0);
		addLabeledKnob<SmallKnob>("spread", 5, 86, module, ComputerscareGolyPenerator::IN_SCALE, 0, 0);
		addLabeledKnob<SmallKnob>("scale", 33, 290, module, ComputerscareGolyPenerator::OUT_SCALE, 0, 0);
		addLabeledKnob<SmoothKnob>("offset", 2, 284, module, ComputerscareGolyPenerator::OUT_OFFSET, 0, 0);

		channelWidget = new PolyOutputChannelsWidget(Vec(28, 309), module, ComputerscareGolyPenerator::POLY_CHANNELS);
		addChild(channelWidget);

		addOutput(createOutput<InPort>(Vec(28, 329), module, ComputerscareGolyPenerator::POLY_OUTPUT));

	}
	void appendContextMenu(Menu* menu) override {
		ComputerscareGolyPenerator* penerator = dynamic_cast<ComputerscareGolyPenerator*>(this->module);

		MenuParam* colorParam = new MenuParam(penerator->paramQuantities[ComputerscareGolyPenerator::COLOR], 2);
		menu->addChild(colorParam);

		menu->addChild(new MenuSeparator);

		AlgorithmChildMenu *algoMenu = new AlgorithmChildMenu();
		algoMenu->text = "Algorithm";
		algoMenu->rightText = RIGHT_ARROW;
		algoMenu->penerator = penerator;
		menu->addChild(algoMenu);
	}

	template <typename BASE>
	void addLabeledKnob(std::string label, int x, int y, ComputerscareGolyPenerator *module, int paramIndex, float labelDx, float labelDy, bool snap = false) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 14;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		if (snap) {
			addParam(createParam<BASE>(Vec(x, y), module, paramIndex));
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
