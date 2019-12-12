#include <string.h>
#include "plugin.hpp"
#include "Computerscare.hpp"
#include "dtpulse.hpp"


static const int BUFFER_SIZE = 512;


struct StolyFickPigure : Module {
	enum ParamIds {
		TIME_PARAM,
		TRIM,
		OFFSET,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float bufferX[16][BUFFER_SIZE] = {};
	int channelsX = 0;
	int bufferIndex = 0;
	int frameIndex = 0;

	StolyFickPigure() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
			const float timeBase = (float) BUFFER_SIZE / 6;

		configParam(TIME_PARAM, 6.f, 16.f, 14.f, "Time", " ms/div", 1 / 2.f, 1000 * timeBase);

		configParam(TRIM, -2.f, 2.f, 1.f, "Input Trim");
		configParam(OFFSET, -5.f, 5.f, 0.f, "Input Offset", " Volts");

	
	}

	void onReset() override {
		//std::memset(bufferX, 0, sizeof(bufferX));
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

		// Add frame to buffer
		if (bufferIndex < BUFFER_SIZE) {
			if (++frameIndex > frameCount) {
				frameIndex = 0;
				float trimVal = params[TRIM].getValue();
				float offsetVal = params[OFFSET].getValue();

				if (inputs[X_INPUT].isConnected()) {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(std::min(c, this->channelsX)) * trimVal + offsetVal + 99 + (1071 * c) % 19;
					}
				}
				else {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = offsetVal + 99 + (1071 * c) % 19;
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

		float size = 1+sin(O-29)/4;

		//crotch
		float cx = 62*(1+(sin(E+F)-sin(P+O/2+50))/40000);
		float cy = 210*(1+(sin(A+G-12)-sin(P+H/2))/11000);

		//thigh spread, length, direction
		float thighSpread = (2+sin(J+I+K)-sin(A-N/2))/4;
		float thighLength = 50*(1+(sin(C-100+F+K*2)+sin(C+L-10))/6);
		float thighDirection = (sin(J+O-211)-sin(P*2+I)-sin(B+K))/2;


		//ankle spread,length,direction
		float ankleSpread = (2+sin(O-B)/2+sin(F+2)/2+sin(P-E-D+19.2))/13;
		float ankleLength = thighLength*(1+(sin(F+A+J-K/2+9))/9);
		float ankleDirection =  3*M_PI/2+(3+sin(J+M-L-101)-sin(P-B+22)-sin(H))/8;

		float leftKneeArg = 3*M_PI/2 +thighDirection + thighSpread;
		float rightKneeArg = 3*M_PI/2 +thighDirection - thighSpread;
		

		float leftAnkleArg =  ankleDirection+ankleSpread;
		float rightAnkleArg =  ankleDirection-ankleSpread;


		float leftKneeX=cx+thighLength*cos(leftKneeArg);
		float leftKneeY=cy-thighLength*sin(leftKneeArg);

		float leftAnkleX = leftKneeX+ankleLength*cos(leftAnkleArg);
		float leftAnkleY = leftKneeY-ankleLength*sin(leftAnkleArg);

		float rightKneeX=cx+thighLength*cos(rightKneeArg);
		float rightKneeY=cy-thighLength*sin(rightKneeArg);

		float rightAnkleX = rightKneeX+ankleLength*cos(rightAnkleArg);
		float rightAnkleY = rightKneeY-ankleLength*sin(rightAnkleArg);


		nvgBeginPath(args.vg);
		
		
		nvgMoveTo(args.vg, leftAnkleX,leftAnkleY);
		nvgLineTo(args.vg, leftKneeX,leftKneeY);
		nvgLineTo(args.vg, cx,cy);
		nvgLineTo(args.vg, rightKneeX,rightKneeY);
		nvgLineTo(args.vg, rightAnkleX,rightAnkleY);

		//nvgClosePath(args.vg);
		nvgStroke(args.vg);


		//torso length,direction
		float torsoLength=thighLength*(1.4+(sin(A-12))/4);
		float torsoDirection=M_PI/2+sin(D)/2;

		float neckX = cx+torsoLength*cos(torsoDirection);
		float neckY = cy-torsoLength*sin(torsoDirection);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,cx,cy);
		nvgLineTo(args.vg,neckX,neckY);
		nvgStroke(args.vg);
		
		float armLength=torsoLength*(2+(sin(N+14)-sin(P-L-3))/2)/4;
		float forearmLength=armLength*(1+(sin(F+B+2)-sin(E))/5);
		float armDirection=3*M_PI/2+0.2*(sin(C-M));
		float armSpread=sin(B+P-A)+sin(N-J);

		float leftElbowArg=armDirection+armSpread;
		float rightElbowArg=armDirection-armSpread;
		
		float leftHandArg=sin(E+22+A-4);
		float rightHandArg=sin(F+22-B);
		
		float leftElbowX = neckX+armLength*cos(leftElbowArg);
		float leftElbowY = neckY-armLength*sin(leftElbowArg);

		float leftHandX=leftElbowX+forearmLength*cos(leftHandArg);
		float leftHandY=leftElbowY-forearmLength*sin(leftHandArg);
		
		float rightElbowX = neckX+armLength*cos(rightElbowArg);
		float rightElbowY = neckY-armLength*sin(rightElbowArg);

		float rightHandX=rightElbowX+forearmLength*cos(rightHandArg);
		float rightHandY=rightElbowY-forearmLength*sin(rightHandArg);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,neckX,neckY);
		nvgLineTo(args.vg,leftElbowX,leftElbowY);
		nvgLineTo(args.vg,leftHandX,leftHandY);
		nvgStroke(args.vg);
		
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,neckX,neckY);
		nvgLineTo(args.vg,rightElbowX,rightElbowY);
		nvgLineTo(args.vg,rightHandX,rightHandY);
		nvgStroke(args.vg);
		
