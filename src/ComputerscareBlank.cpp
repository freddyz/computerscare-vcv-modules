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
#include <settings.hpp>
#include <cctype>
#include <algorithm>

#define FONT_SIZE 13


struct ComputerscareBlank : ComputerscareMenuParamModule {
	bool loading = true;
	bool loadedJSON = false;
	bool ready = false;
	std::string path;
	std::string parentDirectory;

	std::vector<std::string> paths;
	std::vector<std::string> catalog;
	int fileIndexInCatalog;
	unsigned int numFilesInCatalog = 0;

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
	std::vector<float> gifDurationsForPingPong;

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
	float lastShuffle = 2.f;

	float lastZoom = -100;
	int zoomCheckInterval = 5000;
	int zoomCheckCounter = 0;
	bool pauseAnimation = true;

	/*
		uninitialized: 0
		gif: 1
		not gif: 2
		error:3
	*/

	bool expanderConnected = false;

	int clockMode = CLOCK_MODE_SYNC;
	bool clockConnected = false;
	bool resetConnected = false;
	bool nextFileInputConnected = false;

	std::vector<std::string> animationModeDescriptions;
	std::vector<std::string> endBehaviorDescriptions;
	std::vector<std::string> nextFileDescriptions;


	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger resetButtonTrigger;

	dsp::SchmittTrigger nextFileTrigger;
	dsp::SchmittTrigger nextFileButtonTrigger;

	dsp::Timer syncTimer;
	dsp::Timer slideshowTimer;


	ComputerscareSVGPanel* panelRef;

	float leftMessages[2][11] = {};

	enum ClockModes {
		CLOCK_MODE_SYNC,
		CLOCK_MODE_SCAN,
		CLOCK_MODE_FRAME
	};

	enum ParamIds {
		ANIMATION_SPEED,
		ANIMATION_ENABLED,
		CONSTANT_FRAME_DELAY,
		ANIMATION_MODE,
		END_BEHAVIOR,
		SHUFFLE_SEED,
		NEXT_FILE_BEHAVIOR,
		SLIDESHOW_ACTIVE,
		SLIDESHOW_TIME,
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

		nextFileDescriptions.push_back("Load Next (Alphabetical) File in Directory");
		nextFileDescriptions.push_back("Load Previous (Alphabetical) File in Directory");
		nextFileDescriptions.push_back("Load Random File from Directory");

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configMenuParam(ANIMATION_SPEED, -1.f, 1.f, 0.f, "Animation Speed", 2, "x", 20.f);
		configParam(ANIMATION_ENABLED, 0.f, 1.f, 1.f, "Animation Enabled");
		configParam(CONSTANT_FRAME_DELAY, 0.f, 1.f, 0.f, "Constant Frame Delay");
		configMenuParam(ANIMATION_MODE, 0.f, "Animation Mode", animationModeDescriptions);
		configMenuParam(NEXT_FILE_BEHAVIOR, 0.f, "Next File Trigger / Button Behavior", nextFileDescriptions);
		configMenuParam(SHUFFLE_SEED, 0.f, 1.f, 0.5f, "Shuffle Seed", 2);

		configParam(SLIDESHOW_ACTIVE, 0.f, 1.f, 0.f, "Slideshow Active");
		configMenuParam(SLIDESHOW_TIME, 0.f, 1.f, 0.f, "Slideshow Time", 2, " s",  400.f, 3.f);

