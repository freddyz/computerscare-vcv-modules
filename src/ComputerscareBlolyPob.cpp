#include <string.h>
#include "Computerscare.hpp"
#include "dtpulse.hpp"


static const int BUFFER_SIZE = 512;

static const int displayWidth = 20 * 15;

struct BlolyPob : Module {
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

	float time = 0.f;

	bool figureEmitsLight = true;


	BlolyPob() {
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

		float deltaPhase = 2.f * args.sampleTime;
		time += deltaPhase;
		if (time > 2 * M_PI) {
			time -= 2 * M_PI;
		}

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
						//bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(std::min(cmap[c], this->channelsX)) * trimVal + offsetVal + 99 + (1071 * cmap[c]) % 19;
						bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(cmap[c]);
					}
				}
				else {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = 0;//inputs[X_INPUT].getVoltage(cmap[c]);
						//bufferX[c][bufferIndex] = offsetVal + 99 + (1071 * cmap[c]) % 19;
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


struct BlolyPobDisplay : TransparentWidget {
	BlolyPob *module;

	BlolyPobDisplay() {
	}

	float rt(float angle, float time, float relPhase) {
		float rtx = 30 * sin(3 * angle + time );
		rtx += 30 * sin(4 * angle + time + relPhase);
		return rtx;
	}

	void drawBlob(const DrawArgs &args, float A, float B, float C, float D, float E, float F, float G, float H, float I, float J, float K, float L, float M, float N, float O, float P) {
		int numPhases = 3;
		for (int i = 0; i < numPhases; i++) {
			float phase = ((float)i / numPhases) * 6.28;
			drawOne(args, phase + module->time, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P);
		}
	}

	void drawOne(const DrawArgs &args, float phase, float A, float B, float C, float D, float E, float F, float G, float H, float I, float J, float K, float L, float M, float N, float O, float P) {

		nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_GREEN);

		nvgLineJoin(args.vg, NVG_ROUND);

		float h = 0.5 + 0.25 * sin(C / 2) + 0.25 * sin(K / 3);	//face hue
		float s = 0.5 + 0.32 * sin(B / 3 - 33.21 - D / 2);	//face saturation
		float l = 0.5 + 0.35 * sin(E / 2);	//face lightness

		NVGcolor faceColor = nvgHSLA(h, s, l, 0xff);

		nvgFillColor(args.vg, faceColor);
		nvgStrokeWidth(args.vg, 1.2);

		float size = 1 + sin(O - 29) / 4;

		float oX = displayWidth / 2;
		float oY = 380 / 2;

		float xx;
		float yy;

		float rad = 100;
		int NN = 100;

		nvgBeginPath(args.vg);


		for (int i = 0; i < NN; i++) {
			float arg = 6.283 * i / NN;
			float rtx = rt(arg + A, phase, B);
			float rty = rtx - C;
			xx = oX + (rad + rtx + G) * cos(arg + D);
			yy = oY + (rad + rty) * sin(arg - F);
			if (i == 0) {
				nvgMoveTo(args.vg, xx, yy);
			} else {
				nvgLineTo(args.vg, xx, yy);
			}
		}
		nvgClosePath(args.vg);
		nvgStroke(args.vg);
		//nvgFill(args.vg);
		//nvgResetScissor(args.vg);
	}

	void draw(const DrawArgs &args) override {
		if (!module) {
			drawBlob(args, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10);
		}
		else {
			if (!module->figureEmitsLight) {
				drawBlob(args, module->bufferX[0][0], module->bufferX[1][0], module->bufferX[2][0], module->bufferX[3][0], module->bufferX[4][0], module->bufferX[5][0], module->bufferX[6][0], module->bufferX[7][0], module->bufferX[8][0], module->bufferX[9][0], module->bufferX[10][0], module->bufferX[11][0], module->bufferX[12][0], module->bufferX[13][0], module->bufferX[14][0], module->bufferX[15][0]);
			}
		}
	}
	void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			if (module->figureEmitsLight) {
				drawBlob(args, module->bufferX[0][0], module->bufferX[1][0], module->bufferX[2][0], module->bufferX[3][0], module->bufferX[4][0], module->bufferX[5][0], module->bufferX[6][0], module->bufferX[7][0], module->bufferX[8][0], module->bufferX[9][0], module->bufferX[10][0], module->bufferX[11][0], module->bufferX[12][0], module->bufferX[13][0], module->bufferX[14][0], module->bufferX[15][0]);
			}
		}
		Widget::drawLayer(args, layer);
	}
};


struct BlolyPobWidget : ModuleWidget {
	BlolyPobWidget(BlolyPob *module) {
		setModule(module);

		box.size = Vec(displayWidth, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			//panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareBlolyPobPanel.svg")));
			addChild(panel);

		}

		{
			BlolyPobDisplay *display = new BlolyPobDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addInput(createInput<PointingUpPentagonPort>(Vec(1, 353), module, BlolyPob::X_INPUT));
		addParam(createParam<SmallKnob>(Vec(31, 357), module, BlolyPob::TRIM));
		addParam(createParam<SmoothKnob>(Vec(51, 353), module, BlolyPob::OFFSET));

		addParam(createParam<ScrambleKnob>(Vec(81, 357), module, BlolyPob::SCRAMBLE));
	}

	void appendContextMenu(Menu* menu) override {
		BlolyPob* module = dynamic_cast<BlolyPob*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Stick Figure Emits Light", "", &module->figureEmitsLight));
	}
};


Model *modelComputerscareBlolyPob = createModel<BlolyPob, BlolyPobWidget>("computerscare-bloly-pob");
