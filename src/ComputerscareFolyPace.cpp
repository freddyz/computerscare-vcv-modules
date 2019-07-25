#include <string.h>
#include "plugin.hpp"
#include "Computerscare.hpp"


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
		configParam(X_SCALE_PARAM, -2.f, 8.f, 0.f, "X scale", " V/div", 1/2.f, 5);
		configParam(X_POS_PARAM, -10.f, 10.f, 0.f, "X position", " V");
		configParam(Y_SCALE_PARAM, -2.f, 8.f, 0.f, "Y scale", " V/div", 1/2.f, 5);
		configParam(Y_POS_PARAM, -10.f, 10.f, 0.f, "Y position", " V");
		const float timeBase = (float) BUFFER_SIZE / 6;
		configParam(TIME_PARAM, 6.f, 16.f, 14.f, "Time", " ms/div", 1/2.f, 1000 * timeBase);
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
				for (int c = 0; c < channelsX; c++) {
					bufferX[c][bufferIndex] = inputs[X_INPUT].getVoltage(c);
				}
				for (int c = 0; c < channelsY; c++) {
					bufferY[c][bufferIndex] = inputs[Y_INPUT].getVoltage(c);
				}
				bufferIndex++;
			}
		}

		// Don't wait for trigger if still filling buffer
		if (bufferIndex < BUFFER_SIZE) {
			return;
		}

		// Trigger immediately if external but nothing plugged in, or in Lissajous mode
		if (lissajous || (external && !inputs[TRIG_INPUT].isConnected())) {
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
		const float holdTime = 0.5f;
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
	int statsFrame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vpp = 0.f;
		float vmin = 0.f;
		float vmax = 0.f;

		void calculate(float *buffer, int channels) {
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE * channels; i++) {
				float v = buffer[i];
				vmax = std::fmax(vmax, v);
				vmin = std::fmin(vmin, v);
			}
			vpp = vmax - vmin;
		}
	};

	Stats statsX, statsY;

	FolyPaceDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Oswald-Regular.ttf"));
	}

	void drawWaveform(const DrawArgs &args, float *bufferX, float offsetX, float gainX, float *bufferY, float offsetY, float gainY) {
		assert(bufferY);
		nvgSave(args.vg);
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		for (int i = 0; i < BUFFER_SIZE; i++) {
			Vec v;
			if (bufferX)
				v.x = (bufferX[i] + offsetX) * gainX / 2.f + 0.5f;
			else
				v.x = (float) i / (BUFFER_SIZE - 1);
			v.y = (bufferY[i] + offsetY) * gainY / 2.f + 0.5f;
			Vec p;
			p.x = rescale(v.x, 0.f, 1.f, b.pos.x, b.pos.x + b.size.x);
			p.y = rescale(v.y, 0.f, 1.f, b.pos.y + b.size.y, b.pos.y);
			if (i == 0)
				nvgMoveTo(args.vg, p.x, p.y);
			else
				nvgLineTo(args.vg, p.x, p.y);
		}
		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 2.f);
		nvgStrokeWidth(args.vg, 1.5f);
		nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
		nvgStroke(args.vg);
		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}
	void drawFace(const DrawArgs &args, float *bufferX, float offsetX, float gainX, float *bufferY, float offsetY, float gainY) {
		float ox=70;
		float oy = 150;

		float h = 0.4;
		float s = 0.8;
		float l = 0.5;
		float fx = ox;
		float fy= oy;
		float mpx=0;
		float mpy=60;
		float frx=70;
		float fry=150;
		float fr = 0.1;
		float msx=ox+mpx-40;
		float msy=oy+mpy+30;
		
		float m1x=ox+mpx+10;
		float m1y=oy-mpy+26;

		float m2x=ox+mpx+21;
		float m2y=oy+mpy+34;

		float mex=ox+mpx+20;
		float mey=oy+mpy+30;

		assert(bufferY);
		nvgSave(args.vg);
		Rect b = Rect(Vec(0, 0),box.size);

		nvgFillColor(args.vg, nvgHSLA(h,s,l,0xff));
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		
		nvgRotate(args.vg,fr);
		nvgEllipse(args.vg, fx,fy,frx,fry);
		nvgRotate(args.vg,-fr);
		

		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 2.f);
		nvgStrokeWidth(args.vg, 4.5f);

		nvgFill(args.vg);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		nvgBeginPath(args.vg);


		nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
		nvgMoveTo(args.vg, msx, msy);
		nvgBezierTo(args.vg, m1x, m1y, m2x, m2y, mex, mey);
		nvgStroke(args.vg);

		
		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}

	void drawTrig(const DrawArgs &args, float value) {
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);

		value = value / 2.f + 0.5f;
		Vec p = Vec(box.size.x, b.pos.y + b.size.y * (1.f - value));

		// Draw line
		nvgStrokeColor(args.vg, nvgRGBA(0x1f, 0x24, 0x16, 0xff));
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p.x - 13, p.y);
			nvgLineTo(args.vg, 0, p.y);
			nvgClosePath(args.vg);
		}
		nvgStroke(args.vg);

		// Draw indicator
		nvgFillColor(args.vg, nvgRGBA(0x1f, 0x24, 0x16, 0xff));
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p.x - 2, p.y - 4);
			nvgLineTo(args.vg, p.x - 9, p.y - 4);
			nvgLineTo(args.vg, p.x - 13, p.y);
			nvgLineTo(args.vg, p.x - 9, p.y + 4);
			nvgLineTo(args.vg, p.x - 2, p.y + 4);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);

		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgFillColor(args.vg, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
		nvgText(args.vg, p.x - 8, p.y + 3, "T", NULL);
		nvgResetScissor(args.vg);
	}

	void drawStats(const DrawArgs &args, Vec pos, const char *title, Stats *stats) {
		nvgFontSize(args.vg, 13);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xf0));
		nvgText(args.vg, pos.x + 6, pos.y + 11, title, NULL);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xd0));
		pos = pos.plus(Vec(22, 11));

		std::string text;
		text = "reginald hormacknadaunald";
		nvgText(args.vg, pos.x, pos.y, text.c_str(), NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

		float gainX = std::pow(2.f, std::round(module->params[FolyPace::X_SCALE_PARAM].getValue())) / 10.f;
		float gainY = std::pow(2.f, std::round(module->params[FolyPace::Y_SCALE_PARAM].getValue())) / 10.f;
		float offsetX = module->params[FolyPace::X_POS_PARAM].getValue();
		float offsetY = module->params[FolyPace::Y_POS_PARAM].getValue();

		// Draw waveforms
		if (module->lissajous) {
			// X x Y
			int lissajousChannels = std::max(module->channelsX, module->channelsY);
			for (int c = 0; c < lissajousChannels; c++) {
				drawFace(args, module->bufferX[c], offsetX, gainX, module->bufferY[c], offsetY, gainY);
			}
		}
		else {
			// Y
			for (int c = 0; c < module->channelsY; c++) {
				nvgStrokeColor(args.vg, nvgRGBA(0x1f, 0x24, 0x16, 0xdf));
				drawWaveform(args, NULL, 0, 0, module->bufferY[c], offsetY, gainY);
			}

			// X
			for (int c = 0; c < module->channelsX; c++) {
				nvgStrokeColor(args.vg, nvgRGBA(0x9f, 0x24, 0x16, 0xcf));
				drawWaveform(args, NULL, 0, 0, module->bufferX[c], offsetX, gainX);
			}

			float trigThreshold = module->params[FolyPace::TRIG_PARAM].getValue();
			trigThreshold = (trigThreshold + offsetX) * gainX;
			drawTrig(args, trigThreshold);
		}

		// Calculate and draw stats
		if (++statsFrame >= 4) {
			statsFrame = 0;
			statsX.calculate(module->bufferX[0], module->channelsX);
			statsY.calculate(module->bufferY[0], module->channelsY);
		}
		drawStats(args, Vec(0, 0), "Josh", &statsX);
		drawStats(args, Vec(0, box.size.y - 15), "Billy", &statsY);
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

		addInput(createInput<PJ301MPort>(Vec(17, 319), module, FolyPace::X_INPUT));
		addInput(createInput<PJ301MPort>(Vec(63, 319), module, FolyPace::Y_INPUT));
		addInput(createInput<PJ301MPort>(Vec(154, 319), module, FolyPace::TRIG_INPUT));

	}
};


Model *modelComputerscareFolyPace = createModel<FolyPace, FolyPaceWidget>("computerscare-foly-pace");
