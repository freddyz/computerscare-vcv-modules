#include <string.h>
#include "plugin.hpp"
#include "Computerscare.hpp"
#include "dtpulse.hpp"


static const int BUFFER_SIZE = 512;


struct FolyPace : Module {
	enum ParamIds {
		X_SCALE_PARAM,
		X_POS_PARAM,
		Y_SCALE_PARAM,
		Y_POS_PARAM,
		TIME_PARAM,
		LISSAJOUS_PARAM,
		TRIG_PARAM,
		EXTERNAL_PARAM,
		TRIM,
		OFFSET,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		PLOT_LIGHT,
		LISSAJOUS_LIGHT,
		INTERNAL_LIGHT,
		EXTERNAL_LIGHT,
		NUM_LIGHTS
	};

	float bufferX[16][BUFFER_SIZE] = {};
	float bufferY[16][BUFFER_SIZE] = {};
	int channelsX = 0;
	int channelsY = 0;
	int bufferIndex = 0;
	int frameIndex = 0;

	dsp::BooleanTrigger sumTrigger;
	dsp::BooleanTrigger extTrigger;
	bool lissajous = false;
	bool external = false;
	dsp::SchmittTrigger triggers[16];

	FolyPace() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(X_SCALE_PARAM, -2.f, 8.f, 0.f, "X scale", " V/div", 1 / 2.f, 5);
		configParam(X_POS_PARAM, -10.f, 10.f, 0.f, "X position", " V");
		configParam(Y_SCALE_PARAM, -2.f, 8.f, 0.f, "Y scale", " V/div", 1 / 2.f, 5);
		configParam(Y_POS_PARAM, -10.f, 10.f, 0.f, "Y position", " V");
		configParam(TRIM, -2.f, 2.f, 1.f, "Input Trim");
		configParam(OFFSET, -5.f, 5.f, 0.f, "Input Offset", " Volts");
		const float timeBase = (float) BUFFER_SIZE / 6;
		configParam(TIME_PARAM, 6.f, 16.f, 14.f, "Time", " ms/div", 1 / 2.f, 1000 * timeBase);
		configParam(LISSAJOUS_PARAM, 0.f, 1.f, 0.f);
		configParam(TRIG_PARAM, -10.f, 10.f, 0.f, "Trigger position", " V");
		configParam(EXTERNAL_PARAM, 0.f, 1.f, 0.f);
	}

	void onReset() override {
		lissajous = false;
		external = false;
		std::memset(bufferX, 0, sizeof(bufferX));
		std::memset(bufferY, 0, sizeof(bufferY));
	}

	void process(const ProcessArgs &args) override {
		// Modes
		if (sumTrigger.process(params[LISSAJOUS_PARAM].getValue() > 0.f)) {
			lissajous = !lissajous;
		}
		lights[PLOT_LIGHT].setBrightness(!lissajous);
		lights[LISSAJOUS_LIGHT].setBrightness(lissajous);

		if (extTrigger.process(params[EXTERNAL_PARAM].getValue() > 0.f)) {
			external = !external;
		}
		lights[INTERNAL_LIGHT].setBrightness(!external);
		lights[EXTERNAL_LIGHT].setBrightness(external);

		// Compute time
		float deltaTime = std::pow(2.f, -params[TIME_PARAM].getValue());
		int frameCount = (int) std::ceil(deltaTime * args.sampleRate);

		// Set channels
		int channelsX = inputs[X_INPUT].getChannels();
		if (channelsX != this->channelsX) {
			std::memset(bufferX, 0, sizeof(bufferX));
			this->channelsX = channelsX;
		}

		int channelsY = inputs[Y_INPUT].getChannels();
		if (channelsY != this->channelsY) {
			std::memset(bufferY, 0, sizeof(bufferY));
			this->channelsY = channelsY;
		}

		// Add frame to buffer
		if (bufferIndex < BUFFER_SIZE) {
			if (++frameIndex > frameCount) {
				frameIndex = 0;
				float trimVal = params[TRIM].getValue();
				float offsetVal = params[OFFSET].getValue();

				if (inputs[X_INPUT].isConnected()) {
					for (int c = 0; c < channelsX; c++) {
						bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(c) * trimVal + offsetVal + 99 + (1071 * c) % 19;
					}
				}
				else {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = offsetVal + 99 + (1071 * c) % 19;
					}
				}

				for (int c = 0; c < channelsY; c++) {
					bufferY[c][bufferIndex] = inputs[Y_INPUT].getVoltage(c) * trimVal + offsetVal;
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

		// Reset if triggered
		float trigThreshold = params[TRIG_PARAM].getValue();
		Input &trigInput = external ? inputs[TRIG_INPUT] : inputs[X_INPUT];

		// This may be 0
		int trigChannels = trigInput.getChannels();
		for (int c = 0; c < trigChannels; c++) {
			float trigVoltage = trigInput.getVoltage(c);
			if (triggers[c].process(rescale(trigVoltage, trigThreshold, trigThreshold + 0.001f, 0.f, 1.f))) {
				trigger();
				return;
			}
		}

		// Reset if we've been waiting for `holdTime`
		const float holdTime = 0.2f;
		if (frameIndex * args.sampleTime >= holdTime) {
			trigger();
			return;
		}
	}

	void trigger() {
		for (int c = 0; c < 16; c++) {
			triggers[c].reset();
		}
		bufferIndex = 0;
		frameIndex = 0;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "lissajous", json_integer((int) lissajous));
		json_object_set_new(rootJ, "external", json_integer((int) external));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "lissajous");
		if (sumJ)
			lissajous = json_integer_value(sumJ);

		json_t *extJ = json_object_get(rootJ, "external");
		if (extJ)
			external = json_integer_value(extJ);
	}
};


