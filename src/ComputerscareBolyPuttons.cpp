#include "Computerscare.hpp"

struct ComputerscareBolyPuttons;

const int numToggles = 16;

struct ComputerscareBolyPuttons : Module {
	int counter = 0;
	int outputRangeEnum = 0;
	bool momentary = false;
	bool radioMode=false;
	float outputRanges[6][2];
	float previousToggle[16] = {0.f};
	rack::dsp::SchmittTrigger momentaryTriggers[16];
	rack::dsp::PulseGenerator pulseGen[16];

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
			configParam(TOGGLE + i, 0.f, 1.f, 0.f, "Channel " + std::to_string(i + 1));
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
void switchOffAllButtonsButOne(int index) {
    for (int i = 0; i < numToggles; i++) {
      if (i != index) {
        params[TOGGLE + i].setValue(0.f);
      }
    }
  }
void checkForParamChanges() {
	int changeIndex = -1;
	float val;
	for(int i = 0; i < numToggles; i++) {
		val=params[TOGGLE + i].getValue();
		if(val == 1.f && previousToggle[i] != val) {
			changeIndex = i;
		}
		previousToggle[i] = val;
	}
	if(changeIndex > -1) {
		switchOffAllButtonsButOne(changeIndex);
	}
}
void onRandomize() override {
    if(radioMode) {
    	int rIndex = floor(random::uniform() * 16);
    	switchOffAllButtonsButOne(rIndex);
    	params[TOGGLE+rIndex].setValue(1.f);
    }
    else {
    	for(int i = 0; i < numToggles; i++) {
    		params[TOGGLE+i].setValue(random::uniform() < 0.5 ? 0.f : 1.f);
    	}
    }
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
		if (momentary) {
			for (int i = 0; i < numToggles; i++) {
				if (momentaryTriggers[i].process(params[TOGGLE + i].getValue())) {
					pulseGen[i].trigger();
					if (inputs[A_INPUT].isConnected()) {
						min = inputs[A_INPUT].getVoltage(i % numAChannels);
					}

					if (inputs[B_INPUT].isConnected()) {
						max = inputs[B_INPUT].getVoltage(i % numBChannels);
					}

					float spread = max - min;
					outputs[POLY_OUTPUT].setVoltage(pulseGen[i].process(APP->engine->getSampleTime())*spread + min, i);
				}

			}

		}
		else {
			if(radioMode) {
				checkForParamChanges();	
			}
			for (int i = 0; i < numToggles; i++) {
				if (inputs[A_INPUT].isConnected()) {
					min = inputs[A_INPUT].getVoltage(i % numAChannels);
				}

				if (inputs[B_INPUT].isConnected()) {
					max = inputs[B_INPUT].getVoltage(i % numBChannels);
				}

				float spread = max - min;
				outputs[POLY_OUTPUT].setVoltage(params[TOGGLE + i].getValue()*spread + min, i);
			}
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
			addChild(panel);

		}
		float xx;
		float yy;
		for (int i = 0; i < numToggles; i++) {
			xx = 5.2f + 27.3 * (i - i % 8) / 8;
			yy = 92 + 33.5 * (i % 8) + 14.3 * (i - i % 8) / 8;
			addLabeledButton(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 1.2 - 2, 2);
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
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		addChild(smallLetterDisplay);

		addParam(createParam<SmallIsoButton>(Vec(x, y), module, ComputerscareBolyPuttons::TOGGLE + index));
	}
	json_t *toJson() override
	{
		json_t *rootJ = ModuleWidget::toJson();
		json_object_set_new(rootJ, "outputRange", json_integer(bolyPuttons->outputRangeEnum));
		json_object_set_new(rootJ, "radioMode", json_boolean(bolyPuttons->radioMode));
		json_object_set_new(rootJ, "momentaryMode", json_boolean(bolyPuttons->momentary));
		return rootJ;
	}
	void fromJson(json_t *rootJ) override
	{
		ModuleWidget::fromJson(rootJ);
		// button states

		json_t *outputRangeEnumJ = json_object_get(rootJ, "outputRange");
		if (outputRangeEnumJ) { bolyPuttons->outputRangeEnum = json_integer_value(outputRangeEnumJ); }
			json_t *radioModeJ = json_object_get(rootJ, "radioMode");
		if (radioModeJ) { bolyPuttons->radioMode = json_is_true(radioModeJ); }
			json_t *momentaryModeJ = json_object_get(rootJ, "momentaryMode");
		if (momentaryModeJ) { bolyPuttons->momentary = json_is_true(momentaryModeJ); }

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
struct RadioModeMenuItem: MenuItem {
  ComputerscareBolyPuttons *bolyPuttons;
  RadioModeMenuItem() {

  }
  void onAction(const event::Action &e) override {
	bolyPuttons->radioMode = !bolyPuttons->radioMode;
  }
  void step() override {
    rightText = bolyPuttons->radioMode? "✔" : "";
    MenuItem::step();
  }
};

void ComputerscareBolyPuttonsWidget::appendContextMenu(Menu *menu)
{
	ComputerscareBolyPuttons *bolyPuttons = dynamic_cast<ComputerscareBolyPuttons *>(this->module);

	menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));
	menu->addChild(construct<MenuLabel>(&MenuLabel::text, "How The Buttons Work"));
  	RadioModeMenuItem *radioMode = new RadioModeMenuItem();
  	radioMode->text = "Exclusive Mode (behaves like radio buttons)";
  	radioMode->bolyPuttons= bolyPuttons;
 	menu->addChild(radioMode);


	menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

	menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Off / On Values (A ... B)"));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "  0v ... +10v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 0));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, " -5v ...  +5v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 1));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "  0v ...  +5v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 2));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "  0v ...  +1v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 3));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, " -1v ...  +1v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 4));
	menu->addChild(construct<OutputRangeItem>(&MenuItem::text, "-10v ... +10v", &OutputRangeItem::bolyPuttons, bolyPuttons, &OutputRangeItem::outputRangeEnum, 5));




}
Model *modelComputerscareBolyPuttons = createModel<ComputerscareBolyPuttons, ComputerscareBolyPuttonsWidget>("computerscare-bolyputtons");
