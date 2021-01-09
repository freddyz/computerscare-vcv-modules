#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
#include "animatedGif.hpp"
#include "CustomBlankFunctions.hpp"

#include <osdialog.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <dirent.h>
#include <algorithm>
#include <random>

#define FONT_SIZE 13


struct ComputerscareBlank : ComputerscareMenuParamModule {
	bool loading = true;
	bool loadedJSON = false;
	bool ready = false;
	std::string path;
	std::string parentDirectory;

	std::vector<std::string> paths;
	std::vector<std::string> catalog;
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
	int mappedFrame = 0;
	int numFrames = 0;
	int sampleCounter = 0;
	float frameDelay = .5;
	std::vector<float> frameDelays;
	std::vector<int> frameMapForScan;
	std::vector<int> shuffledFrames;

	float totalGifDuration = 0.f;

	int samplesDelay = 10000;
	int speed = 100000;
	int imageStatus = 0;
	bool scrubbing = false;
	int scrubFrame = 0;

	int pingPongDirection = 1;

	float speedFactor = 1.f;

	float zeroOffset = 0.f;

	bool tick = false;

	/*
		uninitialized: 0
		gif: 1
		not gif: 2
		error:3
	*/

	bool expanderConnected = false;

	int clockMode = 0;
	bool clockConnected = false;
	bool resetConnected = false;
	bool speedConnected = false;

	std::vector<std::string> animationModeDescriptions;
	std::vector<std::string> endBehaviorDescriptions;

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger resetButtonTrigger;

	dsp::Timer syncTimer;


	ComputerscareSVGPanel* panelRef;

	float leftMessages[2][8] = {};

	enum ParamIds {
		ANIMATION_SPEED,
		ANIMATION_ENABLED,
		CONSTANT_FRAME_DELAY,
		ANIMATION_MODE,
		END_BEHAVIOR,
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
		configMenuParam(ANIMATION_SPEED, 0.05f, 20.f, 1.0, "Animation Speed", 2, "x");
		configParam(ANIMATION_ENABLED, 0.f, 1.f, 1.f, "Animation Enabled");
		configParam(CONSTANT_FRAME_DELAY, 0.f, 1.f, 0.f, "Constant Frame Delay");
		configMenuParam(END_BEHAVIOR, 0.f, 5.f, 0.f, "Animation End Behavior", 2);

		animationModeDescriptions.push_back("Forward");
		animationModeDescriptions.push_back("Reverse");
		animationModeDescriptions.push_back("Ping Pong");
		animationModeDescriptions.push_back("Random Shuffled");
		animationModeDescriptions.push_back("Full Random");

		endBehaviorDescriptions.push_back("Repeat");
		endBehaviorDescriptions.push_back("Stop");
		endBehaviorDescriptions.push_back("Select Random");
		endBehaviorDescriptions.push_back("Load Next");
		endBehaviorDescriptions.push_back("Load Previous");

		configMenuParam(ANIMATION_MODE, 0.f, "Animation Mode", animationModeDescriptions);

		paths.push_back("empty");

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

	}
	void process(const ProcessArgs &args) override {
		if (imageStatus == 1) {
			sampleCounter++;
		}

		samplesDelay = frameDelay * args.sampleRate;

		bool shouldAdvanceAnimation = false;
		if (leftExpander.module && leftExpander.module->model == modelComputerscareBlankExpander) {
			expanderConnected = true;
			// me
			float *messageToSendToExpander = (float*) leftExpander.module->rightExpander.producerMessage;

			float *messageFromExpander = (float*) leftExpander.consumerMessage;

			clockMode = messageFromExpander[0];
			clockConnected = messageFromExpander[1];
			resetConnected = messageFromExpander[3];
			speedConnected = messageFromExpander[5];

			zeroOffset = messageFromExpander[7];

			scrubbing = messageFromExpander[8];



			updateScrubFrame();

			if (clockConnected) {
				bool clockTriggered = clockTrigger.process(messageFromExpander[2]);
				if (clockMode == 0) {
					//sync
					float currentSyncTime = syncTimer.process(args.sampleTime);
					if (clockTriggered) {
						syncTimer.reset();
						setSyncTime(currentSyncTime);
						if (params[ANIMATION_ENABLED].getValue()) {
							goToFrame(0);
						}
					}
				}

				else if (clockMode == 1) {
					//scan
					float scanPosition = messageFromExpander[2];
					scanToPosition(scanPosition);
				}
				else if (clockMode == 2) {
					//frame advance
					shouldAdvanceAnimation = clockTriggered;
				}
			}

			if (resetConnected) {
				if (resetTrigger.process(messageFromExpander[4])) {
					DEBUG("RESET TRIGGER");
					goToFrame(0);
				}
			}
			if(resetButtonTrigger.process(messageFromExpander[9])) {
				goToFrame(0);
			}

			messageToSendToExpander[0] = float (currentFrame);
			messageToSendToExpander[1] = float (numFrames);
			messageToSendToExpander[2] = float (mappedFrame);
			messageToSendToExpander[3] = float (scrubFrame);
			messageToSendToExpander[4] = float (tick);
			
			// Flip messages at the end of the timestep
			leftExpander.module->rightExpander.messageFlipRequested = true;
		}
		else {
			expanderConnected = false;
		}

		if (clockConnected && (clockMode == 2)) {

		}
		else {
			if (sampleCounter > samplesDelay) {
				sampleCounter = 0;
				shouldAdvanceAnimation = true;
			}
		}
		if (params[ANIMATION_ENABLED].getValue() && shouldAdvanceAnimation) {
			tickAnimation();
		}
	}

