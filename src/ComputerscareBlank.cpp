#include "Computerscare.hpp"
#include <osdialog.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

struct ComputerscareBlank;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareBlank : Module {
	int counter = 0;
	std::string path;
	float width=8 * 15;
	float height= 380;
	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		KNOB,
		TOGGLES = KNOB + numKnobs,
		NUM_PARAMS = TOGGLES + numToggles

	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS = POLY_OUTPUT + numOutputs
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareBlank()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numKnobs; i++) {
			configParam(KNOB + i, 0.0f, 10.0f, 0.0f);
			configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1) + " Voltage", " Volts");
		}

	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 5012) {
			//printf("%f \n",random::uniform());
			counter = 0;
			//rect4032
			//south facing high wall
		}
		outputs[POLY_OUTPUT].setChannels(16);
		for (int i = 0; i < numKnobs; i++) {
			outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue(), i);
		}
	}
	void loadScriptDialog() {
		std::string dir = asset::plugin(pluginInstance, "examples");
		char* pathC = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
		if (!pathC) {
			return;
		}
		std::string path = pathC;
		std::free(pathC);

		setPath(path);
	}

	void setPath(std::string path) {
		if (path == "")
			return;

		this->path = path;
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "path", json_string(path.c_str()));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *pathJ = json_object_get(rootJ, "path");
		if (pathJ) {
			path = json_string_value(pathJ);
			setPath(path);
		}
	}

};
struct LoadScriptItem : MenuItem {
	ComputerscareBlank* module;
	void onAction(const event::Action& e) override {
		module->loadScriptDialog();
	}
};



struct PNGDisplay : TransparentWidget {
	ComputerscareBlank *module;
	const float width = 125.0f;
	const float height = 130.0f;
	std::string path = "";
	bool first = true;
	int img = 0;

	PNGDisplay() {
	}

	void draw(const DrawArgs &args) override {
		if (module/* && !module->loading*/) {
			if (path != module->path) {
				img = nvgCreateImage(args.vg, module->path.c_str(), 0);
				path = module->path;
			}

			nvgBeginPath(args.vg);
			//if (module->width>0 && module->height>0)
				nvgScale(args.vg, width/module->width, height/module->height);
		 	NVGpaint imgPaint = nvgImagePattern(args.vg, 0, 0, module->width,module->height, 0, img, 1.0f);
		 	nvgRect(args.vg, 0, 0, module->width, module->height);
		 	nvgFillPaint(args.vg, imgPaint);
		 	nvgFill(args.vg);
			nvgClosePath(args.vg);
		}
	}
};
struct ComputerscareBlankWidget : ModuleWidget {
	ComputerscareBlankWidget(ComputerscareBlank *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
		box.size = Vec(module->width,module->height);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}


		{
			PNGDisplay *display = new PNGDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(module->width, module->height);
			addChild(display);
		}

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareBlank *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 16;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		addParam(createParam<SmoothKnob>(Vec(x, y), module, ComputerscareBlank::KNOB + index));
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	void appendContextMenu(Menu* menu) override {
		ComputerscareBlank* module = dynamic_cast<ComputerscareBlank*>(this->module);

		menu->addChild(new MenuEntry);

		LoadScriptItem* loadScriptItem = createMenuItem<LoadScriptItem>("Load image (PNG)");
		loadScriptItem->module = module;
		menu->addChild(loadScriptItem);


	}
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareBlank = createModel<ComputerscareBlank, ComputerscareBlankWidget>("computerscare-blank");