		paths.push_back("empty");

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

	}
	void process(const ProcessArgs &args) override {
		if (imageStatus == 1) {
			sampleCounter++;
			zoomCheckCounter++;
			if (zoomCheckCounter > zoomCheckInterval) {
				if (settings::zoom != lastZoom) {
					pauseAnimation = true;
				}
				else {
					pauseAnimation = false;
				}
				lastZoom = settings::zoom;
				zoomCheckCounter = 0;
			}
		}

		if (params[SLIDESHOW_ACTIVE].getValue()) {
			//float dTime = exp(5 * params[SLIDESHOW_TIME].getValue());
			float dTime =  3 * std::pow(400.f , params[SLIDESHOW_TIME].getValue());
			if (slideshowTimer.process(args.sampleTime) > dTime) {
				checkAndPerformEndAction(true);
				slideshowTimer.reset();
			}

		}

		samplesDelay = frameDelay * args.sampleRate;

		bool shouldAdvanceAnimation = false;
		bool clockTriggered = false;

		if (ready && leftExpander.module && leftExpander.module->model == modelComputerscareBlankExpander) {
			expanderConnected = true;
			// me
			float *messageToSendToExpander = (float*) leftExpander.module->rightExpander.producerMessage;

			float *messageFromExpander = (float*) leftExpander.consumerMessage;

			clockMode = messageFromExpander[0];
			clockConnected = messageFromExpander[1];
			resetConnected = messageFromExpander[3];
			nextFileInputConnected = messageFromExpander[5];

			zeroOffset = messageFromExpander[7];

			scrubbing = messageFromExpander[8];



			updateScrubFrame();

			if (clockConnected) {
				clockTriggered = clockTrigger.process(messageFromExpander[2]);
				if (clockMode == CLOCK_MODE_SYNC) {
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

				else if (clockMode == CLOCK_MODE_SCAN) {
					//scan
					float scanPosition = messageFromExpander[2];
					scanToPosition(scanPosition);
				}
				else if (clockMode == CLOCK_MODE_FRAME) {
					//frame advance
					shouldAdvanceAnimation = clockTriggered;
				}
			}

			if (nextFileInputConnected) {
				if (nextFileTrigger.process(messageFromExpander[6])) {
					checkAndPerformEndAction(true);
				}
			}

			if (nextFileButtonTrigger.process(messageFromExpander[10])) {
				checkAndPerformEndAction(true);
			}

			if (resetConnected) {
				if (resetTrigger.process(messageFromExpander[4])) {
					goToFrame(0);
				}
			}
			if (resetButtonTrigger.process(messageFromExpander[9])) {
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

		if (expanderConnected && clockConnected && (clockMode == CLOCK_MODE_FRAME)) {
			//no-op for frame mode for some reason?
		}
		else {
			if (sampleCounter > samplesDelay) {
				sampleCounter = 0;
				shouldAdvanceAnimation = true;
			}
		}
		if (numFrames > 1) {
			if (params[ANIMATION_ENABLED].getValue() && shouldAdvanceAnimation) {
				tickAnimation();
				//checkAndPerformEndAction();
			}
		}
		else {
			if ((clockTriggered && (clockConnected && clockMode == CLOCK_MODE_SYNC)) || numFrames > 1) {
				if (currentFrame == 0) {
					//checkAndPerformEndAction();
				}
			}
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

		std::string path = pathC;
		std::free(pathC);

		setPath(path);
	}
	void setContainingDirectory(int index = 0) {

		std::string dir = rack::string::directory(paths[index]);
		std::string currentImageFullpath;
		parentDirectory = dir;
		int imageIndex = 0;;

		struct dirent* dirp = NULL;
		DIR* rep = NULL;

		rep = opendir(dir.c_str());

		catalog.clear();
		//fichier.clear();
		while ((dirp = readdir(rep)) != NULL) {
			std::string name = dirp->d_name;

			std::size_t found = name.find(".gif", name.length() - 5);
			if (found == std::string::npos) found = name.find(".GIF", name.length() - 5);
			if (found == std::string::npos) found = name.find(".png", name.length() - 5);
			if (found == std::string::npos) found = name.find(".PNG", name.length() - 5);
			if (found == std::string::npos) found = name.find(".jpg", name.length() - 5);
			if (found == std::string::npos) found = name.find(".JPG", name.length() - 5);
			if (found == std::string::npos) found = name.find(".jpeg", name.length() - 5);
			if (found == std::string::npos) found = name.find(".JPEG", name.length() - 5);
			if (found == std::string::npos) found = name.find(".bmp", name.length() - 5);
			if (found == std::string::npos) found = name.find(".BMP", name.length() - 5);
			if (found != std::string::npos) {
				currentImageFullpath = parentDirectory + "/" + name;
				catalog.push_back(currentImageFullpath);
				if (currentImageFullpath == paths[index]) {
					fileIndexInCatalog = imageIndex;
				}
				//DEBUG("we got gif:%s", name.c_str());
				imageIndex++;
			}
		}
		numFilesInCatalog = catalog.size();
	}

	void loadRandomGif() {
		fileIndexInCatalog = floor(random::uniform() * numFilesInCatalog);
		loadNewFileByIndex();
	}

	void loadNewFileByIndex() {
		if (numFilesInCatalog > 0) {
			setPath(catalog[fileIndexInCatalog]);
		}
	}
	void nextFileInCatalog() {
		if (numFilesInCatalog > 0) {
			fileIndexInCatalog++;
			fileIndexInCatalog %= numFilesInCatalog;
			loadNewFileByIndex();
		}
	}
	void prevFileInCatalog() {
		if (numFilesInCatalog > 0) {
			fileIndexInCatalog--;
			fileIndexInCatalog += numFilesInCatalog;
			fileIndexInCatalog %= numFilesInCatalog;
			loadNewFileByIndex();
		}
	}
	void goToFileInCatelog(int index) {
		fileIndexInCatalog = index;
		fileIndexInCatalog %= numFilesInCatalog;
		loadNewFileByIndex();
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
		numFrames = frameCount;
	}
	void setImageStatus(int status) {
		imageStatus = status;
	}
	void setSyncTime(float syncDuration) {
		bool constantFrameDelay = params[CONSTANT_FRAME_DELAY].getValue() == 1;
		if (params[ANIMATION_MODE].getValue() == 2) { //pingpong
			if (numFrames > 1) {
				float totalDurationForCurrentZeroOffset = gifDurationsForPingPong[mapBlankFrameOffset(zeroOffset, numFrames)];
				if (constantFrameDelay) {
					speedFactor = (2 * numFrames - 2) * defaultFrameDelayCentiseconds / syncDuration / 100;
				}
				else {
					speedFactor = totalDurationForCurrentZeroOffset / syncDuration;
				}
			}
		}
		else { //all other modes
			if (constantFrameDelay) {
				speedFactor = numFrames * defaultFrameDelayCentiseconds / syncDuration / 100;
			}
			else {
				speedFactor = totalGifDuration / syncDuration;
			}
		}
	}
	void setFrameDelay(float frameDelaySeconds) {
		float speedKnob = std::pow(20.f, params[ANIMATION_SPEED].getValue());
		float appliedSpeedDivisor = 1;
		float base = frameDelaySeconds;

		if (expanderConnected && clockConnected && (clockMode == CLOCK_MODE_SYNC)) {
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

	}
	void setReady(bool v) {
		ready = v;
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
		for (int i = 0; i < numFrames; i++) {
			shuffledFrames.push_back(i);
		}
		unsigned seed = (unsigned) (floor(params[SHUFFLE_SEED].getValue() * 999101));

		std::shuffle(std::begin(shuffledFrames), std::end(shuffledFrames), std::default_random_engine(seed));

	}
	void setTotalGifDuration(float totalDuration) {
		totalGifDuration = totalDuration;
	}
	void setTotalGifDurationIfInPingPongMode(std::vector<float> durs) {
		/* the gif has a different total pingpong duration depending on where the reset frame is
			eg a gif with frame durations:
			1) 100
			2) 5
			3) 5
			4) 5

			if ponging around frame 1 has duration: 100(1)+ 5(2) + 5(3) + 5(4) + 5(3) + 5(2) = 125

			but if ponging around frame 3: 5(3) + 5(4) + 100(1) + 5(2) + 100(1) + 5(4) = 240

		*/
		gifDurationsForPingPong = durs;

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

			float shuf = params[SHUFFLE_SEED].getValue();
			if (shuf != lastShuffle) {
				setFrameShuffle();
			}
			lastShuffle = shuf;
			//DEBUG("current:%i, samplesDelay:%i", currentFrame, samplesDelay);
		}
	}
	void checkAndPerformEndAction(bool forceEndAction = false) {
		if (currentFrame == 0 || forceEndAction) {
			int eb = params[NEXT_FILE_BEHAVIOR].getValue();

			if (eb == 0) {
				nextFileInCatalog();
			}
			else if (eb == 1) {
				prevFileInCatalog();
			}
			else if (eb == 2 ) {
				loadRandomGif();
			}
		}
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
		if (numFrames && ready && frameNum != currentFrame) {
			sampleCounter = 0;
			currentFrame = frameNum;
			mappedFrame = (currentFrame  + mapBlankFrameOffset(zeroOffset, numFrames) + numFrames) % numFrames;
			if (params[ANIMATION_MODE].getValue() == 3) {
				mappedFrame = shuffledFrames[mappedFrame];
			}

			currentFrame += numFrames;
			currentFrame %= numFrames;
			setCurrentFrameDelayFromTable();
			//DEBUG("currentFrame:%i, mappedFrame:%i, scrubFrame:%i", currentFrame, mappedFrame, scrubFrame);
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
		if (ready && numFrames > 1) {
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

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "[  (left square bracket): Load previous image from same directory"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "]  (right square bracket): Load next image from same directory"));
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


	void resetZooms() {
		DEBUG("resetting zooms lol");
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

	void setOffsets() {
	}
	void draw(const DrawArgs &args) override {
		if (blankModule && blankModule->loadedJSON) {
			std::string modulePath = blankModule->getPath();
			if (path != modulePath) {
				DEBUG("path not module path");
				DEBUG("path: %s, modulePath:%s", path.c_str(), modulePath.c_str());
				gifBuddy = AnimatedGifBuddy(args.vg, modulePath.c_str());
				if (gifBuddy.getImageStatus() == 3) {
					std::string badGifPath = asset::plugin(pluginInstance, "res/broken-file.gif");
					gifBuddy = AnimatedGifBuddy(args.vg, badGifPath.c_str());
				}
				img = gifBuddy.getHandle();

				blankModule->setFrameCount(gifBuddy.getFrameCount());
				blankModule->setFrameDelays(gifBuddy.getAllFrameDelaysSeconds());
				blankModule->setTotalGifDuration(gifBuddy.getTotalGifDuration());
				blankModule->setTotalGifDurationIfInPingPongMode(gifBuddy.getPingPongGifDuration());
				blankModule->setFrameDelay(gifBuddy.getSecondsDelay(0));
				blankModule->setImageStatus(gifBuddy.getImageStatus());
				blankModule->setContainingDirectory();
				blankModule->setReady(true);


				nvgImageSize(args.vg, img, &imgWidth, &imgHeight);
				imgRatio = ((float)imgWidth / (float)imgHeight);

				//path==empty means that it is 1st load of the module from JSON
				//not empty -> anything then reset

				if (modulePath != "empty") {
					DEBUG("path:%s", modulePath.c_str());
				}
				if (path != "empty") {
					resetZooms();
				}
				path = modulePath;


			}

			if (blankModule->imageFitEnum != lastEnum && lastEnum != -1) {
				lastEnum = blankModule->imageFitEnum;
				resetZooms();
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
			if (!blankModule->pauseAnimation) {
				gifBuddy.displayGifFrame(args.vg, currentFrame);
			}
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
		box.size = Vec(200, 380);

		description = new SmallLetterDisplay();
		description->value = "Frame Zero, for EOC output, reset input, and sync mode";
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

		TransparentWidget();
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
	void draw(const DrawArgs &args) {
		if (module) {
			TransparentWidget::draw(args);
		}
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
		endMenu->text = "Next File Trigger / Button Behavior";
		endMenu->rightText = RIGHT_ARROW;
		endMenu->param = blankModule->paramQuantities[ComputerscareBlank::NEXT_FILE_BEHAVIOR];
		endMenu->options = blankModule->nextFileDescriptions;

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

		MenuParam* animEnabled = new MenuParam(blank->paramQuantities[ComputerscareBlank::ANIMATION_ENABLED], 0);
		menu->addChild(animEnabled);

		MenuParam* speedParam = new MenuParam(blank->paramQuantities[ComputerscareBlank::ANIMATION_SPEED], 2);
		menu->addChild(speedParam);

		MenuParam* mp = new MenuParam(blank->paramQuantities[ComputerscareBlank::CONSTANT_FRAME_DELAY], 0);
		menu->addChild(mp);

		MenuParam* shuffleParam = new MenuParam(blank->paramQuantities[ComputerscareBlank::SHUFFLE_SEED], 2);
		menu->addChild(shuffleParam);


		MenuParam* slideshowEnabled = new MenuParam(blank->paramQuantities[ComputerscareBlank::SLIDESHOW_ACTIVE], 0);
		menu->addChild(slideshowEnabled);

		MenuParam* slideshowSpeedParam = new MenuParam(blank->paramQuantities[ComputerscareBlank::SLIDESHOW_TIME], 2);
		menu->addChild(slideshowSpeedParam);

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
					pngDisplay->resetZooms();
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
			}
		}
		if (e.action == GLFW_RELEASE && !e.mods ) {
			switch (e.key) {
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
			case GLFW_KEY_LEFT_BRACKET: {
				blankModule->prevFileInCatalog();
				e.consume(this);
			} break;
			case GLFW_KEY_RIGHT_BRACKET: {
				blankModule->nextFileInCatalog();
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
	GiantFrameDisplay *frameDisplay;
	ParamSelectMenu *modeMenu;
	ParamSelectMenu *endMenu;
};



Model *modelComputerscareBlank = createModel<ComputerscareBlank, ComputerscareBlankWidget>("computerscare-blank");