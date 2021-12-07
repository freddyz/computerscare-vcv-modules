#include "Computerscare.hpp"
#include <patch.hpp>

struct ComputerscarePatchVersioning;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

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
	std::string generateNewPatchName() {
		return getPatchBasename() + " v" + std::to_string(counter) + ".vcv";
	}
	std::string getPatchBasename() {
		std::string currentPatchName = system::getFilename(APP->patch->path);

		size_t lastindex = currentPatchName.find_last_of(".");
		return currentPatchName.substr(0, lastindex);
	}
	void savePatch() {
		counter++;
		std::string newPatchName = generateNewPatchName();
		std::string versionDir = createPatchDirectory(getPatchBasename());

		copyPatch(system::join(versionDir, newPatchName));
	}
	std::string createPatchDirectory(std::string name) {

		std::string patchesDir;
		std::string filename;
		patchesDir = asset::user("patches");
		std::string versionDir = system::join(patchesDir, "versions", name);
		system::createDirectories(versionDir);
		return versionDir;
	}
	void onSave(const SaveEvent& e) override {
		savePatch();
	}
	void copyPatch(std::string dstPath) {
		std::string currentPatchName = APP->patch->path;
		system::copy(currentPatchName, dstPath);
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
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePatchVersioningPanel.svg")));

		float outputY = 334;
		box.size = Vec(15 * 10, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareIsoPanel.svg")));

			//module->panelRef = panel;

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


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscarePatchVersioning = Model::create<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscarePatchVersioning = createModel<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare-patch-versioning");
