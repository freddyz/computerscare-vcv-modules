#include "Computerscare.hpp"
#include <patch.hpp>
#include <dirent.h>

struct ComputerscarePatchVersioning;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

const std::string numbers = "0123456789";
const std::string letters = "abcdefghijklmnopqrstuvwxyz";

struct ComputerscarePatchVersioning : Module {
	int counter = 0;
	ComputerscareSVGPanel* panelRef;
	rack::dsp::SchmittTrigger saveTrigger;

	std::vector<std::string> patchVersionFilenames;

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
		scanPatchVersions();
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

		DEBUG("data FROM jsoN jaja");

		json_t *counterJ = json_object_get(rootJ, "counter");
		if (counterJ) { counter = json_integer_value(counterJ); }

		json_t *filenamesJ = json_object_get(rootJ, "patchVersionFilenames");



		if (filenamesJ) {
			DEBUG("jajaja okay...");
			for (int i = 0; i < json_array_size(filenamesJ); i++) {
				json_t *filenameJ = json_array_get(filenamesJ, i);
				//patchVersionFilenames.push_back(json_string_value(filenameJ));
			}
		}
	}

	void process(const ProcessArgs &args) override {
		bool saveClicked = saveTrigger.process(params[SAVE_BUTTON].getValue());
		if (saveClicked) {
			savePatch();
		}
	}
	void selectedPatch(int index) {
		DEBUG("jaja loaded patch %i %s", index, patchVersionFilenames[index].c_str());

		loadPatch(index);
	}
	void goBack() {

	}
	void goForward() {

	}
	void goUp() {

	}
	void goDown() {

	}
	void goIn() {

	}
	void goOut() {

	}
	void goLeft() {

	}
	void goRight() {

	}

	void mixinJSON(int mixinCounter, std::vector<std::string> mixinFilenames) {

		//dataFromJson(prevData);
		//counter = oldCounter;

		//patchVersionFilenames.push_back("HOGFUCKER");
		DEBUG("HOGFUCKER %i", mixinFilenames.size());
		//counter = mixinCounter;
		//patchVersionFilenames.resize(mixinFilenames.size());
		//patchVersionFilenames = mixinFilenames;
		DEBUG("counter%i, size:%i", counter, patchVersionFilenames.size());
	}

	void loadPatch(int index) {

		int prevSize = patchVersionFilenames.size();

		std::vector<std::string> oldFilenames = patchVersionFilenames;
		//int masterCounter = counter;

		DEBUG("previous size:%i", prevSize);



		std::string patchBasename = getPatchBasename();
		std::string patchFilename = patchVersionFilenames[index];
		std::string versionsFolder = createPatchDirectory(patchBasename);
		std::string patchPath = system::join(versionsFolder, patchFilename);

		std::string dstFilename =  asset::user("pork.vcv");

		DEBUG("loading patch %s", patchPath.c_str());

		DEBUG("dstFilename %s", dstFilename.c_str());



		system::copy(patchPath, dstFilename);

		APP->patch->load(patchPath);




		DEBUG("loaded %s", patchPath.c_str());

		//mixinJSON(masterCounter, oldFilenames);
	}
	void scanPatchVersions() {
		std::string dir = createPatchDirectory(getPatchBasename());
		int versionIndex = 0;

		struct dirent* dirp = NULL;
		DIR* rep = NULL;
		rep = opendir(dir.c_str());
		patchVersionFilenames.clear();

		if (rep) {
			while ((dirp = readdir(rep)) != NULL) {
				std::string name = dirp->d_name;

				std::size_t found = name.find(".vcv", name.length() - 5);
				if (found != std::string::npos) {
					patchVersionFilenames.push_back(name);
					/*if (currentImageFullpath == paths[index]) {
						versionIndex = versionIndex;
					}*/
					versionIndex++;
				}
			}
		}
	}

	std::string generateNewPatchName() {
		return getPatchBasename() + " v" + std::to_string(counter) + ".vcv";
	}
	std::string getPatchBasename(std::string input = APP->patch->path) {
		std::string currentPatchName = system::getFilename(input);

		size_t lastindex = currentPatchName.find_last_of(".");
		return currentPatchName.substr(0, lastindex);
	}
	void savePatch() {
		counter++;
		std::string newPatchName = generateNewPatchName();
		std::string versionDir = createPatchDirectory(getPatchBasename());

		copyPatch(system::join(versionDir, newPatchName));
		patchVersionFilenames.push_back(newPatchName);
		DEBUG("versioned %s", newPatchName.c_str());

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
						module->goBack();
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
struct VersionsDisplay : SmallLetterDisplay
{
	ComputerscarePatchVersioning *module;
	VersionsDisplay()
	{
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		if (module)
		{

			std::string versionString =  ("v" + std::to_string(module->counter) );
			value = versionString;


		}
		else {
			value = std::to_string((random::u32() % 24) + 1);
		}
		SmallLetterDisplay::draw(args);
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

		VersionsDisplay *versDisplay = new VersionsDisplay();
		versDisplay->module = module;
		versDisplay->box.pos = mm2px(Vec(10, 24));
		versDisplay->box.size = mm2px(Vec(19, 7));
		versDisplay->value = "Z";
		versDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
		addChild(versDisplay);
	}
	void appendContextMenu(Menu *menu)
	{
		ComputerscarePatchVersioning *module = dynamic_cast<ComputerscarePatchVersioning *>(this->module);

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		menu->addChild(createSubmenuItem("Patch Versions", "",
		[ = ](Menu * menu) {
			menu->addChild(createMenuLabel("Load Patch:"));

			DEBUG("patchVersionFilenames.size %i", (int)module->patchVersionFilenames.size());
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
	VersionsDisplay *versDisplay;

};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscarePatchVersioning = Model::create<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscarePatchVersioning = createModel<ComputerscarePatchVersioning, ComputerscarePatchVersioningWidget>("computerscare-patch-versioning");
