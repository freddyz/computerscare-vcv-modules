#include "Computerscare.hpp"

struct ComputerscareBolyPuttons;

const int numToggles = 16;

struct ComputerscareBolyPuttons : Module {
	int counter = 0;
	int outputRangeEnum = 0;
	float outputRanges[4][2];

	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		TOGGLE,
		NUM_PARAMS = TOGGLE + numToggles

	};
	enum InputIds {
		CHANNEL_INPUT,
		A_INPUT,
		B_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareBolyPuttons()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numToggles; i++) {
			//configParam(KNOB + i, 0.0f, 10.0f, 0.0f);
			configParam(TOGGLE + i, 0.f, 1.f, 0.f, "Channel " + std::to_string(i + 1) + " Voltage", " Volts");
		}

		outputRanges[0][0] = 0.f;
		outputRanges[0][1] = 10.f;
		outputRanges[1][0] = -5.f;
		outputRanges[1][1] = 5.f;
		outputRanges[2][0] = 0.f;
		outputRanges[2][1] = 5.f;
		outputRanges[3][0] = 0.f;
		outputRanges[3][1] = 1.f;
		outputRanges[4][0] = -1.f;
		outputRanges[4][1] = 1.f;
		outputRanges[5][0] = -10.f;
		outputRanges[5][1] = 10.f;
	}
	void process(const ProcessArgs &args) override {
		float min = outputRanges[outputRangeEnum][0];
		float max = outputRanges[outputRangeEnum][1];
		int numAChannels = inputs[A_INPUT].getChannels();
		int numBChannels = inputs[B_INPUT].getChannels();
		counter++;
		if (counter > 5012) {
			//printf("%f \n",random::uniform());
			counter = 0;
			//rect4032
			//south facing high wall
		}
		outputs[POLY_OUTPUT].setChannels(16);

		//if (outputs[SCALED_OUTPUT + i].isConnected() || outputs[QUANTIZED_OUTPUT + i].isConnected()) {
           // numInputChannels = inputs[CHANNEL_INPUT + i].getChannels();
		
		for (int i = 0; i < numToggles; i++) {
			if(inputs[A_INPUT].isConnected()) {
			min = inputs[A_INPUT].getVoltage(i % numAChannels);
		}
		
		if(inputs[B_INPUT].isConnected()) {
			max = inputs[B_INPUT].getVoltage(i % numBChannels);
		}
		
		float spread = max - min;
			outputs[POLY_OUTPUT].setVoltage(params[TOGGLE + i].getValue()*spread + min, i);
		}
	}

};

struct ComputerscareBolyPuttonsWidget : ModuleWidget {
	ComputerscareBolyPuttonsWidget(ComputerscareBolyPuttons *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareBolyPuttonsPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}
		float xx;
		float yy;
		for (int i = 0; i < numToggles; i++) {
			xx = 7.4f + 27.3 * (i % 2);
			yy = 94 + 16.5 * (i - i % 2) + 11.3 * (i % 2);
			addLabeledButton(std::to_string(i + 1), xx, yy, module, i, (i % 2) * (3 + 10 * (i < 9)) - 2, 0);
		}


		addInput(createInput<InPort>(Vec(9, 58), module, ComputerscareBolyPuttons::A_INPUT));
		addInput(createInput<PointingUpPentagonPort>(Vec(33, 55), module, ComputerscareBolyPuttons::B_INPUT));
	
		addOutput(createOutput<PointingUpPentagonPort>(Vec(1, 24), module, ComputerscareBolyPuttons::POLY_OUTPUT));
		bolyPuttons = module;
	}
	void addLabeledButton(std::string label, int x, int y, ComputerscareBolyPuttons *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 16;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		addParam(createParam<SmallIsoButton>(Vec(x, y), module, ComputerscareBolyPuttons::TOGGLE + index));

		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	json_t *toJson() override
	{
		json_t *rootJ = ModuleWidget::toJson();
		json_object_set_new(rootJ, "outputRange", json_integer(bolyPuttons->outputRangeEnum));
		return rootJ;
	}
	void fromJson(json_t *rootJ) override
	{
		ModuleWidget::fromJson(rootJ);
		// button states

		json_t *outputRangeEnumJ = json_object_get(rootJ, "outputRange");
		if (outputRangeEnumJ) { bolyPuttons->outputRangeEnum = json_integer_value(outputRangeEnumJ); }

	}
	void appendContextMenu(Menu *menu) override;
	ComputerscareBolyPuttons *bolyPuttons;
	SmallLetterDisplay* smallLetterDisplay;
};
struct OutputRangeItem : MenuItem {
	ComputerscareBolyPuttons *bolyPuttons;
	int outputRangeEnum;
	void onAction(const event::Action &e) override {
		bolyPuttons->outputRangeEnum = outputRangeEnum;
	}
	void step() override {
		rightText = CHECKMARK(bolyPuttons->outputRangeEnum == outputRangeEnum);
		MenuItem::step();
	}
};
void ComputerscareBolyPuttonsWidget::appendContextMenu(Menu *menu)
{
	ComputerscareBolyPuttons *bolyPuttons = dynamic_cast<ComputerscareBolyPuttons *>(this->module);

	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);


	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Output Range";
	menu->addChild(modeLabel);

	// randomization output bounds
	menu->addChild(construct<MenuLabel>());
	menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Output Range"));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "  0v ... +10v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 0));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, " -5v ...  +5v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 1));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "  0v ...  +5v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 2));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "  0v ...  +1v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 3));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, " -1v ...  +1v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 4));

	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "-10v ... +10v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 5));

}
Model *modelComputerscareBolyPuttons = createModel<ComputerscareBolyPuttons, ComputerscareBolyPuttonsWidget>("computerscare-bolyputtons");
