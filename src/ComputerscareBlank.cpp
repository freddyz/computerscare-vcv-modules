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
	bool loading = true;
	bool loadedJSON = false;
	std::string path;
	std::string lastPath;
	float width = 120;
	float height = 380;
	bool invertY = false;
	float zoom = 1.f;
	float xOffset = 0.f;
	float yOffset = 0.f;
	int imageFitEnum = 0;
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
	void loadImageDialog() {
		std::string dir = this->path.empty() ?  asset::user("") : rack::string::directory(this->path);
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
	void setWidth(float w) {
		this->width = w;
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "path", json_string(path.c_str()));
		json_object_set_new(rootJ, "width", json_real(width));
		json_object_set_new(rootJ, "imageFitEnum", json_integer(imageFitEnum));
		json_object_set_new(rootJ, "invertY", json_boolean(invertY));
		json_object_set_new(rootJ, "zoom", json_real(zoom));
		json_object_set_new(rootJ, "xOffset", json_real(xOffset));
		json_object_set_new(rootJ, "yOffset", json_real(yOffset));
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
		json_t *imageFitEnumJ = json_object_get(rootJ, "imageFitEnum");
		if (imageFitEnumJ) { imageFitEnum = json_integer_value(imageFitEnumJ); }
		json_t *invertYJ = json_object_get(rootJ, "invertY");
		if (invertYJ) { invertY = json_is_true(invertYJ); }
		json_t *zoomJ = json_object_get(rootJ, "zoom");
		if (zoomJ) {
			zoom = json_number_value(zoomJ);
		}
		json_t *xOffsetJ = json_object_get(rootJ, "xOffset");
		if (xOffsetJ) {
			xOffset = json_number_value(xOffsetJ);
		}
		json_t *yOffsetJ = json_object_get(rootJ, "yOffset");
		if (yOffsetJ) {
			yOffset = json_number_value(yOffsetJ);
		}
		this->loading = false;
	}

};
struct LoadImageItem : MenuItem {
	ComputerscareBlank* module;
	void onAction(const event::Action& e) override {
		module->loadImageDialog();
	}
};
struct ImageFitModeItem : MenuItem {
	ComputerscareBlank *blank;
	int imageFitEnum;
	void onAction(const event::Action &e) override {
		blank->imageFitEnum = imageFitEnum;
	}
	void step() override {
		rightText = CHECKMARK(blank->imageFitEnum == imageFitEnum);
		MenuItem::step();
	}
};
struct InvertYMenuItem: MenuItem {
	ComputerscareBlank *blank;
	InvertYMenuItem() {

	}
	void onAction(const event::Action &e) override {
		blank->invertY = !blank->invertY;
	}
	void step() override {
		rightText = blank->invertY ? "✔" : "";
		MenuItem::step();
	}
};

struct PNGDisplay : TransparentWidget {
	ComputerscareBlank *module;
	const float width = 125.0f;
	const float height = 130.0f;

	int imgWidth, imgHeight;
	float imgRatio;
	int lastEnum = -1;
	std::string path = "";
	bool first = true;
	int img = 0;

	PNGDisplay() {
	}

