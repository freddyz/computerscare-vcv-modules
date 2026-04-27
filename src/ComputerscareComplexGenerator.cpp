#include "Computerscare.hpp"
#include "complex/ComplexControl.hpp"

#include <array>

struct ComputerscareComplexGenerator;

	const int numComplexGeneratorKnobs = 16;


struct ComputerscareComplexGenerator : ComputerscareComplexBase {
	ComputerscareSVGPanel* panelRef;
	int laneControlModes[numComplexGeneratorKnobs] = {};
	enum ParamIds {
		COMPLEX_XY,

		COMPOLY_CHANNELS=COMPLEX_XY + 2*numComplexGeneratorKnobs,
		
		SCALE_VAL_AB,
		SCALE_TRIM_AB = SCALE_VAL_AB+2,

		OFFSET_VAL_AB = SCALE_TRIM_AB+2,
		OFFSET_TRIM_AB = OFFSET_VAL_AB+2,

		DELTA_SCALE_AB = OFFSET_TRIM_AB+2,
		DELTA_OFFSET_AB = DELTA_SCALE_AB+2,
		

		MAIN_OUTPUT_MODE,

		NEXT,
		GLOBAL_SCALE,
		GLOBAL_OFFSET,
		NUM_PARAMS

	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		COMPOLY_MAIN_OUT_A,
		COMPOLY_MAIN_OUT_B,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};