	void updateScrubFrame() {
		if (ready) {
			scrubFrame = mapBlankFrameOffset(zeroOffset, numFrames);
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
		parentDirectory = dir;
		std::string path = pathC;
		std::free(pathC);

		setPath(path);
	}
	void getContainingDirectory(int index = 0) {

		std::string dir = rack::string::directory(paths[index]);

		struct dirent* dirp = NULL;
		DIR* rep = NULL;

		rep = opendir(dir.c_str());

		int i = 0;
		catalog.clear();
		//fichier.clear();
		while ((dirp = readdir(rep)) != NULL) {
			std::string name = dirp->d_name;

			std::size_t found = name.find(".gif", name.length() - 5);
			if (found != std::string::npos) {
				catalog.push_back(name);
				//DEBUG("we got gif:%s",name.c_str());
			}
		}
	}
	void loadRandomGif(int index = 0) {
		std::string dir = rack::string::directory(paths[index]);
		getContainingDirectory();
		if (catalog.size()) {
			int randomIndex = floor(random::uniform() * catalog.size());
			setPath(dir + "/" + catalog[randomIndex]);
		}
		else {
			DEBUG("no gifs in this directory");
		}
	}


	void setPath(std::string path, int index = 0) {
		//if (paths.size() <= index) {
		//paths.push_back(path);
		//}
		//else {
		numFrames = 0;
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
	void setSyncTime(float syncDuration) {
		if (params[CONSTANT_FRAME_DELAY].getValue() == 0) {
			speedFactor = totalGifDuration / syncDuration;
		}
		else {
			speedFactor = numFrames * defaultFrameDelayCentiseconds / syncDuration / 100;
		}

	}
	void setFrameDelay(float frameDelaySeconds) {
		float speedKnob = abs(params[ANIMATION_SPEED].getValue());
		float appliedSpeedDivisor = 1;
		float base = frameDelaySeconds;

		if (expanderConnected && clockConnected && (clockMode == 0)) {
			appliedSpeedDivisor = speedFactor;
		}
		else {
			appliedSpeedDivisor = speedKnob;
		}


		if (params[CONSTANT_FRAME_DELAY].getValue()) {
			frameDelay = defaultFrameDelayCentiseconds / appliedSpeedDivisor / 100;
		}
		else {
			frameDelay = base / appliedSpeedDivisor;
		}

	}
	void setFrameDelays(std::vector<float> frameDelaysSeconds) {
		frameDelays = frameDelaysSeconds;
		setFrameMap();
		setFrameShuffle();
		ready = true;
	}
	void setFrameMap() {
		frameMapForScan.resize(0);

		//  tokenStack.insert(tokenStack.end(), terminalStack.begin(), terminalStack.end());

		for (unsigned int i = 0; i < frameDelays.size(); i++) {
			int currentCentiseconds = (int) (frameDelays[i] * 100);
			for (int j = 0; j < currentCentiseconds; j++) {
				frameMapForScan.push_back(i);
			}
		}
	}
	void setFrameShuffle() {
		shuffledFrames.resize(0);
		for(int i=0; i < numFrames; i++) {
			shuffledFrames.push_back(i);
		}
		auto rng = std::default_random_engine {};
		std::shuffle(std::begin(shuffledFrames), std::end(shuffledFrames), rng);

	}
	void setTotalGifDuration(float totalDuration) {
		totalGifDuration = totalDuration;
	}
	std::string getPath() {
		//return numFrames > 0 ? paths[currentFrame] : "";
		return paths[0];
	}
	void tickAnimation() {
		if (numFrames > 1) {
			int animationMode = params[ANIMATION_MODE].getValue();
			if (params[ANIMATION_SPEED].getValue() >= 0 ) {
				if (animationMode == 0 || animationMode == 3) {
					nextFrame();
				} else if (animationMode == 1)  {
					prevFrame();
				}
				else if (animationMode == 2) {
					if (pingPongDirection == 1) {

						nextFrame();
					} else {
						prevFrame();
					}
				}
				else if (animationMode == 4 ) {
					goToRandomFrame();
				}
				tick = !tick;

			}
			else {
				prevFrame();
			}
			if (animationMode == 2) {
				//DEBUG("PRE ping current:%i,direction:%i", currentFrame, pingPongDirection);
				if (pingPongDirection == 1) {
					if (currentFrame == numFrames - 1) {
						pingPongDirection = -1;
					}
				}
				else {
					if (currentFrame == 0) {
						pingPongDirection = 1;
					}
				}
			}

			if (currentFrame == 0) {
				int eb = params[END_BEHAVIOR].getValue();

				if (eb == 3 ) {
					loadRandomGif();
				}
			}
		}
		//DEBUG("current:%i, samplesDelay:%i", currentFrame, samplesDelay);
	}
	void setCurrentFrameDelayFromTable() {
		if (ready) {
			setFrameDelay(frameDelays[mappedFrame]);
		}
	}
	void nextFrame() {
		goToFrame(currentFrame + 1);

	}
	void prevFrame() {
		goToFrame(currentFrame - 1);
	}
	void goToFrame(int frameNum) {
		if (numFrames && ready) {
			sampleCounter = 0;
			currentFrame = frameNum;
			mappedFrame = (currentFrame  + mapBlankFrameOffset(zeroOffset, numFrames)) % numFrames;
			if(params[ANIMATION_MODE].getValue() == 3) {
				mappedFrame = shuffledFrames[mappedFrame];
			}
			
			currentFrame += numFrames;
			currentFrame %= numFrames;
			setCurrentFrameDelayFromTable();
			DEBUG("currentFrame:%i, mappedFrame:%i, scrubFrame:%i",currentFrame,mappedFrame,scrubFrame);
		}
		else {
			DEBUG("no frames lol");
		}
	}
	void goToRandomFrame() {
		int randFrame = (int) std::floor(random::uniform() * numFrames);
		goToFrame(randFrame);
	}
	void scanToPosition(float scanVoltage) {
		/* 0v = frame 0
			10.0v = frame n-1

		*/
		if (ready) {
			int frameNum;
			float vu = (scanVoltage) / 10.01f;
			if (params[CONSTANT_FRAME_DELAY].getValue()) {
				frameNum = floor((vu) * numFrames);
			}
			else {
				//frameMapForScan
				frameNum = frameMapForScan[floor(vu * frameMapForScan.size())];
			}
			goToFrame(frameNum);
		}
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
struct ssmi : MenuItem
{
	//ComputerscareRolyPouter *pouter;
	int mySetVal = 1;
	ParamQuantity *myParamQuantity;
	ssmi(int i, ParamQuantity* pq)
	{
		mySetVal = i;
		myParamQuantity = pq;
	}

	void onAction(const event::Action &e) override
	{
		myParamQuantity->setValue(mySetVal);
		//pouter->setAll(mySetVal);
	}
	void step() override {
		rightText = myParamQuantity->getValue() == mySetVal ? "✔" : "";
		MenuItem::step();
	}
};
struct ParamSelectMenu : MenuItem {
	ParamQuantity* param;
	std::vector<std::string> options;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (unsigned int i = 0; i < options.size(); i++) {
			ssmi *menuItem = new ssmi(i, param);
			menuItem->text = options[i];
			//menuItem->pouter = pouter;
			menu->addChild(menuItem);
		}
		return menu;
	}
	void step() override {
		rightText = "(" + options[param->getValue()] + ") " + RIGHT_ARROW;
		MenuItem::step();
	}
};
struct KeyboardControlChildMenu : MenuItem {
	ComputerscareBlank *blank;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "A,S,D,F: Translate image position"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Z,X: Zoom in/out"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "J,L: Previous / next frame"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "K: Go to first frame"));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "I: Go to random frame"));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "O: Load random image from same directory"));

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "P: Toggle animation on/off"));


		InvertYMenuItem *invertYMenuItem = new InvertYMenuItem();
		invertYMenuItem->text = "Invert Y-Axis";
		invertYMenuItem->blank = blank;
		menu->addChild(invertYMenuItem);

		return menu;
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
				blankModule->setFrameDelays(gifBuddy.getAllFrameDelaysSeconds());
				blankModule->setTotalGifDuration(gifBuddy.getTotalGifDuration());
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
			//if (blankModule->currentFrame != currentFrame) {
			gifBuddy.displayGifFrame(args.vg, currentFrame);
			//}
		}
	}
	void step() override {
		if (blankModule && blankModule->loadedJSON) {
			if (blankModule->mappedFrame != currentFrame) {
				currentFrame = blankModule->mappedFrame;
			}
			if (blankModule->scrubbing) {
				currentFrame = blankModule->scrubFrame;
			}
		}
		TransparentWidget::step();
	}
};