		float headHeight = torsoLength*(0.5+sin(H-E-I-D)/9-sin(F+B-C+E)/7);
		float headWidth = headHeight*(0.6+sin(I+D-M/2)/7+sin(G/2+J-10)/6);
		float headAngle = M_PI/2 + (sin(C+A)/6+sin(D+G)/9);

		float headRotation=sin(C+A)/2+sin(M/2)/3;
		
		nvgBeginPath(args.vg);

		nvgTranslate(args.vg, neckX, neckY);
		nvgRotate(args.vg,headRotation);
		nvgEllipse(args.vg, 0,-headHeight,headWidth,headHeight);

		nvgFill(args.vg);
		nvgStroke(args.vg);

		
		float mouthWidth=headWidth/2;
		float mouthOpen = 100*(1+sin(K))/headHeight;
		float mouthSmile=10*(sin(C)+sin(L))/headHeight;
		float mouthSkew=0;
		float mouthThickness=1;

		float mouthX = 0;
		float mouthY = -headHeight/2+mouthSmile*7;


		NVGcolor mouthLipColor=nvgRGB(0x24, 0x24, 0x31);

		//drawMouth(args, mouthX, mouthY, mouthWidth, mouthOpen, mouthSmile, mouthSkew, mouthThickness, mouthLipColor);


		nvgTranslate(args.vg,-neckX,-neckY);
		
		//nvgGlobalCompositeOperation(args.vg, NVG_ATOP);



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
			drawStickFigure(args, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10, random::uniform() * 10);
		}
		else {
			drawStickFigure(args, module->bufferX[0][0], module->bufferX[1][0], module->bufferX[2][0], module->bufferX[3][0], module->bufferX[4][0], module->bufferX[5][0], module->bufferX[6][0], module->bufferX[7][0], module->bufferX[8][0], module->bufferX[9][0], module->bufferX[10][0], module->bufferX[11][0], module->bufferX[12][0], module->bufferX[13][0], module->bufferX[14][0], module->bufferX[15][0]);
		}
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


	}
	void drawShadow(const DrawArgs& args)  {
		DEBUG("my draw shadow has been called");
		nvgBeginPath(args.vg);
		float r = 20; // Blur radius
		float c = 20; // Corner radius
		math::Vec b = math::Vec(-10, 30); // Offset from each corner
		nvgRect(args.vg, b.x - r, b.y - r, box.size.x - 2 * b.x + 2 * r, box.size.y - 2 * b.y + 2 * r);
		NVGcolor shadowColor = nvgRGBAf(120, 0, 0, 0.7);
		NVGcolor transparentColor = nvgRGBAf(120, 0, 0, 0);
		nvgFillPaint(args.vg, nvgBoxGradient(args.vg, b.x, b.y, box.size.x - 2 * b.x, box.size.y - 2 * b.y, c, r, shadowColor, transparentColor));
		nvgFill(args.vg);
	}
};


Model *modelComputerscareStolyFickPigure = createModel<StolyFickPigure, StolyFickPigureWidget>("computerscare-stoly-fick-pigure");