	ComputerscareComplexGenerator()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < numComplexGeneratorKnobs; i++) {
			configParam(COMPLEX_XY + 2*i, -10.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
			configParam(COMPLEX_XY + 2*i+1, -10.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
		}

		configParam(SCALE_VAL_AB    , -10.f, 10.f, 1.f, "Channel " );
		configParam(SCALE_VAL_AB + 1, -10.f, 10.f, 0.f, "Channel ");
		getParamQuantity(SCALE_VAL_AB)->randomizeEnabled = false;
		getParamQuantity(SCALE_VAL_AB+1)->randomizeEnabled = false;

		configParam(OFFSET_VAL_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(OFFSET_VAL_AB + 1, -10.f, 10.f, 0.f, "Channel ");
		getParamQuantity(OFFSET_VAL_AB)->randomizeEnabled = false;
		getParamQuantity(OFFSET_VAL_AB+1)->randomizeEnabled = false;


		configParam(DELTA_OFFSET_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(DELTA_OFFSET_AB + 1, -10.f, 10.f, 0.f, "Channel ");


		configParam(COMPOLY_CHANNELS, 1.f, 16.f, 16.f, "Compoly Lanes");
		configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
		configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");
		configParam<cpx::CompolyModeParam>(MAIN_OUTPUT_MODE,0.f,3.f,0.f,"Main Output Mode");

		getParamQuantity(COMPOLY_CHANNELS)->randomizeEnabled = false;
		getParamQuantity(COMPOLY_CHANNELS)->resetEnabled = false;
		getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
		getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;
		getParamQuantity(MAIN_OUTPUT_MODE)->randomizeEnabled = false;

		configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE,0>>(COMPOLY_MAIN_OUT_A, "Main");
		configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE,1>>(COMPOLY_MAIN_OUT_B, "Main");

	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();
		
		float offsetX = params[OFFSET_VAL_AB].getValue();
		float offsetY = params[OFFSET_VAL_AB+1].getValue();

		float scaleX = params[SCALE_VAL_AB].getValue();
		float scaleY = params[SCALE_VAL_AB+1].getValue();

		math::Vec scaleRect = Vec(scaleX,scaleY);




		int mainOutputMode = params[MAIN_OUTPUT_MODE].getValue();
		for (int i = 0; i < polyChannels; i++) {
			if(i < numComplexGeneratorKnobs) {

				float x0 = params[COMPLEX_XY + 2*i].getValue();
				float y0 = params[COMPLEX_XY + 2*i+1].getValue();

				float x1 = x0*scaleRect.x - y0*scaleRect.y;
				float y1 = x0*scaleRect.y + y0*scaleRect.x;

				float outX = x1 + offsetX;
				float outY = y1 + offsetY;
				float outR = std::hypot(outX, outY);
				float outTheta = std::atan2(outY, outX);

				setOutputVoltages(COMPOLY_MAIN_OUT_A, mainOutputMode, i, outX, outY, outR, outTheta);
			} 
			
			//outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue()*trim + offset, i);
		}
	}
	void checkPoly() override {
		polyChannels = params[COMPOLY_CHANNELS].getValue();
		if (polyChannels == 0) {
			polyChannels = 16;
			params[COMPOLY_CHANNELS].setValue(16);
		}
		int mainOutputMode = params[MAIN_OUTPUT_MODE].getValue();
		setOutputChannels(COMPOLY_MAIN_OUT_A, mainOutputMode, polyChannels);
	}
	json_t *dataToJson() override {
    json_t *rootJ = json_object();
		json_t *laneControlModesJ = json_array();
		for (int i = 0; i < numComplexGeneratorKnobs; i++) {
			json_array_append_new(laneControlModesJ, json_integer(laneControlModes[i]));
		}
		json_object_set_new(rootJ, "laneControlModes", laneControlModesJ);
    return rootJ;
  }

    void dataFromJson(json_t *rootJ) override {
		json_t *laneControlModesJ = json_object_get(rootJ, "laneControlModes");
		if (laneControlModesJ && json_is_array(laneControlModesJ)) {
			for (int i = 0; i < numComplexGeneratorKnobs; i++) {
				json_t *modeJ = json_array_get(laneControlModesJ, i);
				if (modeJ) {
					int mode = json_integer_value(modeJ);
					laneControlModes[i] = std::max(0, std::min(3, mode));
				}
			}
		}
    }
};

struct NoRandomSmallKnob : SmallKnob {
	NoRandomSmallKnob() {
		SmallKnob();
	};
};
struct NoRandomMediumSmallKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/computerscare-medium-small-knob.svg"));

	NoRandomMediumSmallKnob() {
		setSvg(enabledSvg);
		RoundKnob();
	};
};

struct DisableableSmoothKnob : RoundKnob {
	std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
	std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/computerscare-medium-small-knob-disabled.svg"));

	int channel = 0;
	bool disabled = false;
	ComputerscarePolyModule *module;

	DisableableSmoothKnob() {
		setSvg(enabledSvg);
		shadow->box.size = math::Vec(0, 0);
		shadow->opacity = 0.f;
	}
	void step() override {
		if (module) {
			bool candidate = channel > module->polyChannels - 1;
			if (disabled != candidate) {
				setSvg(candidate ? disabledSvg : enabledSvg);
				onChange(*(new event::Change()));
				fb->dirty = true;
				disabled = candidate;
			}
		}
		else {
		}
		RoundKnob::step();
	}
};

struct ComplexGeneratorLaneControl;

struct ComplexGeneratorLaneModeButton : TransparentWidget {
	ComplexGeneratorLaneControl* laneControl = nullptr;

	void onButton(const event::Button &e) override;
	void draw(const DrawArgs &args) override;
};

struct ComplexGeneratorLaneControl : Widget {
	ComputerscareComplexGenerator* module = nullptr;
	int paramIndex = 0;
	int laneIndex = 0;
	int localMode = 0;
	int lastMode = -1;
	cpx::ComplexControl* control = nullptr;

	ComplexGeneratorLaneControl(ComputerscareComplexGenerator* module, int laneIndex,
	                            int paramIndex) {
		this->module = module;
		this->laneIndex = laneIndex;
		this->paramIndex = paramIndex;
	}

	int mode() const {
		if (module)
			return module->laneControlModes[laneIndex];
		return localMode;
	}

	void setMode(int mode) {
		mode = std::max(0, std::min(3, mode));
		if (module)
			module->laneControlModes[laneIndex] = mode;
		else
			localMode = mode;
	}

	void nextMode() {
		setMode((mode() + 1) % 4);
	}

	void rebuildControl(int mode) {
		if (control) {
			removeChild(control);
			delete control;
			control = nullptr;
		}

		cpx::ComplexControlPreset preset = cpx::ComplexControlPreset::Arrow;
		if (mode == 1 || mode == 2)
			preset = cpx::ComplexControlPreset::XYKnobs;
		else if (mode == 3)
			preset = cpx::ComplexControlPreset::ArrowXY;

		control = new cpx::ComplexControl(module, paramIndex, preset);
		control->box = Rect(Vec(0.f, 0.f), box.size);
		control->layoutChildren();
		addChildBottom(control);

		if (mode == 1 || mode == 2 || mode == 3) {
			control->setShowDisplay(true);
			if (control->display) {
				control->display->sourceMode = cpx::ComplexDisplayWidget::SourceMode::Rect;
				control->display->displayMode = mode == 2
					? cpx::ComplexDisplayWidget::DisplayMode::Polar
					: cpx::ComplexDisplayWidget::DisplayMode::Rect;
			}
		}
	}

	void step() override {
		int currentMode = mode();
		if (currentMode != lastMode) {
			lastMode = currentMode;
			rebuildControl(currentMode);
		}
		Widget::step();
	}

	void draw(const DrawArgs &args) override {
		Widget::draw(args);
		if (module && laneIndex >= module->polyChannels) {
			nvgBeginPath(args.vg);
			nvgEllipse(args.vg, box.size.x * 0.5f, box.size.y * 0.5f,
			           box.size.x * 0.5f, box.size.y * 0.5f);
			nvgFillColor(args.vg, nvgRGBA(120, 120, 120, 135));
			nvgFill(args.vg);
		}
	}
};

void ComplexGeneratorLaneModeButton::onButton(const event::Button &e) {
	if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
		e.consume(this);
		if (laneControl)
			laneControl->nextMode();
	}
}