struct GiantFrameDisplay : TransparentWidget {
	ComputerscareBlank *module;
	SmallLetterDisplay *description;
	SmallLetterDisplay *frameDisplay;
	GiantFrameDisplay() {
		box.size = Vec(200,380);

		description = new SmallLetterDisplay();
		description->value = "Frame Zero, for EOC output and reset input";
		description->fontSize = 24;
		description->breakRowWidth = 200.f;
		description->box.pos.y = box.size.y - 130;


		frameDisplay = new SmallLetterDisplay();
		frameDisplay->fontSize = 90;
		frameDisplay->box.size = Vec(300, 120);
		frameDisplay->textOffset = Vec(0, 50);
		frameDisplay->box.pos.y = box.size.y - 200;
		frameDisplay->breakRowWidth = 200.f;
		frameDisplay->baseColor = nvgRGBAf(0.8, 0.8, 0.8, 0.7);


		
		addChild(frameDisplay);
		addChild(description);
		//TransparentWidget();
	}
	void step() {
		if (module) {
			visible = module->scrubbing;
			frameDisplay->value = string::f("%i / %i", module->scrubFrame + 1, module->numFrames);
		}
		else {
			visible = false;
		}
		TransparentWidget::step();
	}
};

struct ComputerscareBlankWidget : MenuParamModuleWidget {
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

