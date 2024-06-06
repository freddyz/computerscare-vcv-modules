#include "Computerscare.hpp"
#include "dtpulse.hpp"

struct ComputerscareTolyPoolsV2;

/*
Input:

first rotate
knob, CV

numChannels select (auto)
knob,cv


input:
0123456789abcdef

want:
3456

rotate 4,clip 4

*/


struct ComputerscareTolyPoolsV2 : Module {
	int counter = 83910;
	int numOutputChannelsControlValue = 0;
	int numOutputChannels = 1;
	int rotation = 0;

	int knobRotation=0;
	int numChannelsKnob = 0;

	int numInputChannels = 1;

	int rotationModeEnum=0;
	int rotationBase = 16;

	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		ROTATE_KNOB,
		NUM_CHANNELS_KNOB,
		AUTO_CHANNELS_SWITCH,
		NUM_PARAMS

	};
	enum InputIds {
		POLY_INPUT,
		ROTATE_CV,
		NUM_CHANNELS_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_CHANNELS_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareTolyPoolsV2()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(ROTATE_KNOB, -16.f, 16.f, 0.f, "Rotation Offset", " channels");

		configParam<AutoParamQuantity>(NUM_CHANNELS_KNOB, 0.f, 16.f, 0.f, "Number of Output Channels Offset");

		configInput(POLY_INPUT, "Main");
		configInput(ROTATE_CV, "Rotation CV");
		configInput(NUM_CHANNELS_CV, "Number of Channels CV");

		configOutput(POLY_OUTPUT, "Main");
		configOutput(NUM_CHANNELS_OUTPUT, "Number of Input Channels");
	}
	void process(const ProcessArgs &args) override {
		counter++;
		
		int cvRotation = 0;
		int cvOutputChannels = 0;

		int finalPositiveRotation = 0;

		bool inputIsConnected = inputs[POLY_INPUT].isConnected();

		if (counter > 982) {
			counter = 0;
			numChannelsKnob = params[NUM_CHANNELS_KNOB].getValue();
			knobRotation = (int) round(params[ROTATE_KNOB].getValue());
		}
		
		if(inputIsConnected) {
			numInputChannels = inputs[POLY_INPUT].getChannels();
		} else {
			numInputChannels = 0;
		}

		if (inputs[NUM_CHANNELS_CV].isConnected()) {
			cvOutputChannels = (int) round(inputs[NUM_CHANNELS_CV].getVoltage(0)*1.6f);
		}
		if (inputs[ROTATE_CV].isConnected()) {
			cvRotation = (int) round(inputs[ROTATE_CV].getVoltage(0) * 1.6f);
		}

		rotation = knobRotation+cvRotation;

		numOutputChannelsControlValue = math::clamp(cvOutputChannels+numChannelsKnob,0,16);


		if(numOutputChannelsControlValue == 0) {
			numOutputChannels = inputIsConnected ? numInputChannels : 1;
		} else {
			numOutputChannels = numOutputChannelsControlValue;
		}

		outputs[POLY_OUTPUT].setChannels(numOutputChannels);
		outputs[NUM_CHANNELS_OUTPUT].setVoltage(mapChannelCountToVoltage(numInputChannels));



		if(rotationModeEnum == 0) {
			rotationBase = inputIsConnected ? numInputChannels : 16; // so when unconnected, the rotation knob illustrates the normal range
		} else if(rotationModeEnum == 1) {
			rotationBase = std::max(numOutputChannels,numInputChannels);
		} else if(rotationModeEnum == 2) {
			rotationBase = 16;
		}

		if(rotation > 0) {
			finalPositiveRotation = rotation % rotationBase;
		} else if(rotation < 0) {
			/*
				 eg: rotationBase=16, rotation = -21
				 positiveRotation = 16-(21 % 16) = 16 - (5) = +11
			*/
			finalPositiveRotation = rotationBase - ((-rotation) % rotationBase);
		}


		if(inputs[POLY_INPUT].isConnected() && outputs[POLY_OUTPUT].isConnected()) {
			for (int i = 0; i < numOutputChannels; i++) {
				outputs[POLY_OUTPUT].setVoltage(inputs[POLY_INPUT].getVoltage((i + finalPositiveRotation) % rotationBase), i);
			}
		} else {
			for (int i = 0; i < numOutputChannels; i++) {
				outputs[POLY_OUTPUT].setVoltage(0.f, i);
			}
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "rotationModeEnum", json_integer(rotationModeEnum));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *rotationModeJ = json_object_get(rootJ, "rotationModeEnum");

		if (rotationModeJ) {
			rotationModeEnum = json_integer_value(rotationModeJ);
		}
	}

};
struct PoolsSmallDisplayV2 : SmallLetterDisplay
{
	ComputerscareTolyPoolsV2 *module;
	int ch;
	int type = 0;
	PoolsSmallDisplayV2(int someType)
	{
		type = someType;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		//this->setNumDivisionsString();
		if (module)
		{

			if (type == 0) {
				if(module->numOutputChannelsControlValue == 0) {
					value = "A"; //Automatic - output channels match input channels
				} else {
					value = std::to_string(module->numOutputChannelsControlValue);
				}
				
			}
			else if (type == 1) {

				//keep the displayed knob value between -15 and +15
				int rotationDisplay = 0;
				if(module->rotation > 0) {
					rotationDisplay = module->rotation % module->rotationBase;
				} else if(module->rotation < 0) {
					rotationDisplay = -1*( (-1* module->rotation) % module->rotationBase);
				}

				value = std::to_string(rotationDisplay);
			}
			else if (type == 2) {
				value = std::to_string(module->numInputChannels);
			}

		}
		else {
			value = std::to_string((random::u32() % 16) + 1);
		}
		SmallLetterDisplay::draw(args);
	}

};