struct FolyPaceDisplay : TransparentWidget {
	FolyPace *module;
	std::shared_ptr<Font> font;


	FolyPaceDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Oswald-Regular.ttf"));
	}

	void drawFace(const DrawArgs &args, float A, float B, float C, float D, float E, float F, float G, float H, float I, float J, float K, float L, float M, float N, float O, float P) {

		//printf("ch1:%f\n",A);
		//std::string byte = getByteString(A);

		//nvgReset(args.vg);

		float sf = 1 + 0.2 * sin(B - C); //scaleFactor
		float ox = 67.5 + sf * 20.33 * sin(D - C / 2);
		float oy = 180 + sf * 30.33 * sin(G - F);

		float h = 0.4 + 0.3 * sin(A / 2) + 0.3 * sin(K / 3);	//face hue
		float s = 0.5 + 0.32 * sin(B / 3 - 33.21 - D / 2);	//face saturation
		float l = 0.5 + 0.45 * sin(C / 2);	//face lightness
		float fx = ox ;	//face y
		float fy = oy;	//face x


		float frx = sf * (70 + 40 * sin(A - B / 2));	// face x radius
		float fry = sf * (150 + 80 * sin(F / 2.2)); //face y radius
		float fr = 0.04 * sin(H - M) + 0.02 * sin(H / 3 + 2.2) + 0.02 * sin(L + P + 8.222); //face rotation
		NVGcolor faceColor = nvgHSLA(h, s, l, 0xff);

		float mpx = ox - 3 * sin(G + I + A);
		float mpy = oy + 20 + sf * (7 + 0.2 * sin(G - I));

		float msx = mpx - 30 * sf + 3 * sin(G);
		float msy = mpy + 5 * sin(L + I + P);


		NVGcolor eyecolor = nvgHSLA(0.5, 0.9, 0.5, 0xff);
		float mex = mpx + 30 * sf + 3 * sin(G + K / 2);
		float mey = mpy + 5 * (1 + sf) * sin(G + 992.2);


		float m1x = mpx - 15 * sf + 3 * sin(A / 3 - M);
		float m1y = mpy - 6 * sf * sin(M) + 14 * sf * cos(E - C + A);

		float m2x = mpx + 15 * sf - 10 * sf * sin(-N) + 14 * sf * sin(E);
		float m2y = mpy - 1 * sf * sin(G - A / 2);

		float epx = ox;
		float epy = oy - 10 * (2 + sf + sin(I - J / 2));

		float eyeSpacing = frx / 2 * (1.8 + 0.5 * sin(200 - J));
		float erlx = frx / 4 * (1 + 0.4 * sin(G));
		float erly = frx / 4 * (1 + 0.4 * sin(H - N + 100));
		float errx = 10 + 3 * sin(M) + 4 * sin(M - 2 - 882.2);
		float erry = 10 + 2 * sin(J) + 4 * sin(J - erly / 20);

		float irisRad = erly * 0.4 * (1.3 + 0.4 * sin(K - D + 1));
		float pupilRad = irisRad * 0.4 * (1 + 0.6 * sin(E));

		float gazeDir = 3.14159 * (1 + sin(B - K));
		float gazeStrength = 4 * (1.3 + 0.5 * sin(D - 1) + 0.6 * sin(1 - L / 2));

		NVGcolor irisColor = nvgHSLA(l, s, h, 0xff);
		NVGcolor pupilColor = nvgHSLA(0.1, 0.1, 0.1, 0xff);

		//nvgSave(args.vg);

		Rect b = Rect(Vec(0, 0), box.size);

		nvgFillColor(args.vg, faceColor);
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);

		nvgRotate(args.vg, fr);

		drawHead(args, fx, fy, frx, fry, faceColor);


		float leftEyebrowHeight = erly * (1.9 + 0.6 * sin(G) + 0.3 * sin(K - B / 2));
		float rightEyebrowHeight = erly * (1.9 + 0.6 * sin(G - 2.2 + N) + 0.2 * sin(L + 33));
		float leftEyebrowAngle = 0.5 * sin(C) + 0.2 * sin(H / 2 - 2);
		float rightEyebrowAngle = 0.7 * sin(F) + 0.3 * sin(2 - I);
		NVGcolor eyebrowColor = nvgHSLA(0.1, 0.2, 0.2, 0xff);
		float eyebrowThickness = 5.f * (1.3 + sin(M - 2));
		float eyebrowLength = frx * 0.3 * (2.2 + sin(G) + 0.4 * sin(B - 2));

		drawEyes(args, epx, epy, eyeSpacing, erlx, erly, 1, irisRad, pupilRad, gazeDir, gazeStrength, irisColor, pupilColor);
		drawEyebrows(args, epx, epy, eyeSpacing, leftEyebrowHeight, rightEyebrowHeight, leftEyebrowAngle, rightEyebrowAngle, eyebrowColor, eyebrowThickness, eyebrowLength);


		float mouthX = ox;
		float mouthY = oy + 0.4 * fry * (1 + 0.4 * sin(C / 2));
		float mouthWidth = frx * 0.6 * (1.2 + 0.6 * sin(C));
		float mouthOpen = fry * 0.06 * (1 + sin(O) - sin(A * 2 + 44));
		float mouthSmile = sin(D) * 2.0;
		float mouthSkew = sin(L) - sin(H);
		float mouthThickness = 5.4 * (sin(H) - sin(M / 2));
		NVGcolor mouthLipColor = nvgHSLA(0.1 * sin(N) - 0.1, 0.6 + 0.3 * sin(M), 0.5 + .4 * sin(I), 0xff);

		nvgGlobalCompositeOperation(args.vg, NVG_ATOP);


		drawMouth(args, mouthX, mouthY, mouthWidth, mouthOpen, mouthSmile, mouthSkew, mouthThickness, mouthLipColor);



		nvgResetScissor(args.vg);
		//nvgRestore(args.vg);
	}

	void drawEyebrows(const DrawArgs &args, float x, float y, float eyeSpacing, float leftEyebrowHeight, float rightEyebrowHeight, float leftEyebrowAngle, float rightEyebrowAngle, NVGcolor eyebrowColor, float eyebrowThickness, float eyebrowLength) {
		nvgBeginPath(args.vg);
		nvgStrokeColor(args.vg, eyebrowColor);
		nvgStrokeWidth(args.vg, eyebrowThickness);
		float cosLeft = cos(leftEyebrowAngle);
		float sinLeft = sin(leftEyebrowAngle);
		float cosRight = cos(rightEyebrowAngle);
		float sinRight = sin(rightEyebrowAngle);

		float r = eyebrowLength / 2;

		nvgMoveTo(args.vg, x - eyeSpacing / 2 - r * cosLeft, y - leftEyebrowHeight - r * sinLeft);
		nvgLineTo(args.vg, x - eyeSpacing / 2 + r * cosLeft, y - leftEyebrowHeight + r * sinLeft);
		//nvgStroke(args.vg);

		nvgMoveTo(args.vg, x + eyeSpacing / 2 - r * cosRight, y - rightEyebrowHeight - r * sinRight);
		nvgLineTo(args.vg, x + eyeSpacing / 2 + r * cosRight, y - rightEyebrowHeight + r * sinRight);
		nvgStroke(args.vg);

		nvgClosePath(args.vg);
	}
	void drawHead(const DrawArgs &args, float x, float y, float width, float height, NVGcolor color) {
		nvgBeginPath(args.vg);


		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 2.f);
		nvgStrokeWidth(args.vg, 4.5f);

		nvgEllipse(args.vg, x, y, width, height);
		nvgClosePath(args.vg);
		nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
		nvgFill(args.vg);
	}
	void drawMouth(const DrawArgs &args, float x, float y, float width, float open, float smile, float skew, float thickness, NVGcolor lipColor) {
		nvgBeginPath(args.vg);
		nvgStrokeWidth(args.vg, thickness);
		nvgStrokeColor(args.vg, lipColor);
		//nvgStrokeWidth(args.vg, 4.5f);

		nvgMoveTo(args.vg, x - width / 2, y - 20.f * smile);

		//top
		nvgBezierTo(args.vg, x - width / 4, y - open * smile, x + width / 4, y - open * smile, x + width / 2, y - 10.f * smile);

		//bottom
		nvgBezierTo(args.vg, x + width / 4, y + smile * open, x - width / 4, y + smile * open, x - width / 2, y - 20.f * smile);
		nvgClosePath(args.vg);
		nvgGlobalCompositeOperation(args.vg, NVG_ATOP);
		nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 0xff));
		nvgStroke(args.vg);
		nvgFill(args.vg);

	}
	void drawEyes(const DrawArgs &args, float x, float y, float spacing, float rx, float ry, float open, float irisRad, float pupilRad, float gazeDir, float gazeStrength, NVGcolor irisColor, NVGcolor pupilColor) {
		float leftX = x - spacing / 2;
		float rightX = x + spacing / 2;
		float pupilOffsetX = gazeStrength * cos(gazeDir);
		float pupilOffsetY = gazeStrength * sin(gazeDir);
		float leftPupilX = leftX + pupilOffsetX;
		float leftPupilY = y + pupilOffsetY;
		float rightPupilX = rightX + pupilOffsetX;
		float rightPupilY = y + pupilOffsetY;

		nvgBeginPath(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		//nvgStrokeWidth(args.vg, 4.5f);

		//whites of eyes
		nvgFillColor(args.vg, nvgRGB(250, 250, 250));


		nvgEllipse(args.vg, leftX, y, rx, ry);
		nvgEllipse(args.vg, rightX, y, rx, ry);

		nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
		nvgFill(args.vg);
		nvgClosePath(args.vg);

		//iris

		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, irisColor);


		nvgCircle(args.vg, leftPupilX, leftPupilY, irisRad);
		nvgCircle(args.vg, rightPupilX, rightPupilY, irisRad);
		nvgGlobalCompositeOperation(args.vg, NVG_ATOP);
		nvgFill(args.vg);
		nvgClosePath(args.vg);

		//pupil
		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, pupilColor);


		nvgCircle(args.vg, leftPupilX, leftPupilY, pupilRad);
		nvgCircle(args.vg, rightPupilX, rightPupilY, pupilRad);
		nvgGlobalCompositeOperation(args.vg, NVG_ATOP);
		nvgFill(args.vg);
		nvgClosePath(args.vg);

		nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