		frameDisplay = new GiantFrameDisplay();
		frameDisplay->module = blankModule;
		addChild(frameDisplay);

	}

	void appendContextMenu(Menu* menu) override {
		ComputerscareBlank* blank = dynamic_cast<ComputerscareBlank*>(this->blankModule);


		modeMenu = new ParamSelectMenu();
		modeMenu->text = "Animation Mode";
		modeMenu->rightText = RIGHT_ARROW;
		modeMenu->param = blankModule->paramQuantities[ComputerscareBlank::ANIMATION_MODE];
		modeMenu->options = blankModule->animationModeDescriptions;

		endMenu = new ParamSelectMenu();
		endMenu->text = "Animation End Behavior";
		endMenu->rightText = RIGHT_ARROW;
		endMenu->param = blankModule->paramQuantities[ComputerscareBlank::END_BEHAVIOR];
		endMenu->options = blankModule->endBehaviorDescriptions;
		
		menu->addChild(new MenuEntry);


		menu->addChild(modeMenu);
		menu->addChild(endMenu);

		KeyboardControlChildMenu *kbMenu = new KeyboardControlChildMenu();
		kbMenu->text = "Keyboard Controls";
		kbMenu->rightText = RIGHT_ARROW;
		kbMenu->blank = blank;
		menu->addChild(kbMenu);


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

		/*SmoothKnob* speedParam = new SmoothKnob();
		speedParam->paramQuantity = blankModule->paramQuantities[ComputerscareBlank::ANIMATION_SPEED];

		MenuEntry* LabeledKnob = new MenuEntry();
		MenuLabel* johnLabel = construct<MenuLabel>(&MenuLabel::text, "Animation Speed");
		johnLabel->box.pos = Vec(speedParam->box.size.x,0);

		LabeledKnob->addChild(johnLabel);
		LabeledKnob->addChild(speedParam);

		//menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Animation Speed"));
		menu->addChild(LabeledKnob);*/

		MenuParam* animEnabled = new MenuParam(blankModule->paramQuantities[ComputerscareBlank::ANIMATION_ENABLED], 0);
		menu->addChild(animEnabled);

		MenuParam* speedParam = new MenuParam(blankModule->paramQuantities[ComputerscareBlank::ANIMATION_SPEED], 2);
		menu->addChild(speedParam);

		MenuParam* mp = new MenuParam(blankModule->paramQuantities[ComputerscareBlank::CONSTANT_FRAME_DELAY], 0);
		menu->addChild(mp);






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
			case GLFW_KEY_O: {
				blankModule->loadRandomGif();
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
	GiantFrameDisplay* frameDisplay;
	ParamSelectMenu *modeMenu;
	ParamSelectMenu *endMenu;
};



Model *modelComputerscareBlank = createModel<ComputerscareBlank, ComputerscareBlankWidget>("computerscare-blank");