struct ComputerscareTolyPoolsWidgetV2 : ModuleWidget {
	ComputerscareTolyPoolsWidgetV2(ComputerscareTolyPoolsV2 *module) {

		setModule(module);
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareTolyPoolsPanel.svg")));

			addChild(panel);

		}


		addInput(createInput<InPort>(Vec(1	, 50), module, ComputerscareTolyPoolsV2::POLY_INPUT));
		poolsSmallDisplay = new PoolsSmallDisplayV2(2);
		poolsSmallDisplay->box.size = Vec(14, 20);
		poolsSmallDisplay->box.pos = Vec(-3 , 80);
		poolsSmallDisplay->fontSize = 22;
		poolsSmallDisplay->textAlign = 18;
		poolsSmallDisplay->breakRowWidth = 20;
		poolsSmallDisplay->module = module;
		addChild(poolsSmallDisplay);


		addLabeledKnob("Num Output Channels", 10, 156, module, ComputerscareTolyPoolsV2::NUM_CHANNELS_KNOB, -14, -24, 0);
		addInput(createInput<InPort>(Vec(10, 186), module, ComputerscareTolyPoolsV2::NUM_CHANNELS_CV));

		addLabeledKnob("Rotation", 10, 256, module, ComputerscareTolyPoolsV2::ROTATE_KNOB, -13, -5, 1);
		addInput(createInput<InPort>(Vec(10, 286), module, ComputerscareTolyPoolsV2::ROTATE_CV));


		addOutput(createOutput<OutPort>(Vec(28, 30), module, ComputerscareTolyPoolsV2::POLY_OUTPUT));

		addOutput(createOutput<PointingUpPentagonPort>(Vec(31, 76), module, ComputerscareTolyPoolsV2::NUM_CHANNELS_OUTPUT));
	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareTolyPoolsV2 *module, int index, float labelDx, float labelDy, int type) {

		poolsSmallDisplay = new PoolsSmallDisplayV2(type);
		poolsSmallDisplay->box.size = Vec(30, 20);
		poolsSmallDisplay->box.pos = Vec(x - 7.5 , y + 1.f);
		poolsSmallDisplay->fontSize = 22;
		poolsSmallDisplay->textAlign = 18;
		poolsSmallDisplay->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
		poolsSmallDisplay->breakRowWidth = 30;
		poolsSmallDisplay->module = module;


		outputChannelLabel = new SmallLetterDisplay();
		outputChannelLabel->box.size = Vec(5, 5);
		outputChannelLabel->box.pos = Vec(x + labelDx, y - 12 + labelDy);
		outputChannelLabel->fontSize = 15;
		outputChannelLabel->textAlign = 1;
		outputChannelLabel->breakRowWidth = 55;

		outputChannelLabel->value = label;

		addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, index));
		addChild(poolsSmallDisplay);

	}

	void appendContextMenu(Menu *menu) override;


	PoolsSmallDisplayV2* poolsSmallDisplay;
	SmallLetterDisplay* outputChannelLabel;
};
struct PoolsModeItem : MenuItem {
	ComputerscareTolyPoolsV2 *pools;
	int modeEnum;
	void onAction(const event::Action &e) override {
		pools->rotationModeEnum = modeEnum;
	}
	void step() override {
		rightText = CHECKMARK(pools->rotationModeEnum == modeEnum);
		MenuItem::step();
	}
};

	void ComputerscareTolyPoolsWidgetV2::appendContextMenu(Menu* menu) {
		ComputerscareTolyPoolsV2* pools = dynamic_cast<ComputerscareTolyPoolsV2*>(this->module);
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Rotation Mode"));
		menu->addChild(construct<PoolsModeItem>(&MenuItem::text, "Repeat Input Channels", &PoolsModeItem::pools, pools, &PoolsModeItem::modeEnum, 0));
		menu->addChild(construct<PoolsModeItem>(&MenuItem::text, "Rotate Through Maximum of Output, Input Channels", &PoolsModeItem::pools, pools, &PoolsModeItem::modeEnum, 1));
		menu->addChild(construct<PoolsModeItem>(&MenuItem::text, "Rotate Through 16 Channels (Legacy)", &PoolsModeItem::pools, pools, &PoolsModeItem::modeEnum, 2));
	
	}


Model *modelComputerscareTolyPoolsV2 = createModel<ComputerscareTolyPoolsV2, ComputerscareTolyPoolsWidgetV2>("computerscare-toly-pools-v2");