//eyebrows

	}
	void drawNose(const DrawArgs &args, float x, float y, float height, float width, float thickness, NVGcolor color) {

	}


	void draw(const DrawArgs &args) override {
		if (!module) {
			drawFace(args, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10);
		}
		else {
			drawFace(args, module->bufferX[0][0], module->bufferX[1][0], module->bufferX[2][0], module->bufferX[3][0], module->bufferX[4][0], module->bufferX[5][0], module->bufferX[6][0], module->bufferX[7][0], module->bufferX[8][0], module->bufferX[9][0], module->bufferX[10][0], module->bufferX[11][0], module->bufferX[12][0], module->bufferX[13][0], module->bufferX[14][0], module->bufferX[15][0]);

		}
	}
};


struct FolyPaceWidget : ModuleWidget {
	FolyPaceWidget(FolyPace *module) {
		setModule(module);

		box.size = Vec(9 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareFolyPacePanel.svg")));
			addChild(panel);

		}

		{
			FolyPaceDisplay *display = new FolyPaceDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addInput(createInput<PointingUpPentagonPort>(Vec(1, 353), module, FolyPace::X_INPUT));
		addParam(createParam<SmallKnob>(Vec(31, 356), module, FolyPace::TRIM));
		addParam(createParam<SmoothKnob>(Vec(51, 353), module, FolyPace::OFFSET));


	}
};


Model *modelComputerscareFolyPace = createModel<FolyPace, FolyPaceWidget>("computerscare-foly-pace");
