#include <string.h>
#include <ctime>
#include "plugin.hpp"
#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
#include "dtpulse.hpp"


static const int BUFFER_SIZE = 512;


struct DrolyPaw : Module {
	enum ParamIds {
		TIME_PARAM,
		INPUT_TRIM,
		INPUT_OFFSET,
		SCRAMBLE,
		DRAW_MODE,
		CLEAR_BUTTON,
		CLEAR_EVERY_FRAME,
		DRAWPARAMS_TRIM,
		DRAWPARAMS_OFFSET,
		DRAW_EVERY_FRAME,
		MODULE_WIDTH,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		CLEAR_TRIGGER,
		DRAW_TRIGGER,
		DRAW_GATE,
		CLEAR_GATE,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float bufferX[16][BUFFER_SIZE] = {};
	float bufferY[16][BUFFER_SIZE] = {};
	int cmap[16];
	int cmapParams[16];
	int channelsX = 0;
	int channelsY = 0;
	int bufferIndex = 0;
	int frameIndex = 0;
	int cnt = 0;
	int interiorCounter = 9000;
	bool clearArmed = false;
	float lastScramble = 0;

	rack::dsp::SchmittTrigger globalManualClockTrigger;
	rack::dsp::SchmittTrigger globalManualResetTrigger;


	int A = 31;
	int B = 32;
	int C = 29;
	int D = 2;


	DrolyPaw() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float timeBase = (float) BUFFER_SIZE / 6;

		for (int i = 0; i < 16; i++) {
			cmap[i] = i;
			cmapParams[i] = i;
		}

		configParam(TIME_PARAM, 6.f, 16.f, 14.f, "Time", " ms/div", 1 / 2.f, 1000 * timeBase);

		configParam(INPUT_TRIM, -2.f, 2.f, 0.2f, "Input Attenuverter");
		configParam(INPUT_OFFSET, -5.f, 5.f, 0.f, "Input Offset", " Volts");



		configParam(SCRAMBLE, -10.f, 10.f, 0.f, "Scrambling");
		configParam(DRAW_MODE, 0.f, 64.f, 1.f, "Draw Mode");
		configParam(CLEAR_BUTTON, 0.f, 1.f, 0.f);
		configParam(CLEAR_EVERY_FRAME, 0.f, 1.f, 1.f, "Clear Every Frame");
		configParam(DRAW_EVERY_FRAME, 0.f, 1.f, 1.f, "Draw Every Frame");

		configParam(DRAWPARAMS_TRIM, -2.f, 2.f, 1.f, "Draw Parameters Attenuverter");
		configParam(DRAWPARAMS_OFFSET, -5.f, 5.f, 0.f, "Draw Parameters Offset", " Volts");
		configParam(MODULE_WIDTH, 15.f, 450.f, 195.f, "Width");
	}

