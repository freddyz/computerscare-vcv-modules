#include "Computerscare.hpp"
#include <patch.hpp>

struct ComputerscarePatchVersioning;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

const std::string numbers = "0123456789";
const std::string letters = "abcdefghijklmnopqrstuvwxyz";



struct ComputerscarePatchVersioning : Module {
	int counter = 0;
	bool parentLetter = true;

	ComputerscareSVGPanel* panelRef;
	rack::dsp::SchmittTrigger saveTrigger;

	std::vector<std::string> patchVersionFilenames;

	enum ParamIds {
		SAVE_BUTTON,
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


	ComputerscarePatchVersioning()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(SAVE_BUTTON, "Manual Trigger");

	}
	void process(const ProcessArgs &args) override {
		bool saveClicked = saveTrigger.process(params[SAVE_BUTTON].getValue());
		if (saveClicked) {
			savePatch();
		}
	}
	std::string generateNewPatchName() {
		return "patch-" + std::to_string(counter);
	}
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "counter", json_integer(counter));

		json_t *filenamesJ = json_array();
		for (int i = 0; i < (int) patchVersionFilenames.size(); i++) {
			json_t *filenameJ = json_string(patchVersionFilenames[i].c_str());
			json_array_append_new(filenamesJ, filenameJ);
		}
		json_object_set_new(rootJ, "patchVersionFilenames", filenamesJ);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		float val;

		json_t *counterJ = json_object_get(rootJ, "counter");
		if (counterJ) { counter = json_integer_value(counterJ); }

		json_t *filenamesJ = json_object_get(rootJ, "patchVersionFilenames");
		if (filenamesJ) {
			for (int i = 0; i < json_array_size(filenamesJ); i++) {
				json_t *filenameJ = json_array_get(filenamesJ, i);
				patchVersionFilenames.push_back(json_string_value(filenameJ));
			}
		}
	}
	void savePatch() {
		DEBUG("not doing anything");
	}

	void selectedPatch(int index) {
		DEBUG("selected patch %i", index);
	}

	void onAdd(const AddEvent& e) override {
		DEBUG("onAdd callud");
		// Read file...

		//savePatchInStorage("lastPatch.vcv");
	}

	void onSave(const SaveEvent& e) override {
		DEBUG("onSave culled");
		counter++;

		std::string filename = generateNewPatchName();

		patchVersionFilenames.push_back(filename);
		savePatchInStorage(filename);
	}
	void savePatchInStorage(std::string baseFileName) {
		json_t* patchJson =  APP->patch->toJson();

		std::string versionsFolder = system::join(createPatchStorageDirectory(), "versions");
		system::createDirectory(versionsFolder);


		std::string screenshotsFolder = system::join(createPatchStorageDirectory(), "screenshots");
		system::createDirectory(screenshotsFolder);


		//screenshots take up a lot of space but theyre cool lol
		//APP->window->screenshot(system::join(screenshotsFolder, baseFileName + ".png"));

		std::string filename = baseFileName + ".vcv";

		std::string patchPath = system::join(versionsFolder, filename);

		FILE* file = std::fopen(patchPath.c_str(), "w");
		if (!file) {
			// Fail silently
			return;
		}
		json_dumpf(patchJson, file, JSON_INDENT(2));
		std::fclose(file);
	}

};
struct KeyContainer : Widget {
	ComputerscarePatchVersioning* module = NULL;

	void onHoverKey(const event::HoverKey& e) override {
		if (module && !module->isBypassed()) {

			if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
				/*module->keys[idx].mods & GLFW_MOD_ALT ? 0.7f : 0.f);
				module->lights[StrokeModule<PORTS>::LIGHT_CTRL + idx].setBrightness(module->keys[idx].mods & GLFW_MOD_CONTROL ? 0.7f : 0.f);
				module->lights[StrokeModule<PORTS>::LIGHT_SHIFT + idx].setBrightness(module->keys[idx].mods & GLFW_MOD_SHIFT*/


				if (e.key == GLFW_KEY_S) {
					if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_ALT) {
						module->savePatch();
						e.consume(this);
					}

				}
			}

			/*if (e.action == GLFW_RELEASE) {
				for (int i = 0; i < PORTS; i++) {
					if (e.key == module->keys[i].key) {
						module->keyDisable(i);
						e.consume(this);
					}
				}
			}*/
		}
		Widget::onHoverKey(e);
	}

};
struct ssmi : MenuItem
{

	ComputerscarePatchVersioning *module;
	int mySetVal = 1;
	ssmi(int setVal)
	{
		mySetVal = setVal;
	}

	void onAction(const event::Action &e) override
	{
		//module->setAll(mySetVal);
		DEBUG("PORK HORSE %i", mySetVal);
	}
};
struct SetAllItem : MenuItem {
	ComputerscarePatchVersioning *module;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 1; i < 17; i++) {
			ssmi *menuItem = new ssmi(i);
			menuItem->text = "Set all to ch. " + std::to_string(i);
			menuItem->module = module;
			menu->addChild(menuItem);
		}
		return menu;
	}
};

struct ComputerscarePatchVersioningWidget : ModuleWidget {
	KeyContainer* keyContainer = NULL;
	ComputerscarePatchVersioningWidget(ComputerscarePatchVersioning *module) {

		setModule(module);

		float outputY = 334;
		box.size = Vec(15 * 10, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareIsoPanel.svg")));

			addChild(panel);

		}

		if (module) {
			keyContainer = new KeyContainer;
			keyContainer->module = module;
			// This is where the magic happens: add a new widget on top-level to Rack
			APP->scene->rack->addChild(keyContainer);
		}

		addParam(createParam<MomentaryIsoButton>(Vec(50, 100), module, ComputerscarePatchVersioning::SAVE_BUTTON));
	}
	void appendContextMenu(Menu *menu)
	{
		ComputerscarePatchVersioning *module = dynamic_cast<ComputerscarePatchVersioning *>(this->module);

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		menu->addChild(createSubmenuItem("Patch Versions", "",
		[ = ](Menu * menu) {
			menu->addChild(createMenuLabel("Load Patch:"));


			for (int i = 0; i < (int)module->patchVersionFilenames.size(); i++) {
				menu->addChild(createMenuItem(module->patchVersionFilenames[i], "",
				[ = ]() {module->selectedPatch(i);}
				                             ));
			}


		}
		                                ));
	}
	~ComputerscarePatchVersioningWidget() {
		if (keyContainer) {
			APP->scene->rack->removeChild(keyContainer);
			delete keyContainer;
		}
	}

};

Model *modelComputerscarePatchVersioning = createModel<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare-patch-versioning");
