#include <string.h>
#include "Computerscare.hpp"
#include "dtpulse.hpp"


static const int BUFFER_SIZE = 512;


struct StolyFickPigure : Module {
	enum ParamIds {
		TIME_PARAM,
		TRIM,
		OFFSET,
		SCRAMBLE,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		SCRAMBLE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float bufferX[16][BUFFER_SIZE] = {};
	int cmap[16];
	int channelsX = 0;
	int bufferIndex = 0;
	int frameIndex = 0;
	int cnt = 0;
	float lastScramble = 0;

	int A = 31;
	int B = 32;
	int C = 29;
	int D = 2;

	bool figureEmitsLight = true;


	StolyFickPigure() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float timeBase = (float) BUFFER_SIZE / 6;

		for (int i = 0; i < 16; i++) {
			cmap[i] = i;
		}

		configParam(TIME_PARAM, 6.f, 16.f, 14.f, "Time", " ms/div", 1 / 2.f, 1000 * timeBase);

		configParam(TRIM, -2.f, 2.f, 0.2f, "Input Trim");
		configParam(OFFSET, -5.f, 5.f, 0.f, "Input Offset", " Volts");

		configParam(SCRAMBLE, -10.f, 10.f, 0.f, "Scrambling");

		configInput(X_INPUT, "Main");

	}

	void onReset() override {
		//std::memset(bufferX, 0, sizeof(bufferX));
	}
	void updateScramble(float v) {
		for (int i = 0; i < 16; i++) {
			cmap[i] = (i * A + B + (int)std::floor(v * 1010.1)) % 16;
		}
	}
	void checkScramble() {
		float xx = params[SCRAMBLE].getValue();
		if (lastScramble != xx) {
			lastScramble = xx;
			updateScramble(xx);
		}
	}
	void process(const ProcessArgs &args) override {
		// Modes
		// Compute time
		float deltaTime = std::pow(2.f, -params[TIME_PARAM].getValue());

		int frameCount = (int) std::ceil(deltaTime * args.sampleRate);

		// Set channels
		int channelsX = inputs[X_INPUT].getChannels();
		if (channelsX != this->channelsX) {
			std::memset(bufferX, 0, sizeof(bufferX));
			this->channelsX = channelsX;
		}

		if (cnt > 4101) {

			checkScramble();
			cnt = 0;
		}
		cnt++;
		// Add frame to buffer
		if (bufferIndex < BUFFER_SIZE) {
			if (++frameIndex > frameCount) {
				frameIndex = 0;
				float trimVal = params[TRIM].getValue();
				float offsetVal = params[OFFSET].getValue();

				if (inputs[X_INPUT].isConnected()) {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(std::min(cmap[c], this->channelsX)) * trimVal + offsetVal + 99 + (1071 * cmap[c]) % 19;
						//bufferX[c][bufferIndex]=inputs[X_INPUT].getVoltage(cmap[c])
					}
				}
				else {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = offsetVal + 99 + (1071 * cmap[c]) % 19;
					}
				}

				bufferIndex++;
			}
		}

		// Don't wait for trigger if still filling buffer
		if (bufferIndex < BUFFER_SIZE) {
			return;
		}

		// Trigger immediately if external but nothing plugged in, or in Lissajous mode
		if (true) {
			trigger();
			return;
		}

		frameIndex++;


	}

	void trigger() {
		bufferIndex = 0;
		frameIndex = 0;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "figureEmitsLight", json_boolean(figureEmitsLight));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* figureEmitsLightJ = json_object_get(rootJ, "figureEmitsLight");
		if (figureEmitsLightJ)
			figureEmitsLight = json_boolean_value(figureEmitsLightJ);
	}
};


struct StolyFickPigureDisplay : TransparentWidget {
	StolyFickPigure *module;

	StolyFickPigureDisplay() {
	}

	void drawStickFigure(const DrawArgs &args, float A, float B, float C, float D, float E, float F, float G, float H, float I, float J, float K, float L, float M, float N, float O, float P) {

		nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_GREEN);

		nvgLineJoin(args.vg, NVG_ROUND);