void ComplexGeneratorLaneModeButton::draw(const DrawArgs &args) {
	int mode = laneControl ? laneControl->mode() : 0;
	NVGcolor fill = nvgRGB(18, 37, 47);
	if (mode == 1)
		fill = nvgRGB(32, 78, 58);
	else if (mode == 2)
		fill = nvgRGB(76, 52, 92);
	else if (mode == 3)
		fill = nvgRGB(84, 72, 32);

	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
	nvgFillColor(args.vg, fill);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 0.8f);
	nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_LIGHT_GREEN);
	nvgStroke(args.vg);

	nvgFontSize(args.vg, 4.5f);
	nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(args.vg, nvgRGB(230, 230, 220));
	const char* label = mode == 0 ? "A" : mode == 1 ? "XY" : mode == 2 ? "P" : "D";
	nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.58f, label, nullptr);
}

struct ComputerscareComplexGeneratorWidget : ModuleWidget {
	ComputerscareComplexGeneratorWidget(ComputerscareComplexGenerator *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareComplexGeneratorPanel.svg")));
		box.size = Vec(8 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareComplexGeneratorPanel.svg")));
			addChild(panel);
		}
		channelWidget = new CompolyLaneCountWidget(Vec(92, 4), module, ComputerscareComplexGenerator::COMPOLY_CHANNELS,&module->polyChannels,false);
	

		//addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module, ComputerscareComplexGenerator::POLY_OUTPUT));

		addChild(channelWidget);

		cpx::CompolyPortsWidget* mainOutput = new cpx::CompolyPortsWidget(Vec(60, 350),module,ComputerscareComplexGenerator::COMPOLY_MAIN_OUT_A,ComputerscareComplexGenerator::MAIN_OUTPUT_MODE,0.6);
    addChild(mainOutput);



    cpx::ComplexXY* offsetValAB = new cpx::ComplexXY(module,ComputerscareComplexGenerator::OFFSET_VAL_AB);
    offsetValAB->box.size=Vec(25,25);
    offsetValAB->box.pos=Vec(38, 27);
    addChild(offsetValAB);

    cpx::ComplexXY* scaleValAB = new cpx::ComplexXY(module,ComputerscareComplexGenerator::SCALE_VAL_AB);
    scaleValAB->box.size=Vec(25,25);
    scaleValAB->box.pos=Vec(5, 27);
    addChild(scaleValAB);

		addSmallLabel("scale", 3, 17, 12);
		addSmallLabel("offset", 36, 17, 12);

		//addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module, ComputerscareComplexGenerator::GLOBAL_SCALE));
		//addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscareComplexGenerator::GLOBAL_OFFSET));


		float xx;
		float yy;
		float yInitial = 56;
		float ySpacing =  34;
		float yRightColumnOffset = 14.3 / 8;
		for (int i = 0; i < numComplexGeneratorKnobs; i++) {
			xx = 1.4f + 32.3 * (i - i % 8) / 8;
			yy = yInitial + ySpacing * (i % 8) +  yRightColumnOffset * (i - i % 8) ;
			addLabeledKnob(std::to_string(i + 1), xx, yy, module, i*2, (i - i % 8) * 1.2 - 3 + (i == 8 ? 5 : 0), 2);
		}

	}
	void addSmallLabel(std::string label, int x, int y, float fontSize) {
		SmallLetterDisplay* labelDisplay = new SmallLetterDisplay();
		labelDisplay->box.size = Vec(25, 10);
		labelDisplay->box.pos = Vec(x, y);
		labelDisplay->fontSize = fontSize;
		labelDisplay->value = label;
		labelDisplay->letterSpacing = 0.2f;
		labelDisplay->textAlign = 1;
		addChild(labelDisplay);
	}

	void addLabeledKnob(std::string label, int x, int y, ComputerscareComplexGenerator *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 18;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

	/*	ParamWidget* pob =  createParam<DisableableSmoothKnob>(Vec(x, y), module, ComputerscareComplexGenerator::KNOB + index);

		DisableableSmoothKnob* fader = dynamic_cast<DisableableSmoothKnob*>(pob);

		fader->module = module;
		fader->channel = index;

		addParam(fader);*/

		ComplexGeneratorLaneControl* control = new ComplexGeneratorLaneControl(
			module, index / 2, ComputerscareComplexGenerator::COMPLEX_XY + index);
		control->box = Rect(Vec(x, y), Vec(25, 25));
		addChild(control);

		ComplexGeneratorLaneModeButton* modeButton = new ComplexGeneratorLaneModeButton();
		modeButton->box = Rect(Vec(x + labelDx, y - 3 + labelDy), Vec(7.f, 7.f));
		modeButton->laneControl = control;
		addChild(modeButton);

		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	CompolyLaneCountWidget* channelWidget;
	PolyChannelsDisplay* channelDisplay;
	DisableableSmoothKnob* fader;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareComplexGenerator = createModel<ComputerscareComplexGenerator, ComputerscareComplexGeneratorWidget>("computerscare-complex-generator");
