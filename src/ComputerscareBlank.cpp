#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
#include "animatedGif.hpp"
#include <osdialog.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

struct ComputerscareBlank;

struct ComputerscareBlank : Module {
	bool loading = true;
	bool loadedJSON = false;
	std::string path;

	std::vector<std::string> paths;
	float width = 120;
	float height = 380;
	int rotation = 0;
	bool invertY = true;
	float zoomX = 1.f;
	float zoomY = 1.f;
	float xOffset = 0.f;
	float yOffset = 0.f;
	int imageFitEnum = 0;
	int currentFrame = 0;
	int numFrames = 0;
	int stepCounter = 0;
	float frameDelay = .5;
	int samplesDelay = 10000;
	int speed = 100000;
	int imageStatus = 0;

	ComputerscareSVGPanel* panelRef;
	enum ParamIds {
		ANIMATION_SPEED,
		ANIMATION_ENABLED,
		CONSTANT_FRAME_DELAY,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS,
		CLOCK_INPUT,
		RESET_INPUT
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareBlank()  {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ANIMATION_SPEED, 0.f, 2.f, 0.1, "Animation Speed");
		configParam(ANIMATION_ENABLED, 0.f, 1.f, 1.f, "Animation Enabled");
		configParam(CONSTANT_FRAME_DELAY, 0.f, 1.f, 0.f, "Constant Frame Delay");

		paths.push_back("empty");
	}
	void process(const ProcessArgs &args) override {
		stepCounter++;
		samplesDelay = frameDelay * args.sampleRate;

		if (stepCounter > samplesDelay) {
			stepCounter = 0;
			if (params[ANIMATION_ENABLED].getValue()) {
				if (numFrames > 1) {
					currentFrame ++;
					currentFrame %= numFrames;
				}
			}
		}
	}
	void onReset() override {
		zoomX = 1;
		zoomY = 1;
		xOffset = 0;
		yOffset = 0;
	}
	void loadImageDialog(int index = 0) {
		std::string dir = this->paths[index].empty() ?  asset::user("../") : rack::string::directory(this->paths[index]);
		char* pathC = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
		if (!pathC) {
			return;
		}
		std::string path = pathC;
		std::free(pathC);

		setPath(path);
	}

	void setPath(std::string path, int index = 0) {
		//if (paths.size() <= index) {
		//paths.push_back(path);
		//}
		//else {
		paths[index] = path;
		//}
		printf("setted %s\n", path.c_str());
		//numFrames = paths.size();
		currentFrame = 0;
	}
	void setFrameCount(int frameCount) {
		DEBUG("setting frame count %i", frameCount);
		numFrames = frameCount;
	}
	void setImageStatus(int status) {
		imageStatus = status;
	}
	void setFrameDelay(float frameDelaySeconds) {
		if (params[CONSTANT_FRAME_DELAY].getValue()) {
			frameDelay = params[ANIMATION_SPEED].getValue();
		}
		else {
			frameDelay = frameDelaySeconds;
		}
	}
	std::string getPath() {
		//return numFrames > 0 ? paths[currentFrame] : "";
		return paths[0];
	}
	void nextFrame() {
		currentFrame++;
		currentFrame %= numFrames;
	}
	void prevFrame() {
		currentFrame--;
		currentFrame += numFrames;
		currentFrame %= numFrames;
	}
	void goToFrame(int frameNum) {
		currentFrame = 0;
	}
	void goToRandomFrame() {
		currentFrame = (int) std::floor(random::uniform()*numFrames);
	}
	void toggleAnimationEnabled() {
		float current = params[ANIMATION_ENABLED].getValue();
		if (current == 1.0) {
			params[ANIMATION_ENABLED].setValue(0);
		}
		else {
			params[ANIMATION_ENABLED].setValue(1);
		}
	}
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		if (paths.size() > 0) {
			json_object_set_new(rootJ, "path", json_string(paths[0].c_str()));
		}

