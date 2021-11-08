#include "Computerscare.hpp"
#include <patch.hpp>

struct ComputerscarePatchVersioning;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

const std::string numbers = "0123456789";
const std::string letters = "abcdefghijklmnopqrstuvwxyz";

std::string generateNewPatchName() {
	std::string currentPatchName = APP->patch->path;
	size_t lastindex = currentPatchName.find_last_of(".");
	std::string rawname = currentPatchName.substr(0, lastindex);
	return rawname + "-v.vcv";
}

struct ComputerscarePatchVersioning : Module {
	int counter = 0;
	ComputerscareSVGPanel* panelRef;
	rack::dsp::SchmittTrigger saveTrigger;
	enum ParamIds {
		KNOB,
		SAVE_BUTTON,
		NUM_PARAMS

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


	ComputerscarePatchVersioning()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		/* for (int i = 0; i < numKnobs; i++) {
				configParam(KNOB + i, 0.0f, 10.0f, 0.0f);
				configParam(KNOB+i, 0.f, 10.f, 0.f, "Channel "+std::to_string(i+1) + " Voltage", " Volts");
		}
		configParam(TOGGLES, 0.0f, 1.0f, 0.0f);
		outputs[POLY_OUTPUT].setChannels(16);*/
	}
	void process(const ProcessArgs &args) override {
		bool saveClicked = saveTrigger.process(params[SAVE_BUTTON].getValue());
		if (saveClicked) {
			savePatch();
		}
	}
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "counter", json_integer(counter));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		float val;

		json_t *counterJ = json_object_get(rootJ, "counter");
		if (counterJ) { counter = json_integer_value(counterJ); }

	}
	void savePatch() {
		std::string newPatchName = generateNewPatchName();
		APP->patch->save(newPatchName);
		APP->patch->path = newPatchName;
		APP->history->setSaved();
	}

	void onAdd(const AddEvent& e) override {
		DEBUG("onAdd callud");
		std::string path = system::join(createPatchStorageDirectory(), "wavetable.wav");
		// Read file...

		//savePatchInStorage("lastPatch.vcv");
	}

	void onSave(const SaveEvent& e) override {
		DEBUG("onSave culled");
		counter++;
		std::string filename = "patch-" + std::to_string(counter);
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
	~ComputerscarePatchVersioningWidget() {
		if (keyContainer) {
			APP->scene->rack->removeChild(keyContainer);
			delete keyContainer;
		}
	}

};

Model *modelComputerscarePatchVersioning = createModel<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare-patch-versioning");
