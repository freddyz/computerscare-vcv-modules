#include "Computerscare.hpp"
#include "complex/ComplexWidgets.hpp"
#include "complex/math/ComplexMath.hpp"

#include <array>

struct ComputerscareComplexTransformer;




struct ComputerscareComplexTransformer : ComputerscareComplexBase {
	ComputerscareSVGPanel* panelRef;
	int compolyChannelsMainOutput = 0;

	enum ParamIds {

		COMPOLY_CHANNELS,
		
		SCALE_VAL_AB,
		SCALE_TRIM_AB = SCALE_VAL_AB+2,

		OFFSET_VAL_AB = SCALE_TRIM_AB+2,
		OFFSET_TRIM_AB = OFFSET_VAL_AB+2,
		
		Z_INPUT_MODE,
		W_INPUT_MODE,
		A_INPUT_MODE,
		B_INPUT_MODE,
		C_INPUT_MODE,
		MAIN_OUTPUT_MODE,
		PRODUCT_OUTPUT_MODE,
		NUM_PARAMS
	};
	enum InputIds {
		Z_INPUT,
		W_INPUT = Z_INPUT+2,
		A_INPUT= W_INPUT+2,
		B_INPUT= A_INPUT+2,
		C_INPUT= B_INPUT+2,
		NUM_INPUTS=C_INPUT+2,
	};
	enum OutputIds {
		COMPOLY_MAIN_OUT_A,
		COMPOLY_MAIN_OUT_B,
		COMPOLY_PRODUCT_OUT_A,
		COMPOLY_PRODUCT_OUT_B,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};