		json_object_set_new(rootJ, "width", json_real(width));
		json_object_set_new(rootJ, "imageFitEnum", json_integer(imageFitEnum));
		json_object_set_new(rootJ, "invertY", json_boolean(invertY));
		json_object_set_new(rootJ, "zoomX", json_real(zoomX));
		json_object_set_new(rootJ, "zoomY", json_real(zoomY));
		json_object_set_new(rootJ, "xOffset", json_real(xOffset));
		json_object_set_new(rootJ, "yOffset", json_real(yOffset));
		json_object_set_new(rootJ, "rotation", json_integer(rotation));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {



		json_t *pathJ = json_object_get(rootJ, "path");
		if (pathJ) {
			setPath(json_string_value(pathJ));
		}

		json_t *widthJ = json_object_get(rootJ, "width");
		if (widthJ)
			width = json_number_value(widthJ);
		json_t *imageFitEnumJ = json_object_get(rootJ, "imageFitEnum");
		if (imageFitEnumJ) { imageFitEnum = json_integer_value(imageFitEnumJ); }

		json_t *invertYJ = json_object_get(rootJ, "invertY");
		if (invertYJ) { invertY = json_is_true(invertYJ); }
		json_t *zoomXJ = json_object_get(rootJ, "zoomX");
		if (zoomXJ) {
			zoomX = json_number_value(zoomXJ);
		}
		json_t *zoomYJ = json_object_get(rootJ, "zoomY");
		if (zoomYJ) {
			zoomY = json_number_value(zoomYJ);
		}
		json_t *xOffsetJ = json_object_get(rootJ, "xOffset");
		if (xOffsetJ) {
			xOffset = json_number_value(xOffsetJ);
		}
		json_t *yOffsetJ = json_object_get(rootJ, "yOffset");
		if (yOffsetJ) {
			yOffset = json_number_value(yOffsetJ);
		}
		json_t *rotationJ = json_object_get(rootJ, "rotation");
		if (rotationJ) { rotation = json_integer_value(rotationJ); }
		this->loading = false;
	}

};
struct LoadImageItem : MenuItem {
	ComputerscareBlank* blankModule;
	void onAction(const event::Action& e) override {
		blankModule->loadImageDialog();
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
	ComputerscareBlank *blankModule;

	int imgWidth, imgHeight;
	float imgRatio, widgetRatio;
	int lastEnum = -1;
	std::string path = "empty";
	int img = 0;
	int currentFrame = -1;
	AnimatedGifBuddy gifBuddy;

	PNGDisplay() {
	}


	void setZooms() {
		if (blankModule->imageFitEnum == 0) {
			blankModule->zoomX = blankModule->width / imgWidth;
			blankModule->zoomY = blankModule->height / imgHeight;
			blankModule->xOffset = 0;
			blankModule->yOffset = 0;

		} else if (blankModule->imageFitEnum == 1) { // fit width
			blankModule->zoomX = blankModule->width / imgWidth;
			blankModule->zoomY = blankModule->zoomX;
			blankModule->xOffset = 0;
			blankModule->yOffset = 0;

		} else if (blankModule->imageFitEnum == 2) { //fit height
			blankModule->zoomY = blankModule->height / imgHeight;
			blankModule->zoomX = blankModule->zoomY;
			blankModule->xOffset = 0;
			blankModule->yOffset = 0;
		}
		else if (blankModule->imageFitEnum == 3) {

		}


	}
	void draw(const DrawArgs &args) override {
		if (blankModule && blankModule->loadedJSON) {
			std::string modulePath = blankModule->getPath();
			if (path != modulePath) {
				DEBUG("path not module path");
				DEBUG("path: %s, modulePath:%s", path.c_str(), modulePath.c_str());
				gifBuddy = AnimatedGifBuddy(args.vg, modulePath.c_str());
				if (gifBuddy.getImageStatus() == 3) {
					std::string badGifPath = asset::plugin(pluginInstance, "res/bad-gif.gif");
					gifBuddy = AnimatedGifBuddy(args.vg, badGifPath.c_str());
				}
				img = gifBuddy.getHandle();

				blankModule->setFrameCount(gifBuddy.getFrameCount());
				blankModule->setFrameDelay(gifBuddy.getSecondsDelay(0));
				blankModule->setImageStatus(gifBuddy.getImageStatus());



				nvgImageSize(args.vg, img, &imgWidth, &imgHeight);
				imgRatio = ((float)imgWidth / (float)imgHeight);
				path = modulePath;
				if (path != "empty") {
					setZooms();
				}


			}

			if (blankModule->imageFitEnum != lastEnum && lastEnum != -1) {
				lastEnum = blankModule->imageFitEnum;
				setZooms();
			}
			lastEnum = blankModule->imageFitEnum;
			if (!path.empty() && path != "empty") {
				nvgBeginPath(args.vg);
				NVGpaint imgPaint;
				nvgScale(args.vg, blankModule->zoomX, blankModule->zoomY);
				imgPaint = nvgImagePattern(args.vg, blankModule->xOffset, blankModule->yOffset, imgWidth, imgHeight, 0, img, 1.0f);
				nvgRect(args.vg, blankModule->xOffset, blankModule->yOffset, imgWidth, imgHeight);
				nvgFillPaint(args.vg, imgPaint);
				nvgFill(args.vg);
				nvgClosePath(args.vg);
			}
			if (blankModule->currentFrame != currentFrame) {
				currentFrame = blankModule->currentFrame;
				blankModule->setFrameDelay(gifBuddy.getSecondsDelay(currentFrame));
				gifBuddy.displayGifFrame(args.vg, currentFrame);
			}
		}
	}
};

struct ComputerscareBlankWidget : ModuleWidget {
	ComputerscareBlankWidget(ComputerscareBlank *blankModule) {

		setModule(blankModule);
		if (blankModule) {
			this->blankModule = blankModule;
			box.size = Vec(blankModule->width, blankModule->height);
		} else {
			box.size = Vec(8 * 15, 380);
		}
		{
			BGPanel *bgPanel = new BGPanel(nvgRGB(0xE0, 0xE0, 0xD9));
			bgPanel->box.size = box.size;
			this->bgPanel = bgPanel;
			addChild(bgPanel);
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareCustomBlankPanel.svg")));
			this->panel = panel;
			addChild(panel);

		}

		if (blankModule) {
			{
				PNGDisplay *pngDisplay = new PNGDisplay();
				pngDisplay->blankModule = blankModule;
				pngDisplay->box.pos = Vec(0, 0);

				pngDisplay->box.size = Vec( blankModule->width , blankModule->height );

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
		ComputerscareBlank* blank = dynamic_cast<ComputerscareBlank*>(this->blankModule);

		menu->addChild(new MenuEntry);

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Keyboard Controls:"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "A,S,D,F: Translate image position"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Z,X: Zoom in/out"));


		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));
		LoadImageItem* loadImageItem = createMenuItem<LoadImageItem>("Load image (PNG, JPEG, BMP, GIF)");
		loadImageItem->blankModule = blank;
		menu->addChild(loadImageItem);

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Current Image Path:"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, blank->getPath()));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Image Scaling"));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Fit Both (stretch both directions)", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 0));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Fit Width", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 1));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Fit Height", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 2));
		menu->addChild(construct<ImageFitModeItem>(&MenuItem::text, "Free", &ImageFitModeItem::blank, blank, &ImageFitModeItem::imageFitEnum, 3));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

		InvertYMenuItem *invertYMenuItem = new InvertYMenuItem();
		invertYMenuItem->text = "Invert Y-Axis";
		invertYMenuItem->blank = blank;
		menu->addChild(invertYMenuItem);

		SmoothKnob* speedParam = new SmoothKnob();
		speedParam->paramQuantity = blankModule->paramQuantities[ComputerscareBlank::ANIMATION_SPEED];
		
		MenuEntry* LabeledKnob = new MenuEntry();
		MenuLabel* johnLabel = construct<MenuLabel>(&MenuLabel::text, "Animation Speed");
		johnLabel->box.pos = Vec(speedParam->box.size.x,0);

		LabeledKnob->addChild(johnLabel);
		LabeledKnob->addChild(speedParam);

		//menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Animation Speed"));
		menu->addChild(LabeledKnob);

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));



	}
	void step() override {
		if (module) {
			if (blankModule && !blankModule->loadedJSON) {
				box.size.x = blankModule->width;
				panel->box.size.x = blankModule->width;
				bgPanel->box.size.x = blankModule->width;
				panel->box.pos.x = blankModule->width / 2 - 60.f;
				pngDisplay->box.size.x = blankModule->width;
				//pngDisplay->box.pos.x = blankModule->xOffset;
				//pngDisplay->box.pos.y = blankModule->yOffset;
				rightHandle->box.pos.x = blankModule->width - rightHandle->box.size.x;
				blankModule->loadedJSON = true;
			}
			else {
				if (box.size.x != blankModule->width) {
					blankModule->width = box.size.x;
					panel->box.pos.x = box.size.x / 2 - 60.f;
					pngDisplay->box.size.x = box.size.x;

					bgPanel->box.size.x = box.size.x;
					rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
					pngDisplay->setZooms();
				}
				panel->visible = blankModule->path.empty();
			}
			ModuleWidget::step();
		}
	};
	void onHoverKey(const event::HoverKey& e) override {
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
		//duplicate is ctrl-d, ignore keys if mods are pressed so duplication doesnt translate the image
		if (e.action == RACK_HELD && !e.mods ) {
			switch (e.key) {
			case GLFW_KEY_A: {
				blankModule->xOffset += dPosition / blankModule->zoomX;
				e.consume(this);
			} break;
			case GLFW_KEY_S: {
				blankModule->yOffset -= (blankModule->invertY ? dPosition : -dPosition) / blankModule->zoomY;
				e.consume(this);
			} break;
			case GLFW_KEY_D: {
				blankModule->xOffset -= dPosition / blankModule->zoomX;
				e.consume(this);
			} break;
			case GLFW_KEY_W: {
				blankModule->yOffset += (blankModule->invertY ? dPosition : -dPosition) / blankModule->zoomY;
				e.consume(this);
			} break;
			case GLFW_KEY_Z: {
				blankModule->zoomX *= (1 + dZoom);
				blankModule->zoomY *= (1 + dZoom);
				e.consume(this);
			} break;
			case GLFW_KEY_X: {
				blankModule->zoomX *= (1 - dZoom);
				blankModule->zoomY *= (1 - dZoom);
				e.consume(this);
			} break;
			case GLFW_KEY_Q: {
				blankModule->rotation += 1;
				blankModule->rotation %= 4;
				e.consume(this);
			} break;
			case GLFW_KEY_E: {
				blankModule->rotation -= 1;
				blankModule->rotation += 4;
				blankModule->rotation %= 4;
				e.consume(this);
			} break;
			case GLFW_KEY_J: {
				blankModule->prevFrame();
				e.consume(this);
			} break;
			case GLFW_KEY_L: {
				blankModule->nextFrame();
				e.consume(this);
			} break;
			case GLFW_KEY_K: {
				blankModule->goToFrame(0);
				e.consume(this);
			} break;
			case GLFW_KEY_I: {
				blankModule->goToRandomFrame();
				e.consume(this);
			} break;
			case GLFW_KEY_U: {
				blankModule->goToRandomFrame();
				e.consume(this);
			} break;
			case GLFW_KEY_P: {
				blankModule->toggleAnimationEnabled();
				e.consume(this);
			} break;

			}
		}
		ModuleWidget::onHoverKey(e);
	}
	ComputerscareBlank *blankModule;
	PNGDisplay *pngDisplay;
	ComputerscareSVGPanel *panel;
	BGPanel *bgPanel;
	TransparentWidget *display;
	ComputerscareResizeHandle *leftHandle;
	ComputerscareResizeHandle *rightHandle;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareBlank = createModel<ComputerscareBlank, ComputerscareBlankWidget>("computerscare-blank");