		float h = 0.5 + 0.25 * sin(C / 2) + 0.25 * sin(K / 3);	//face hue
		float s = 0.5 + 0.32 * sin(B / 3 - 33.21 - D / 2);	//face saturation
		float l = 0.5 + 0.35 * sin(E / 2);	//face lightness

		NVGcolor faceColor = nvgHSLA(h, s, l, 0xff);

		nvgFillColor(args.vg, faceColor);
		nvgStrokeWidth(args.vg, 3.2);

		float size = 1 + sin(O - 29) / 4;

		//crotch
		float cx = 62 * (1 + (sin(E + F) - sin(P + O / 2 + 50)) / 40000);
		float cy = 210 * (1 + (sin(A + G - 12) - sin(P + H / 2)) / 11000);

		//thigh spread, length, direction
		float thighSpread = (2 + sin(J + I + K) - sin(A - N / 2)) / 4;
		float thighLength = 50 * (1 + (sin(C - 100 + F + K * 2) + sin(C + L - 10)) / 6);
		float thighDirection = (sin(J + O - 211) - sin(P * 2 + I) - sin(B + K)) / 2;


		//ankle spread,length,direction
		float ankleSpread = (2 + sin(O - B) / 2 + sin(F + 2) / 2 + sin(P - E - D + 19.2)) / 13;
		float ankleLength = thighLength * (1 + (sin(F + A + J - K / 2 + 9)) / 9);
		float ankleDirection =  3 * M_PI / 2 + (3 + sin(J + M - L - 101) - sin(P - B + 22) - sin(H)) / 8;

		float leftKneeArg = 3 * M_PI / 2 + thighDirection + thighSpread;
		float rightKneeArg = 3 * M_PI / 2 + thighDirection - thighSpread;


		float leftAnkleArg =  ankleDirection + ankleSpread;
		float rightAnkleArg =  ankleDirection - ankleSpread;


		float leftKneeX = cx + thighLength * cos(leftKneeArg);
		float leftKneeY = cy - thighLength * sin(leftKneeArg);

		float leftAnkleX = leftKneeX + ankleLength * cos(leftAnkleArg);
		float leftAnkleY = leftKneeY - ankleLength * sin(leftAnkleArg);

		float rightKneeX = cx + thighLength * cos(rightKneeArg);
		float rightKneeY = cy - thighLength * sin(rightKneeArg);

		float rightAnkleX = rightKneeX + ankleLength * cos(rightAnkleArg);
		float rightAnkleY = rightKneeY - ankleLength * sin(rightAnkleArg);


		nvgBeginPath(args.vg);


		nvgMoveTo(args.vg, leftAnkleX, leftAnkleY);
		nvgLineTo(args.vg, leftKneeX, leftKneeY);
		nvgLineTo(args.vg, cx, cy);
		nvgLineTo(args.vg, rightKneeX, rightKneeY);
		nvgLineTo(args.vg, rightAnkleX, rightAnkleY);

		//nvgClosePath(args.vg);
		nvgStroke(args.vg);


		//torso length,direction
		float torsoLength = thighLength * (1.4 + (sin(A - 12)) / 4);
		float torsoDirection = M_PI / 2 + sin(D) / 2;