	void onReset() override {
		//std::memset(bufferX, 0, sizeof(bufferX));
	}
	void updateScramble(float v) {
		for (int i = 0; i < 16; i++) {
			cmap[i] = i;//(i * A + B + (int)std::floor(v * 1010.1)) % 16;
		}
	}
	void checkScramble() {
		float xx = params[SCRAMBLE].getValue();
		if (lastScramble != xx) {
			lastScramble = xx;
			updateScramble(xx);
		}
	}
	void armClear() {
		clearArmed = true;
	}
	bool checkClear() {
		if (clearArmed) {
			clearArmed = false;
			return true;
		}
		else {
			return false;
		}
	}
	void process(const ProcessArgs &args) override {
		// Modes
		// Compute time
		float deltaTime = std::pow(2.f, -params[TIME_PARAM].getValue());

		int frameCount = (int) std::ceil(deltaTime * args.sampleRate);

		if (params[CLEAR_EVERY_FRAME].getValue() || globalManualResetTrigger.process(params[CLEAR_BUTTON].getValue())) {
			armClear();
		}

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

		if (cnt > 4101) {

			checkScramble();
			cnt = 0;
		}
		cnt++;
		// Add frame to buffer
		if (bufferIndex < BUFFER_SIZE) {
			if (++frameIndex > frameCount) {
				frameIndex = 0;

				float trimVal = params[INPUT_TRIM].getValue();
				float offsetVal = params[INPUT_OFFSET].getValue();

				float paramsTrimVal = params[DRAWPARAMS_TRIM].getValue();
				float paramsOffsetVal = params[DRAWPARAMS_OFFSET].getValue();

				if (inputs[X_INPUT].isConnected()) {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(std::min(cmap[c], this->channelsX)) * trimVal + offsetVal;
						//bufferX[c][bufferIndex]=inputs[X_INPUT].getVoltage(c);
						//bufferX[c][bufferIndex]=inputs[X_INPUT].getVoltage(c)*trimVal+offsetVal;
					}
				}
				else {
					for (int c = 0; c < 16; c++) {
						bufferX[c][bufferIndex] = sin((float)interiorCounter / 100000 * (c + 1)) + offsetVal; // t is an integer type


						//bufferX[c][bufferIndex] = offsetVal + 99 + (1071 * cmap[c]) % 19;
					}
					interiorCounter++;
					if (interiorCounter > 400000) {
						interiorCounter = 0;
					}
				}
				if (inputs[Y_INPUT].isConnected()) {
					for (int c = 0; c < 16; c++) {
						bufferY[c][bufferIndex] = inputs[Y_INPUT].getVoltage(std::min(cmapParams[c], this->channelsY)) * paramsTrimVal + paramsOffsetVal;
					}
				}
				else {
					for (int c = 0; c < 16; c++) {


						bufferY[c][bufferIndex] = paramsTrimVal * sin((float)interiorCounter / 100000 * (c + 1)) + paramsOffsetVal;
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

	float bget(int ch, int offset = 0) {
		return bufferX[ch % 16][offset];
	}
	float bgetf(int ch) {
		return bufferX[(ch * ch + 910) % 16][(ch * ch * ch - ch * ch + 10101) % BUFFER_SIZE];
	}


	void trigger() {
		bufferIndex = 0;
		frameIndex = 0;
	}
};
namespace rack {
namespace widget {
struct NoClearWidget : FramebufferWidget {
	NoClearWidget() {
		FramebufferWidget();
	}
	void step() override {
		// Render every frame
		dirty = true;
		FramebufferWidget::step();
	}
	/** Draws to the framebuffer.
	Override to initialize, draw, and flush the OpenGL state.
	*/
	void drawFramebuffer() override  {
		NVGcontext* vg = APP->window->vg;

		float pixelRatio = fbSize.x * oversample / fbBox.size.x;
		nvgBeginFrame(vg, fbBox.size.x, fbBox.size.y, pixelRatio);

		// Use local scaling
		nvgTranslate(vg, -fbBox.pos.x, -fbBox.pos.y);
		nvgTranslate(vg, fbOffset.x, fbOffset.y);
		nvgScale(vg, fbScale.x, fbScale.y);

		DrawArgs args;
		args.vg = vg;
		args.clipBox = box.zeroPos();
		args.fb = fb;
		Widget::draw(args);

		glViewport(0.0, 0.0, fbSize.x * oversample, fbSize.y * oversample);
		//glClearColor(0.0, 0.0, 0.0, 0.0);
		// glClearColor(0.0, 1.0, 1.0, 0.5);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		nvgEndFrame(vg);

		// Clean up the NanoVG state so that calls to nvgTextBounds() etc during step() don't use a dirty state.
		nvgReset(vg);
	}
};

struct DrolyPawDisplay : FramebufferWidget {
	DrolyPaw *module;
	float width=195;
	void step() override {

		dirty = true;
		box.size.x = width;
		FramebufferWidget::step();
		
	}
	/*DrolyPawDisplay() {
		FramebufferWidget();
	}*/
	void drawThingie(const DrawArgs &args, float buffer[16][BUFFER_SIZE], float paramsBuffer[16][BUFFER_SIZE]) {
		DrawHelper draw = DrawHelper(args.vg);
		Points pts = Points();
		width = module->params[DrolyPaw::MODULE_WIDTH].getValue();
		nvgTranslate(args.vg, width/2, 190);
		int mode = module->params[DrolyPaw::DRAW_MODE].getValue();

		if (mode == 0) {
			for (int i = 0; i < 4; i++) {
				pts.triangle(Vec(buffer[i * 4][0] * 10, buffer[i * 4 + 1][0] * 10), Vec(buffer[i * 4 + 2][0] * 3, buffer[i * 4 + 3][0] * 3));
				pts.offset(Vec(buffer[(i * 11 + 3) % 16][0], buffer[(i * 3 + 11) % 16][0]));
				draw.drawShape(pts.get(), draw.sincolor(buffer[0][0] + i * buffer[1][0]));
			}
		}
		else if (mode == 1) {
			pts.spray(100);
			pts.scale(Vec(buffer[2][0]*buffer[0][0], buffer[2][0]*buffer[1][0]));
			draw.drawField(pts.get(), draw.sincolor(buffer[3][0] + buffer[4][0]*random::uniform()), buffer[5][0]*buffer[6][0]);
			//draw.drawDots(pts.get(), draw.sincolor(random::uniform()), 2.f);
		}
		else if (mode == 2) {
			//16 horizontal lines


			float spaceexp = 1 / (1 + expf(-paramsBuffer[3][0]));
			pts.linear(16, Vec(0, -spaceexp * box.size.y / 2), Vec(0, spaceexp * box.size.y + 40));
			std::vector<Vec> polyVals;
			std::vector<NVGcolor> colors;
			std::vector<Vec> thicknesses;

			for (int i = 0; i < 16; i++) {
				polyVals.push_back(Vec(buffer[i][0] * width/4, 0.f));
				colors.push_back(draw.sincolor(paramsBuffer[0][0] + 2 + paramsBuffer[2][0]*i));

				thicknesses.push_back(Vec(expf(paramsBuffer[1][0] * 2 + 2) + 0.5, 0));
			}
			draw.drawLines(pts.get(), polyVals, colors, thicknesses);
		}
		else if ( mode == 3) {
			//number,-dTHickness,dAngle,dColor (passed to sincolor)
			draw.drawLines(fmin(65, std::floor(exp(40 * buffer[0][1]) + 2)), buffer[1][0], buffer[2][0] / 4, buffer[3][0] * 2);
		}
		else {
			int nx = (mode * 17) % 10;
			int ny = (mode * 11 + 3) % 10;
			pts.grid(nx, ny, Vec(buffer[2][0] * 10, buffer[1][0] * 10));
			Points rTheta = Points();
			rTheta.linear(nx * ny, Vec(30, 0), Vec(buffer[3][0], buffer[0][0]));
			draw.drawLines(pts.get(), rTheta.get(), draw.sincolor(buffer[0][0]), 2);
		}
	}
	void drawThingie(const DrawArgs &args) {

	}
	void drawFramebuffer() override  {
		NVGcontext* vg = APP->window->vg;

		float pixelRatio = fbSize.x * oversample / fbBox.size.x;
		nvgBeginFrame(vg, fbBox.size.x, fbBox.size.y, pixelRatio);

		// Use local scaling
		nvgTranslate(vg, -fbBox.pos.x, -fbBox.pos.y);
		nvgTranslate(vg, fbOffset.x, fbOffset.y);
		nvgScale(vg, fbScale.x, fbScale.y);

		DrawArgs args;
		args.vg = vg;
		args.clipBox = box.zeroPos();
		args.fb = fb;
		//Widget::draw(args);
		if (!module) {
			drawThingie(args);
		}
		else {
			if (module->params[DrolyPaw::DRAW_EVERY_FRAME].getValue()) {
				drawThingie(args, module->bufferX, module->bufferY);
			}

		}

		glViewport(0.0, 0.0, fbSize.x * oversample, fbSize.y * oversample);
		if (module) {
			if (module->checkClear()) {
				glClearColor(0.0, 0.0, 0.0, 0.0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			}
		}
		nvgEndFrame(vg);

		// Clean up the NanoVG state so that calls to nvgTextBounds() etc during step() don't use a dirty state.
		nvgReset(vg);
	}
};
}
}


struct DrolyPawWidget : ModuleWidget {
	DrolyPawWidget(DrolyPaw *module) {
		setModule(module);

		if (module) {
			DEBUG("is mod");
			box.size = Vec(module->params[DrolyPaw::MODULE_WIDTH].getValue(), 380);
		}
		else {
			box.size = Vec(13 * 15, 380);//195
		}
		{
			BGPanel *bgPanel = new BGPanel(nvgRGB(0x16, 0x15, 0x19));
			bgPanel->box.size = box.size;
			this->bgPanel = bgPanel;
			addChild(bgPanel);

			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareDrolyPawPanel.svg")));
			addChild(panel);

		}

		{
			DrolyPawDisplay *display = new DrolyPawDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			//display->sizex
			addChild(display);
		}


		addInput(createInput<PointingUpPentagonPort>(Vec(1, 353), module, DrolyPaw::X_INPUT));


		addParam(createParam<SmallKnob>(Vec(31, 357), module, DrolyPaw::INPUT_TRIM));
		addParam(createParam<SmoothKnob>(Vec(51, 353), module, DrolyPaw::INPUT_OFFSET));

		addInput(createInput<PointingUpPentagonPort>(Vec(81, 353), module, DrolyPaw::Y_INPUT));
		addParam(createParam<SmallKnob>(Vec(101, 357), module, DrolyPaw::DRAWPARAMS_TRIM));
		addParam(createParam<SmoothKnob>(Vec(121, 353), module, DrolyPaw::DRAWPARAMS_OFFSET));

		//addParam(createParam<ScrambleKnob>(Vec(81, 357), module, DrolyPaw::SCRAMBLE));
		addParam(createParam<MediumDotSnapKnob>(Vec(141, 354), module, DrolyPaw::DRAW_MODE));
		addParam(createParam<ComputerscareResetButton>(Vec(1, 334), module, DrolyPaw::CLEAR_BUTTON));
		addParam(createParam<SmallIsoButton>(Vec(24, 334), module, DrolyPaw::CLEAR_EVERY_FRAME));
		addParam(createParam<SmallIsoButton>(Vec(44, 334), module, DrolyPaw::DRAW_EVERY_FRAME));


		ComputerscareResizeHandle *leftHandle = new ComputerscareResizeHandle();

		ComputerscareResizeHandle *rightHandle = new ComputerscareResizeHandle();
		rightHandle->right = true;
		this->rightHandle = rightHandle;
		addChild(leftHandle);
		addChild(rightHandle);
		drolyPawModule = module;


	}

	void step() override {
		if (module) {
			float width =  module->params[DrolyPaw::MODULE_WIDTH].getValue();
			//DEBUG("%f",width);
			if (box.size.x != width) {
				/*if(drolyPawModule->manualSetWidth) {
					DEBUG("Manual set");
					box.size.x=width;
					drolyPawModule->manualSetWidth=false;
				}*/
				//else {
					DEBUG("%f",width);
					bgPanel->box.size.x=box.size.x;
					module->params[DrolyPaw::MODULE_WIDTH].setValue(box.size.x);
				//}
			}
			rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
			
			ModuleWidget::step();
		}

	};
	BGPanel *bgPanel;
	DrolyPaw *drolyPawModule;
	ComputerscareResizeHandle *leftHandle;
	ComputerscareResizeHandle *rightHandle;
};


Model *modelComputerscareDrolyPaw = createModel<DrolyPaw, DrolyPawWidget>("computerscare-droly-paw");
