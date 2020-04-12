#include <string.h>
#include "plugin.hpp"
#include "Computerscare.hpp"
#include "dtpulse.hpp"


static const int BUFFER_SIZE = 512;


struct DrolyPaw : Module {
	enum ParamIds {
		TIME_PARAM,
		TRIM,
		OFFSET,
		SCRAMBLE,
		DRAW_MODE,
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


	DrolyPaw() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float timeBase = (float) BUFFER_SIZE / 6;

		for (int i = 0; i < 16; i++) {
			cmap[i] = i;
		}

		configParam(TIME_PARAM, 6.f, 16.f, 14.f, "Time", " ms/div", 1 / 2.f, 1000 * timeBase);

		configParam(TRIM, -2.f, 2.f, 0.2f, "Input Trim");
		configParam(OFFSET, -5.f, 5.f, 0.f, "Input Offset", " Volts");

		configParam(SCRAMBLE, -10.f, 10.f, 0.f, "Scrambling");
		configParam(DRAW_MODE, 0.f, 64.f, 1.f, "Draw Mode");

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
						bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(std::min(cmap[c], this->channelsX)) * trimVal + offsetVal;
						//bufferX[c][bufferIndex]=inputs[X_INPUT].getVoltage(cmap[c]);
						//bufferX[c][bufferIndex]=inputs[X_INPUT].getVoltage(c);
						//bufferX[c][bufferIndex]=inputs[X_INPUT].getVoltage(c)*trimVal+offsetVal;
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

	float bget(int ch) {
		return bufferX[ch % 16][0];
	}
	float bgetf(int ch) {
		return bufferX[(ch * ch + 910) % 16][(ch * ch * ch - ch * ch + 10101) % BUFFER_SIZE];
	}


	void trigger() {
		bufferIndex = 0;
		frameIndex = 0;
	}
};
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
	void step() override {
		// Render every frame
		dirty = true;
		FramebufferWidget::step();
	}
	/*DrolyPawDisplay() {
		FramebufferWidget();
	}*/
	void drawThingie(const DrawArgs &args, float buffer[16][BUFFER_SIZE]) {
		DrawHelper draw = DrawHelper(args.vg);
		Points pts = Points();
		nvgTranslate(args.vg, 67.5, 190);
		int mode = module->params[DrolyPaw::DRAW_MODE].getValue();
		if (mode == 0) {
			for (int i = 0; i < 4; i++) {
				pts.triangle(Vec(buffer[i * 4][0] * 10, buffer[i * 4 + 1][0] * 10), Vec(buffer[i * 4 + 2][0] * 3, buffer[i * 4 + 3][0] * 3));
				pts.offset(Vec(buffer[(i * 11 + 3) % 16][0], buffer[(i * 3 + 11) % 16][0]));
				draw.drawShape(pts.get(), draw.sincolor(buffer[0][0]+i*buffer[1][0]));
			}
		}
		else if (mode == 1) {
			pts.spray(100);
			pts.scale(Vec(buffer[2][0]*buffer[0][0], buffer[2][0]*buffer[1][0]));
			draw.drawDots(pts.get(), draw.sincolor(random::uniform()), 2.f);
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
			drawThingie(args, module->bufferX);
		}

		glViewport(0.0, 0.0, fbSize.x * oversample, fbSize.y * oversample);
		//glClearColor(0.0, 0.0, 0.0, 0.0);
		// glClearColor(0.0, 1.0, 1.0, 0.5);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		nvgEndFrame(vg);

		// Clean up the NanoVG state so that calls to nvgTextBounds() etc during step() don't use a dirty state.
		nvgReset(vg);
	}
};
struct DrolyGLDisplay : OpenGlWidget {
	DrolyPaw *module;
	DrolyGLDisplay() {

	}

	void drawFramebuffer() override {
		float a = module->bufferX[0][0];
		float b = module->bufferX[1][0];

		float c = module->bufferX[2][0];

		glViewport(0.0, 0.0, fbSize.x, fbSize.y);
		glClearColor(a * 2, b * 2, c * 2, 1.0);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(a, fbSize.x, 0.0, fbSize.y, -1.0, 1.0);

		glBegin(GL_TRIANGLES);
		glColor3f(a / 10, b / 10, c / 10);
		glVertex3f(fbSize.x / 2 + b * 10, fbSize.y / 2 + c * 10, a * 10);
		glColor3f(0, 1, 0);
		glVertex3f(fbSize.x / 2 + c * 10, 0, 0);
		glColor3f(0, 0, 1);
		glVertex3f(0, fbSize.y / 2 + b * 10, 0);
		glEnd();
	}
};


struct DrolyPawWidget : ModuleWidget {
	DrolyPawWidget(DrolyPaw *module) {
		setModule(module);

		box.size = Vec(9 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareStolyFickPigurePanel.svg")));
			addChild(panel);

		}

		{
			DrolyGLDisplay *gl = new DrolyGLDisplay();
			gl->module = module;
			gl->box.pos = Vec(0, 0);
			gl->box.size = Vec(box.size.x, box.size.y);
			//addChild(gl);
		}

		{
			DrolyPawDisplay *display = new DrolyPawDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}


		addInput(createInput<PointingUpPentagonPort>(Vec(1, 353), module, DrolyPaw::X_INPUT));
		addParam(createParam<SmallKnob>(Vec(31, 357), module, DrolyPaw::TRIM));
		addParam(createParam<SmoothKnob>(Vec(51, 353), module, DrolyPaw::OFFSET));

		addParam(createParam<ScrambleKnob>(Vec(81, 357), module, DrolyPaw::SCRAMBLE));
		addParam(createParam<MediumDotSnapKnob>(Vec(101, 354), module, DrolyPaw::DRAW_MODE));


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


Model *modelComputerscareDrolyPaw = createModel<DrolyPaw, DrolyPawWidget>("computerscare-droly-paw");
