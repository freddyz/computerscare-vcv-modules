#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
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

		NUM_PARAMS

	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS 
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareBlank()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);


	}
	void process(const ProcessArgs &args) override {

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
		json_object_set_new(rootJ, "width", json_real(width));
		json_object_set_new(rootJ, "height", json_real(height));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *pathJ = json_object_get(rootJ, "path");
		if (pathJ) {
			path = json_string_value(pathJ);
			setPath(path);
		}
		json_t *widthJ = json_object_get(rootJ, "width");
	if (widthJ)
		width = json_number_value(widthJ);
		
	json_t *heightJ = json_object_get(rootJ, "height");
	if (heightJ)
		height = json_number_value(heightJ);
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
				//nvgScale(args.vg, width/module->width, height/module->height);
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
		this->blankModule=module;
		box.size = Vec(module->width, module->height);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
		box.size = Vec(module->width,module->height);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));

			this->panel=panel;
			//module->panelRef = panel;

			addChild(panel);

		}


		{
			PNGDisplay *pngDisplay = new PNGDisplay();
			pngDisplay->module = module;
			pngDisplay->box.pos = Vec(0, 0);
			pngDisplay->box.size = Vec(module->width, module->height);
			this->pngDisplay=pngDisplay;
			addChild(pngDisplay);
		}

	ComputerscareResizeHandle *rightHandle = new ComputerscareResizeHandle();
	rightHandle->right = true;
	this->rightHandle = rightHandle;
	addChild(rightHandle);

	}

	void appendContextMenu(Menu* menu) override {
		ComputerscareBlank* module = dynamic_cast<ComputerscareBlank*>(this->module);

		menu->addChild(new MenuEntry);

		LoadScriptItem* loadScriptItem = createMenuItem<LoadScriptItem>("Load image (PNG)");
		loadScriptItem->module = module;
		menu->addChild(loadScriptItem);


	}
	void step() override;
	ComputerscareBlank *blankModule;
	PNGDisplay *pngDisplay;
	ComputerscareSVGPanel *panel;
	TransparentWidget *display;
	ComputerscareResizeHandle *rightHandle;
	SmallLetterDisplay* smallLetterDisplay;
};
void ComputerscareBlankWidget::step() {
	blankModule->width=box.size.x;

	panel->box.size = box.size;
	//display->box.size = Vec(box.size.x, box.size.y);
	pngDisplay->box.size.x = box.size.x;
	rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
	ModuleWidget::step();
}

Model *modelComputerscareBlank = createModel<ComputerscareBlank, ComputerscareBlankWidget>("computerscare-blank");
