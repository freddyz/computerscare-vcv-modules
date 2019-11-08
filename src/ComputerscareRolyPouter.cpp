#include "Computerscare.hpp"
#include "dtpulse.hpp"

struct ComputerscareRolyPouter;

const int numKnobs = 16;

struct ComputerscareRolyPouter : Module {
	int counter = 0;
	int routing[numKnobs];
	int numOutputChannels = 16;
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		KNOB,
		NUM_PARAMS = KNOB + numKnobs
	};
	enum InputIds {
		POLY_INPUT,
		ROUTING_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareRolyPouter()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numKnobs; i++) {
			configParam(KNOB + i, 1.f, 16.f, (i + 1), "output ch" + std::to_string(i + 1) + " = input ch");
			routing[i] = i;
		}

	}
	void setAll(int setVal) {
		for (int i = 0; i < 16; i++) {
			params[KNOB + i].setValue(setVal);
		}
	}
	void process(const ProcessArgs &args) override {
		counter++;
		int inputChannels = inputs[POLY_INPUT].getChannels();
		int knobSetting;
		
		outputs[POLY_OUTPUT].setChannels(numOutputChannels);

		//if()
		if (inputs[ROUTING_CV].isConnected())  {
			for (int i = 0; i < numOutputChannels; i++) {

				knobSetting = std::round(inputs[ROUTING_CV].getVoltage(i)*1.5)+1;

				routing[i]=knobSetting;
				if (knobSetting > inputChannels) {
					outputs[POLY_OUTPUT].setVoltage(0, i);
				}
				else {
					outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(knobSetting), i);
				}
			}
		} else {
			if (counter > 1000) {
			//printf("%f \n",random::uniform());
			counter = 0;
			for (int i = 0; i < numKnobs; i++) {
				routing[i] = (int)params[KNOB + i].getValue();
			}

		}
			for (int i = 0; i < numOutputChannels; i++) {

				knobSetting = params[KNOB + i].getValue();



				if (knobSetting > inputChannels) {
					outputs[POLY_OUTPUT].setVoltage(0, i);
				}
				else {
					outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage(knobSetting - 1), i);
				}
			}
		}
	}

};
struct PouterSmallDisplay : SmallLetterDisplay
{
	ComputerscareRolyPouter *module;
	int ch;
	PouterSmallDisplay(int outputChannelNumber)
	{

		ch = outputChannelNumber;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		//this->setNumDivisionsString();
		if (module)
		{


			std::string str = std::to_string(module->routing[ch]);
			value = str;



		}
		SmallLetterDisplay::draw(args);
	}

};

struct ComputerscareRolyPouterWidget : ModuleWidget {
	ComputerscareRolyPouterWidget(ComputerscareRolyPouter *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareRolyPouterPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareRolyPouterPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}
		addInput(createInput<PointingUpPentagonPort>(Vec(22, 53), module, ComputerscareRolyPouter::ROUTING_CV));

		float xx;
		float yy;
		for (int i = 0; i < numKnobs; i++) {
			xx = 1.4f + 24.3 * (i - i % 8) / 8;
			yy = 66 + 36.5 * (i % 8) + 14.3 * (i - i % 8) / 8;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i, (i - i % 8) * 1.3 - 5, i < 8 ? 4 : 0);
		}


		addInput(createInput<InPort>(Vec(1, 34), module, ComputerscareRolyPouter::POLY_INPUT));
		

		addOutput(createOutput<PointingUpPentagonPort>(Vec(32, 24), module, ComputerscareRolyPouter::POLY_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareRolyPouter *module, int index, float labelDx, float labelDy) {

		pouterSmallDisplay = new PouterSmallDisplay(index);
		pouterSmallDisplay->box.size = Vec(20, 20);
		pouterSmallDisplay->box.pos = Vec(x - 2.5 , y + 1.f);
		pouterSmallDisplay->fontSize = 26;
		pouterSmallDisplay->textAlign = 18;
		pouterSmallDisplay->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
		pouterSmallDisplay->breakRowWidth = 20;
		pouterSmallDisplay->module = module;


		outputChannelLabel = new SmallLetterDisplay();
		outputChannelLabel->box.size = Vec(5, 5);
		outputChannelLabel->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		outputChannelLabel->fontSize = 14;
		outputChannelLabel->textAlign = index < 8 ? 1 : 4;
		outputChannelLabel->breakRowWidth = 15;

		outputChannelLabel->value = std::to_string(index + 1);

		addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, ComputerscareRolyPouter::KNOB + index));
		addChild(pouterSmallDisplay);
		addChild(outputChannelLabel);

	}
	PouterSmallDisplay* pouterSmallDisplay;
	SmallLetterDisplay* outputChannelLabel;

	void addMenuItems(ComputerscareRolyPouter *pouter, Menu *menu);
	void appendContextMenu(Menu *menu) override;
};
struct ssmi : MenuItem
{
	ComputerscareRolyPouter *pouter;
	ComputerscareRolyPouterWidget *pouterWidget;
	int mySetVal = 1;
	ssmi(int setVal)
	{
		mySetVal = setVal;
		//scale = scaleInput;
	}

	void onAction(const event::Action &e) override
	{
		pouter->setAll(mySetVal);

		// peas->setQuant();
	}
};
void ComputerscareRolyPouterWidget::addMenuItems(ComputerscareRolyPouter *pouter, Menu *menu)
{
	for (int i = 1; i < 17; i++) {
		ssmi *menuItem = new ssmi(i);
		menuItem->text = "Set all to ch. " + std::to_string(i);
		menuItem->pouter = pouter;
		menuItem->pouterWidget = this;
		menu->addChild(menuItem);
	}

}
void ComputerscareRolyPouterWidget::appendContextMenu(Menu *menu)
{
	ComputerscareRolyPouter *pouter = dynamic_cast<ComputerscareRolyPouter *>(this->module);

	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);


	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Presets";
	menu->addChild(modeLabel);

	addMenuItems(pouter, menu);
	/*scaleItemAdd(peas, menu, "212212", "Natural Minor");
	scaleItemAdd(peas, menu, "2232", "Major Pentatonic");
	scaleItemAdd(peas, menu, "3223", "Minor Pentatonic");
	scaleItemAdd(peas, menu, "32113", "Blues");
	scaleItemAdd(peas, menu, "11111111111", "Chromatic");
	scaleItemAdd(peas, menu, "212213", "Harmonic Minor");
	scaleItemAdd(peas, menu, "22222", "Whole-Tone");
	scaleItemAdd(peas, menu, "2121212", "Whole-Half Diminished");

	scaleItemAdd(peas, menu, "43", "Major Triad");
	scaleItemAdd(peas, menu, "34", "Minor Triad");
	scaleItemAdd(peas, menu, "33", "Diminished Triad");
	scaleItemAdd(peas, menu, "434", "Major 7 Tetrachord");
	scaleItemAdd(peas, menu, "433", "Dominant 7 Tetrachord");
	scaleItemAdd(peas, menu, "343", "Minor 7 Tetrachord");
	scaleItemAdd(peas, menu, "334", "Minor 7 b5 Tetrachord");*/
}


Model *modelComputerscareRolyPouter = createModel<ComputerscareRolyPouter, ComputerscareRolyPouterWidget>("computerscare-roly-pouter");