	void draw(const DrawArgs &args) override {
		if (module && module->loadedJSON) {
				if (path != module->path) {
					img = nvgCreateImage(args.vg, module->path.c_str(), 0);
					nvgImageSize(args.vg, img, &imgWidth, &imgHeight);
					imgRatio = ((float)imgWidth / (float)imgHeight);
					path = module->path;
				}

				/*if (module->imageFitEnum != lastEnum) {
					lastEnum = module->imageFitEnum;
					module->xOffset = 0;
					module->yOffset = 0;
					module->zoom = 1;
				}*/
				if (!path.empty()) {
					nvgBeginPath(args.vg);
					NVGpaint imgPaint;
					//if (module->width>0 && module->height>0)
					nvgScale(args.vg, module->zoom, module->zoom);
					if (module->imageFitEnum == 0) {
						imgPaint = nvgImagePattern(args.vg, module->xOffset, module->yOffset, module->width, module->height, 0, img, 1.0f);

					}
					else if (module->imageFitEnum == 1) { // fit width
						//nvgScale(args.vg, width/module->width, height/module->height);
						imgPaint = nvgImagePattern(args.vg, module->xOffset, module->yOffset, module->width, module->width / imgRatio, 0, img, 1.0f);
					}
					else if (module->imageFitEnum == 2) {
						imgPaint = nvgImagePattern(args.vg, module->xOffset, module->yOffset, module->height * imgRatio, module->height, 0, img, 1.0f);
					}
					nvgRect(args.vg, 0, 0, module->width, module->height);
					nvgFillPaint(args.vg, imgPaint);
					nvgFill(args.vg);
					nvgClosePath(args.vg);
				}
		}
	}
	void onHoverKey(const event::HoverKey& e) override;
};
void PNGDisplay::onHoverKey(const event::HoverKey& e) {
	float dZoom = 0.05;
	float dPosition = 10.f;
	if (e.isConsumed())
		return;

	// Scroll with arrow keys
	/*float arrowSpeed = 30.0;
	if ((e.mods & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT))
		arrowSpeed /= 16.0;
	else if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL)
		arrowSpeed *= 4.0;
	else if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT)
		arrowSpeed /= 4.0;*/
	if (e.action == RACK_HELD) {
		switch (e.key) {
		case GLFW_KEY_A: {
			module->xOffset += dPosition;
			e.consume(this);
		} break;
		case GLFW_KEY_S: {
			module->yOffset -= module->invertY ? dPosition : -dPosition;
			e.consume(this);
		} break;
		case GLFW_KEY_D: {
			module->xOffset -= dPosition;
			e.consume(this);
		} break;
		case GLFW_KEY_W: {
			module->yOffset += module->invertY ? dPosition : -dPosition;
			e.consume(this);
		} break;
		case GLFW_KEY_Z: {
			module->zoom += dZoom;
			e.consume(this);
		} break;
		case GLFW_KEY_X: {
			module->zoom -= dZoom;
			e.consume(this);
		} break;
		}
	}
}
struct ComputerscareBlankWidget : ModuleWidget {
	ComputerscareBlankWidget(ComputerscareBlank *module) {

		setModule(module);
		if (module) {
			this->blankModule = module;
			DEBUG("width:%f", module->width);
			box.size = Vec(module->width, module->height);
		} else {
			box.size = Vec(8 * 15, 380);
		}
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
			this->panel = panel;
			addChild(panel);
		}

		if (module) {
			{
				PNGDisplay *pngDisplay = new PNGDisplay();
				pngDisplay->module = module;
				pngDisplay->box.pos = Vec(0, 0);

				pngDisplay->box.size = Vec( module->width , module->height );

				this->pngDisplay = pngDisplay;
				addChild(pngDisplay);
			}
		}
		ComputerscareResizeHandle *leftHandle = new ComputerscareResizeHandle();

		ComputerscareResizeHandle *rightHandle = new ComputerscareResizeHandle();
		rightHandle->right = true;
		this->rightHandle = rightHandle;
		addChild(leftHandle);
		addChild(rightHandle);

	}

	void appendContextMenu(Menu* menu) override {
		ComputerscareBlank* blank = dynamic_cast<ComputerscareBlank*>(this->module);

		menu->addChild(new MenuEntry);

		LoadImageItem* loadImageItem = createMenuItem<LoadImageItem>("Load image");
		loadImageItem->module = blank;
		menu->addChild(loadImageItem);

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Image Scaling"));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Fit Both (stretch both directions)", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 0));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Fit Width", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 1));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Fit Height", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 2));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

		InvertYMenuItem *invertYMenuItem = new InvertYMenuItem();
		invertYMenuItem->text = "Invert Y-Axis";
		invertYMenuItem->blank = blank;
		menu->addChild(invertYMenuItem);



	}
	void step() override;
	ComputerscareBlank *blankModule;
	PNGDisplay *pngDisplay;
	ComputerscareSVGPanel *panel;
	TransparentWidget *display;
	ComputerscareResizeHandle *leftHandle;
	ComputerscareResizeHandle *rightHandle;
	SmallLetterDisplay* smallLetterDisplay;
};
void ComputerscareBlankWidget::step() {
	if (blankModule) {
		//if (!blankModule->loading) {
			if (!blankModule->loadedJSON) {
				DEBUG("we aint loaded the json man:%f", blankModule->width);
				box.size.x = blankModule->width;
				panel->box.size.x = blankModule->width;
				pngDisplay->box.size.x = blankModule->width;
				rightHandle->box.pos.x = blankModule->width - rightHandle->box.size.x;
				blankModule->loadedJSON = true;
			}
			else if (box.size.x != blankModule->width) {
				DEBUG("widget step width:%f", blankModule->width);
				blankModule->width = box.size.x;

				panel->box.size = box.size;
				pngDisplay->box.size.x = box.size.x;
				rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
			}
			ModuleWidget::step();
		}
	//}
}


Model *modelComputerscareBlank = createModel<ComputerscareBlank, ComputerscareBlankWidget>("computerscare-blank");