	ComputerscareComplexTransformer()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);


		configParam(SCALE_VAL_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(SCALE_VAL_AB + 1, -10.f, 10.f, 0.f, "Channel ");
		getParamQuantity(SCALE_VAL_AB)->randomizeEnabled = false;
		getParamQuantity(SCALE_VAL_AB+1)->randomizeEnabled = false;

		configParam(OFFSET_VAL_AB    , -10.f, 10.f, 0.f, "Channel " );
		configParam(OFFSET_VAL_AB + 1, -10.f, 10.f, 0.f, "Channel ");
		getParamQuantity(OFFSET_VAL_AB)->randomizeEnabled = false;
		getParamQuantity(OFFSET_VAL_AB+1)->randomizeEnabled = false;


		configParam<AutoParamQuantity>(COMPOLY_CHANNELS, 0.f, 16.f, 0.f, "Compoly Output Channels");
		getParamQuantity(COMPOLY_CHANNELS)->randomizeEnabled = false;
		getParamQuantity(COMPOLY_CHANNELS)->resetEnabled = false;

		configParam<cpx::CompolyModeParam>(MAIN_OUTPUT_MODE,0.f,3.f,0.f,"Main Output Mode");
		configParam<cpx::CompolyModeParam>(PRODUCT_OUTPUT_MODE,0.f,3.f,0.f,"Product Output Mode");
		
		configParam<cpx::CompolyModeParam>(Z_INPUT_MODE,0.f,3.f,0.f,"z Input Mode");
		configParam<cpx::CompolyModeParam>(W_INPUT_MODE,0.f,3.f,0.f,"w Input Mode");
		configParam<cpx::CompolyModeParam>(A_INPUT_MODE,0.f,3.f,0.f,"a Input Mode");
		configParam<cpx::CompolyModeParam>(B_INPUT_MODE,0.f,3.f,0.f,"b Input Mode");
		configParam<cpx::CompolyModeParam>(C_INPUT_MODE,0.f,3.f,0.f,"c Input Mode");

		getParamQuantity(MAIN_OUTPUT_MODE)->randomizeEnabled = false;
		getParamQuantity(PRODUCT_OUTPUT_MODE)->randomizeEnabled = false;
		getParamQuantity(Z_INPUT_MODE)->randomizeEnabled = false;
		getParamQuantity(W_INPUT_MODE)->randomizeEnabled = false;
		getParamQuantity(A_INPUT_MODE)->randomizeEnabled = false;
		getParamQuantity(B_INPUT_MODE)->randomizeEnabled = false;
		getParamQuantity(C_INPUT_MODE)->randomizeEnabled = false;


		configInput<cpx::CompolyPortInfo<Z_INPUT_MODE,0>>(Z_INPUT, "z");
    configInput<cpx::CompolyPortInfo<Z_INPUT_MODE,1>>(Z_INPUT + 1, "z");

    configInput<cpx::CompolyPortInfo<W_INPUT_MODE,0>>(W_INPUT, "w");
    configInput<cpx::CompolyPortInfo<W_INPUT_MODE,1>>(W_INPUT + 1, "w");

    configInput<cpx::CompolyPortInfo<A_INPUT_MODE,0>>(A_INPUT, "a");
    configInput<cpx::CompolyPortInfo<A_INPUT_MODE,1>>(A_INPUT + 1, "a");


		configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE,0>>(COMPOLY_MAIN_OUT_A, "z+w");
    configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE,1>>(COMPOLY_MAIN_OUT_B, "z+w");

    configOutput<cpx::CompolyPortInfo<PRODUCT_OUTPUT_MODE,0>>(COMPOLY_PRODUCT_OUT_A, "z*w");
    configOutput<cpx::CompolyPortInfo<PRODUCT_OUTPUT_MODE,1>>(COMPOLY_PRODUCT_OUT_B, "z*w");

	}


	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();

		int compolyphonyKnobSetting = params[COMPOLY_CHANNELS].getValue();
		int zInputMode = params[Z_INPUT_MODE].getValue();
		int wInputMode = params[W_INPUT_MODE].getValue();

		// + 1%
		std::vector<std::vector <int>> inputCompolyphony = getInputCompolyphony({Z_INPUT_MODE},{Z_INPUT});

	//	std::vector<std::vector <int>> inputCompolyphony = getInputCompolyphony({Z_INPUT_MODE,W_INPUT_MODE,A_INPUT_MODE},{Z_INPUT,W_INPUT,A_INPUT});


		compolyChannelsMainOutput = calcOutputCompolyphony(compolyphonyKnobSetting,inputCompolyphony);
		int mainOutputMode = params[MAIN_OUTPUT_MODE].getValue();
		setOutputChannels(COMPOLY_MAIN_OUT_A,mainOutputMode,compolyChannelsMainOutput);

		int productOutputMode=params[PRODUCT_OUTPUT_MODE].getValue();
		setOutputChannels(COMPOLY_PRODUCT_OUT_A,productOutputMode,compolyChannelsMainOutput);

		float zx[16] = {};
		float zy[16] = {};

		float wx[16] = {};
		float wy[16] = {};

		float sumx[16] = {};
		float sumy[16] = {};

		float prodx[16] = {};
		float prody[16] = {};

		readInputToRect(Z_INPUT,zInputMode,zx,zy);
		readInputToRect(W_INPUT,wInputMode,wx,wy);

		for (uint8_t c = 0; c < 16; c += 4) {
			simd::float_4 zxv = simd::float_4::load(zx + c);
			simd::float_4 zyv = simd::float_4::load(zy + c);
			simd::float_4 wxv = simd::float_4::load(wx + c);
			simd::float_4 wyv = simd::float_4::load(wy + c);

			(zxv + wxv).store(sumx + c);
			(zyv + wyv).store(sumy + c);
			(zxv * wxv - zyv * wyv).store(prodx + c);
			(zxv * wyv + zyv * wxv).store(prody + c);
		}

		writeOutputFromRect(COMPOLY_MAIN_OUT_A,mainOutputMode,sumx,sumy);
		writeOutputFromRect(COMPOLY_PRODUCT_OUT_A,productOutputMode,prodx,prody);
	}

	int chMap1[16]  = {0,2,4,6,8,10,12,14,0,2,4,6,8,10,12,14};
	int chMap2[16]  = {1,3,5,7,9,11,13,15,1,3,5,7,9,11,13,15};

	void writeOutputFromRect(int firstPortIndex, int outputMode, float* x, float* y) {
		bool polar = outputMode == POLAR_SEPARATED || outputMode == POLAR_INTERLEAVED;
		bool interleaved = outputMode == RECT_INTERLEAVED || outputMode == POLAR_INTERLEAVED;

		if (polar) {
			float r[16] = {};
			float theta[16] = {};
			float thetaVoltage[16] = {};
			for (uint8_t c = 0; c < 16; c += 4) {
				simd::float_4 xv = simd::float_4::load(x + c);
				simd::float_4 yv = simd::float_4::load(y + c);
				simd::hypot(xv, yv).store(r + c);
				simd::atan2(yv, xv).store(theta + c);
			}
			for (uint8_t c = 0; c < 16; c++) {
				thetaVoltage[c] =
					cpx::complex_math::thetaRadiansToCableVoltage(theta[c]);
			}
			writeOutputVoltages(firstPortIndex, interleaved, r, thetaVoltage);
		}
		else {
			writeOutputVoltages(firstPortIndex, interleaved, x, y);
		}
	}

	void writeOutputVoltages(int firstPortIndex, bool interleaved, float* x, float* y) {
		float a[16] = {};
		float b[16] = {};

		if (interleaved) {
			for (uint8_t c = 0; c < 8; c++) {
				a[2 * c] = x[c];
				a[2 * c + 1] = y[c];
			}
			for (uint8_t c = 8; c < 16; c++) {
				b[(2 * c) % 16] = x[c];
				b[(2 * c + 1) % 16] = y[c];
			}
		}
		else {
			for (uint8_t c = 0; c < 16; c++) {
				a[c] = x[c];
				b[c] = y[c];
			}
		}

		outputs[firstPortIndex].writeVoltages(a);
		outputs[firstPortIndex+1].writeVoltages(b);
	}

	void readInputToRect(int firstPortIndex, int inputMode, float* x, float* y) {
		float a[16] = {};
		float b[16] = {};
		inputs[firstPortIndex].readVoltages(a);
		inputs[firstPortIndex+1].readVoltages(b);

		if (inputMode == RECT_SEPARATED) {
			for (uint8_t c = 0; c < 16; c += 4) {
				simd::float_4::load(a + c).store(x + c);
				simd::float_4::load(b + c).store(y + c);
			}
		}
		else if (inputMode == RECT_INTERLEAVED) {
			for (uint8_t c = 0; c < 8; c++) {
				x[c] = a[2 * c];
				y[c] = a[2 * c + 1];
			}
			for (uint8_t c = 8; c < 16; c++) {
				x[c] = b[(2 * c) % 16];
				y[c] = b[(2 * c + 1) % 16];
			}
		}
		else if (inputMode == POLAR_SEPARATED) {
			for (uint8_t c = 0; c < 16; c += 4) {
				simd::float_4 r = simd::float_4::load(a + c);
				simd::float_4 theta =
					simd::float_4::load(b + c) *
					cpx::complex_math::thetaRadiansPerVolt;
				(r * simd::cos(theta)).store(x + c);
				(r * simd::sin(theta)).store(y + c);
			}
		}
		else if (inputMode == POLAR_INTERLEAVED) {
			for (uint8_t c = 0; c < 16; c++) {
				float r = c < 8 ? a[2 * c] : b[(2 * c) % 16];
				float theta = cpx::complex_math::thetaCableVoltageToRadians(
					c < 8 ? a[2 * c + 1] : b[(2 * c + 1) % 16]);
				x[c] = r * std::cos(theta);
				y[c] = r * std::sin(theta);
			}
		}
	}

	json_t *dataToJson() override {
    json_t *rootJ = json_object();
    return rootJ;
  }

    void dataFromJson(json_t *rootJ) override {
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

struct ComputerscareComplexTransformerWidget : ModuleWidget {
	ComputerscareComplexTransformerWidget(ComputerscareComplexTransformer *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareComplexTransformerPanel.svg")));
		box.size = Vec(8 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareComplexTransformerPanel.svg")));
			addChild(panel);
		}
		channelWidget = new CompolyLaneCountWidget(Vec(92, 4), module, ComputerscareComplexTransformer::COMPOLY_CHANNELS,&module->compolyChannelsMainOutput);
	

		//addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module, ComputerscareComplexTransformer::POLY_OUTPUT));

		addChild(channelWidget);





    cpx::ComplexXY* offsetValAB = new cpx::ComplexXY(module,ComputerscareComplexTransformer::OFFSET_VAL_AB);
    offsetValAB->box.size=Vec(25,25);
    offsetValAB->box.pos=Vec(32, 27);
    addChild(offsetValAB);

    cpx::ComplexXY* scaleValAB = new cpx::ComplexXY(module,ComputerscareComplexTransformer::SCALE_VAL_AB);
    scaleValAB->box.size=Vec(25,25);
    scaleValAB->box.pos=Vec(5, 27);
    addChild(scaleValAB);


    cpx::CompolyPortsWidget* zInput = new cpx::CompolyPortsWidget(Vec(50, 80),module,ComputerscareComplexTransformer::Z_INPUT,ComputerscareComplexTransformer::Z_INPUT_MODE,0.8,false,"z");
    zInput->compolyLabel->box.pos = Vec(18,6);
    addChild(zInput);

    cpx::CompolyPortsWidget* wInput = new cpx::CompolyPortsWidget(Vec(50, 140),module,ComputerscareComplexTransformer::W_INPUT,ComputerscareComplexTransformer::W_INPUT_MODE,1.f,false,"w");
    wInput->compolyLabel->box.pos = Vec(15,6);
    addChild(wInput);

		//addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module, ComputerscareComplexTransformer::GLOBAL_SCALE));
		//addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module, ComputerscareComplexTransformer::GLOBAL_OFFSET));

    cpx::CompolyPortsWidget* mainOutput = new cpx::CompolyPortsWidget(Vec(50, 260),module,ComputerscareComplexTransformer::COMPOLY_MAIN_OUT_A,ComputerscareComplexTransformer::MAIN_OUTPUT_MODE,0.7f,true,"zplusw");
    mainOutput->compolyLabel->box.pos = Vec(0,10);
    addChild(mainOutput);

    cpx::CompolyPortsWidget* productOutput = new cpx::CompolyPortsWidget(Vec(50, 320),module,ComputerscareComplexTransformer::COMPOLY_PRODUCT_OUT_A,ComputerscareComplexTransformer::PRODUCT_OUTPUT_MODE,0.7f,true,"ztimesw");
    productOutput->compolyLabel->box.pos = Vec(0,10);
    addChild(productOutput);


	}

	CompolyLaneCountWidget* channelWidget;
	PolyChannelsDisplay* channelDisplay;
	DisableableSmoothKnob* fader;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareComplexTransformer = createModel<ComputerscareComplexTransformer, ComputerscareComplexTransformerWidget>("computerscare-complex-transformer");