		float neckX = cx + torsoLength * cos(torsoDirection);
		float neckY = cy - torsoLength * sin(torsoDirection);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, cx, cy);
		nvgLineTo(args.vg, neckX, neckY);
		nvgStroke(args.vg);

		float armLength = torsoLength * (2 + (sin(N + 14) - sin(P - L - 3)) / 2) / 4;
		float forearmLength = armLength * (1 + (2 + (sin(F + B + 2) - sin(E))) / 300);
		float armDirection = 3 * M_PI / 2 + 0.2 * (sin(C - M));
		float armSpread = sin(B + P - A) + sin(N - J);

		float leftElbowArg = armDirection + armSpread;
		float rightElbowArg = armDirection - armSpread;

		float leftHandArg = sin(E + 22 + A - 4);
		float rightHandArg = sin(F + 22 - B);

		float leftElbowX = neckX + armLength * cos(leftElbowArg);
		float leftElbowY = neckY - armLength * sin(leftElbowArg);

		float leftHandX = leftElbowX + forearmLength * cos(leftHandArg);
		float leftHandY = leftElbowY - forearmLength * sin(leftHandArg);

		float rightElbowX = neckX + armLength * cos(rightElbowArg);
		float rightElbowY = neckY - armLength * sin(rightElbowArg);

		float rightHandX = rightElbowX + forearmLength * cos(rightHandArg);
		float rightHandY = rightElbowY - forearmLength * sin(rightHandArg);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, neckX, neckY);
		nvgLineTo(args.vg, leftElbowX, leftElbowY);
		nvgLineTo(args.vg, leftHandX, leftHandY);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, neckX, neckY);
		nvgLineTo(args.vg, rightElbowX, rightElbowY);
		nvgLineTo(args.vg, rightHandX, rightHandY);
		nvgStroke(args.vg);

		float headHeight = torsoLength * (0.5 + sin(H - E - I - D) / 9 - sin(F + B - C + E) / 7);
		float headWidth = headHeight * (0.6 + sin(I + D - M / 2) / 7 + sin(G / 2 + J - 10) / 6);
		float headAngle = M_PI / 2 + (sin(C + A) / 6 + sin(D + G) / 9);

		float headRotation = sin(C + A) / 2 + sin(M / 2) / 3;

		nvgBeginPath(args.vg);

		nvgTranslate(args.vg, neckX, neckY);
		nvgRotate(args.vg, headRotation);
		nvgEllipse(args.vg, 0, -headHeight, headWidth, headHeight);

		nvgFill(args.vg);
		nvgStroke(args.vg);


		nvgResetScissor(args.vg);
		//nvgRestore(args.vg);
	}

	void draw(const DrawArgs &args) override {
		if (!module) {
			drawStickFigure(args, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10);
		}
		else {
			if (!module->figureEmitsLight) {
				drawStickFigure(args, module->bufferX[0][0], module->bufferX[1][0], module->bufferX[2][0], module->bufferX[3][0], module->bufferX[4][0], module->bufferX[5][0], module->bufferX[6][0], module->bufferX[7][0], module->bufferX[8][0], module->bufferX[9][0], module->bufferX[10][0], module->bufferX[11][0], module->bufferX[12][0], module->bufferX[13][0], module->bufferX[14][0], module->bufferX[15][0]);
			}
		}
	}
	void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			if (module->figureEmitsLight) {
				drawStickFigure(args, module->bufferX[0][0], module->bufferX[1][0], module->bufferX[2][0], module->bufferX[3][0], module->bufferX[4][0], module->bufferX[5][0], module->bufferX[6][0], module->bufferX[7][0], module->bufferX[8][0], module->bufferX[9][0], module->bufferX[10][0], module->bufferX[11][0], module->bufferX[12][0], module->bufferX[13][0], module->bufferX[14][0], module->bufferX[15][0]);
			}
		}
		Widget::drawLayer(args, layer);
	}
};


struct StolyFickPigureWidget : ModuleWidget {
	StolyFickPigureWidget(StolyFickPigure *module) {
		setModule(module);

		box.size = Vec(9 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareStolyFickPigurePanel.svg")));
			addChild(panel);

		}

		{
			StolyFickPigureDisplay *display = new StolyFickPigureDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addInput(createInput<PointingUpPentagonPort>(Vec(1, 353), module, StolyFickPigure::X_INPUT));
		addParam(createParam<SmallKnob>(Vec(31, 357), module, StolyFickPigure::TRIM));
		addParam(createParam<SmoothKnob>(Vec(51, 353), module, StolyFickPigure::OFFSET));

		addParam(createParam<ScrambleKnob>(Vec(81, 357), module, StolyFickPigure::SCRAMBLE));
	}

	void appendContextMenu(Menu* menu) override {
		StolyFickPigure* module = dynamic_cast<StolyFickPigure*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Stick Figure Emits Light", "", &module->figureEmitsLight));
	}
};


Model *modelComputerscareStolyFickPigure = createModel<StolyFickPigure, StolyFickPigureWidget>("computerscare-stoly-fick-pigure");
