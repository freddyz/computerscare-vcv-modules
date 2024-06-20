#include "Computerscare.hpp"
#include "golyFunctions.hpp"
#include "ComputerscareResizableHandle.hpp"

#include <string>
#include <sstream>
#include <iomanip>

const int NUM_LINES = 16;

struct ComputerscareDebug;

std::string noModuleStringValue = "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n";


struct ComputerscareDebug : ComputerscareMenuParamModule {
	enum ParamIds {
		MANUAL_TRIGGER,
		MANUAL_CLEAR_TRIGGER,
		CLOCK_CHANNEL_FOCUS,
		INPUT_CHANNEL_FOCUS,
		SWITCH_VIEW,
		WHICH_CLOCK,
		COLOR,
		DRAW_MODE,
		TEXT_MODE,
		NUM_PARAMS
	};
	enum InputIds {
		VAL_INPUT,
		TRG_INPUT,
		CLR_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	std::vector<std::string> drawModes = {"Off","Horizontal Bars", "Dots", "Arrows", "Connected Arrows", "Horse"};
	std::vector<std::string> textModes= {"Off","Poly List","Complex Rect","Complex Polar"};


	std::string defaultStrValue = "+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n+0.000000\n";
	std::string strValue = "0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n";

	float logLines[NUM_LINES] = {0.f};

	int lineCounter = 0;

	bool showValues = true;

	int clockChannel = 0;
	int inputChannel = 0;

	int clockMode = 1;
	int inputMode = 2;

	int outputRangeEnum = 5;

	int numOutputChannels = 16;


	float outputRanges[8][2];

	int stepCounter;
	dsp::SchmittTrigger clockTriggers[NUM_LINES];
	dsp::SchmittTrigger clearTrigger;
	dsp::SchmittTrigger manualClockTrigger;
	dsp::SchmittTrigger manualClearTrigger;

	enum clockAndInputModes {
		SINGLE_MODE,
		INTERNAL_MODE,
		POLY_MODE
	};

	ComputerscareDebug() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configButton(MANUAL_TRIGGER, "Manual Trigger");
		configButton(MANUAL_CLEAR_TRIGGER, "Reset/Clear");
		configSwitch(SWITCH_VIEW, 0.0f, 2.0f, 2.0f, "Input Mode", {"Single-Channel", "Internal", "Polyphonic"});
		configSwitch(WHICH_CLOCK, 0.0f, 2.0f, 1.0f, "Clock Mode", {"Single-Channel", "Internal", "Polyphonic"});
		configParam(CLOCK_CHANNEL_FOCUS, 0.f, 15.f, 0.f, "Clock Channel Selector");
		configParam(INPUT_CHANNEL_FOCUS, 0.f, 15.f, 0.f, "Input Channel Selector");

		configParam(COLOR, 0.f, 15.f, 0.f, "Color");

		configMenuParam(DRAW_MODE, 0.f, "Draw Mode", drawModes);
		configMenuParam(TEXT_MODE, 1.f, "Text Mode", textModes);

		configInput(VAL_INPUT, "Value");
		configInput(TRG_INPUT, "Clock");
		configInput(CLR_INPUT, "Reset");
		configOutput(POLY_OUTPUT, "Main");



		outputRanges[0][0] = 0.f;
		outputRanges[0][1] = 10.f;
		outputRanges[1][0] = -5.f;
		outputRanges[1][1] = 5.f;
		outputRanges[2][0] = 0.f;
		outputRanges[2][1] = 5.f;
		outputRanges[3][0] = 0.f;
		outputRanges[3][1] = 1.f;
		outputRanges[4][0] = -1.f;
		outputRanges[4][1] = 1.f;
		outputRanges[5][0] = -10.f;
		outputRanges[5][1] = 10.f;
		outputRanges[6][0] = -2.f;
		outputRanges[6][1] = 2.f;
		outputRanges[7][0] = 0.f;
		outputRanges[7][1] = 2.f;

		stepCounter = 0;

		getParamQuantity(SWITCH_VIEW)->randomizeEnabled = false;
		getParamQuantity(WHICH_CLOCK)->randomizeEnabled = false;
		getParamQuantity(CLOCK_CHANNEL_FOCUS)->randomizeEnabled = false;
		getParamQuantity(INPUT_CHANNEL_FOCUS)->randomizeEnabled = false;

		getParamQuantity(DRAW_MODE)->randomizeEnabled = false;
		getParamQuantity(TEXT_MODE)->randomizeEnabled = false;

		randomizeStorage();
	}

	void process(const ProcessArgs &args) override;

	void onRandomize() override {
		randomizeStorage();
	}

	void randomizeStorage() {
		float min = outputRanges[outputRangeEnum][0];
		float max = outputRanges[outputRangeEnum][1];
		float spread = max - min;
		for (int i = 0; i < 16; i++) {
			logLines[i] = min + spread * random::uniform();
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "outputRange", json_integer(outputRangeEnum));

		json_t *sequencesJ = json_array();

		for (int i = 0; i < 16; i++) {
			json_t *sequenceJ = json_real(logLines[i]);
			json_array_append_new(sequencesJ, sequenceJ);
		}
		json_object_set_new(rootJ, "lines", sequencesJ);
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		float val;

		json_t *outputRangeEnumJ = json_object_get(rootJ, "outputRange");
		if (outputRangeEnumJ) { outputRangeEnum = json_integer_value(outputRangeEnumJ); }

		json_t *sequencesJ = json_object_get(rootJ, "lines");

		if (sequencesJ) {
			for (int i = 0; i < 16; i++) {
				json_t *sequenceJ = json_array_get(sequencesJ, i);
				if (sequenceJ)
					val = json_real_value(sequenceJ);
				logLines[i] = val;
			}
		}

	}
	int setChannelCount() {
		clockMode = floor(params[WHICH_CLOCK].getValue());

		inputMode = floor(params[SWITCH_VIEW].getValue());


		int numInputChannels = inputs[VAL_INPUT].getChannels();
		int numClockChannels = inputs[TRG_INPUT].getChannels();

		bool inputConnected = inputs[VAL_INPUT].isConnected();
		bool clockConnected = inputs[TRG_INPUT].isConnected();

		bool noConnection = !inputConnected && !clockConnected;

		int numOutputChannels = 16;

		if (!noConnection) {

			if (clockMode == SINGLE_MODE) {
				if (inputMode == POLY_MODE) {
					numOutputChannels = numInputChannels;
				}
			}
			else if (clockMode == INTERNAL_MODE) {
				if (inputMode == POLY_MODE) {
					numOutputChannels = numInputChannels;
					for (int i = 0; i < 16; i++) {
						logLines[i] = inputs[VAL_INPUT].getVoltage(i);
					}
				}
			}
			else if (clockMode == POLY_MODE) {
				if (inputMode == POLY_MODE) {
					numOutputChannels = std::min(numInputChannels, numClockChannels);
				}
				else if (inputMode == SINGLE_MODE) {
					numOutputChannels = numClockChannels;
				}
				else if (inputMode == INTERNAL_MODE) {
					numOutputChannels = numClockChannels;
				}

			}
		}
		outputs[POLY_OUTPUT].setChannels(numOutputChannels);

		return numOutputChannels;
	}
};

void ComputerscareDebug::process(const ProcessArgs &args) {
	std::string thisVal;

	clockMode = floor(params[WHICH_CLOCK].getValue());

	inputMode = floor(params[SWITCH_VIEW].getValue());

	inputChannel = floor(params[INPUT_CHANNEL_FOCUS].getValue());
	clockChannel = floor(params[CLOCK_CHANNEL_FOCUS].getValue());

	bool inputConnected = inputs[VAL_INPUT].isConnected();

	float min = outputRanges[outputRangeEnum][0];
	float max = outputRanges[outputRangeEnum][1];
	float spread = max - min;

	if (clockMode == SINGLE_MODE) {
		if (clockTriggers[clockChannel].process(inputs[TRG_INPUT].getVoltage(clockChannel) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].getValue()) ) {
			if (inputMode == POLY_MODE) {
				for (int i = 0; i < 16; i++) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
			}
			else if (inputMode == SINGLE_MODE) {
				for ( unsigned int a = NUM_LINES - 1; a > 0; a = a - 1 )
				{
					logLines[a] = logLines[a - 1];
				}


				logLines[0] = inputs[VAL_INPUT].getVoltage(inputChannel);
			}
			else if (inputMode == INTERNAL_MODE) {
				for (int i = 0; i < 16; i++) {
					logLines[i] = min + spread * random::uniform();
				}
			}
		}
	}
	else if (clockMode == INTERNAL_MODE) {
		if (inputConnected) {
			if (inputMode == POLY_MODE) {
				for (int i = 0; i < 16; i++) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
			}
			else if (inputMode == SINGLE_MODE) {
				logLines[inputChannel] = inputs[VAL_INPUT].getVoltage(inputChannel);
			}
		}
		if (inputMode == INTERNAL_MODE) {
			for (int i = 0; i < 16; i++) {
				logLines[i] = min + spread * random::uniform();
			}
		}
	}
	else if (clockMode == POLY_MODE) {
		if (inputMode == POLY_MODE) {
			for (int i = 0; i < 16; i++) {
				if (clockTriggers[i].process(inputs[TRG_INPUT].getVoltage(i) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].getValue()) ) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(i);
				}
			}
		}
		else if (inputMode == SINGLE_MODE) {
			for (int i = 0; i < 16; i++) {
				if (clockTriggers[i].process(inputs[TRG_INPUT].getVoltage(i) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].getValue()) ) {
					logLines[i] = inputs[VAL_INPUT].getVoltage(inputChannel);
				}
			}
		}
		else if (inputMode == INTERNAL_MODE) {
			for (int i = 0; i < 16; i++) {
				if (clockTriggers[i].process(inputs[TRG_INPUT].getVoltage(i) / 2.f) || manualClockTrigger.process(params[MANUAL_TRIGGER].getValue()) ) {
					logLines[i] = min + spread * random::uniform();
				}

			}
		}
	}

	if (clearTrigger.process(inputs[CLR_INPUT].getVoltage() / 2.f) || manualClearTrigger.process(params[MANUAL_CLEAR_TRIGGER].getValue())) {
		for ( unsigned int a = 0; a < NUM_LINES; a++ )
		{
			logLines[a] = 0;
		}
		strValue = defaultStrValue;
	}

	numOutputChannels = setChannelCount();

	stepCounter++;

	if (stepCounter > 1025) {
		stepCounter = 0;

		thisVal = "";
		std::string thisLine = "";
		for ( unsigned int a = 0; a < NUM_LINES; a = a + 1 )
		{

			if (a < numOutputChannels) {
				thisLine = logLines[a] >= 0 ? "+" : "";
				thisLine += std::to_string(logLines[a]);
				thisLine = thisLine.substr(0, 9);
			}
			else {
				thisLine = "";
			}

			thisVal += (a > 0 ? "\n" : "") + thisLine;

			outputs[POLY_OUTPUT].setVoltage(logLines[a], a);
		}
		strValue = thisVal;
	}
}
struct DebugViz : TransparentWidget {
	ComputerscareDebug *module;

	DebugViz() {

	}
	void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
		int drawMode = 1;
		if(module) {
			drawMode=module->params[ComputerscareDebug::DRAW_MODE].getValue();
		}
	
			if (layer == 1) {
				if(drawMode == 1) {

					float valsToDraw[16] = {1.f};
					float colorsToDraw[16] = {1.f};

					float lengthsToDraw[16] = {1.f};
					int numChannelsToDraw = 16;
					float colorArg;
					float ceilVal=35.f;
					float floorVal = -ceilVal;


					if (module) {
						numChannelsToDraw = module->numOutputChannels;
						colorArg = module->params[ComputerscareDebug::COLOR].getValue();


						for (int i = 0; i < numChannelsToDraw; i++) {
							//valsToDraw[i] = module->goly.currentValues[i];
							valsToDraw[i]=module->logLines[i];
							colorsToDraw[i]=module->logLines[i]/3;
							lengthsToDraw[i]=std::max(floorVal,std::min(5*module->logLines[i],ceilVal));
						}
					}
					else {
						for (int i = 0; i < numChannelsToDraw; i++) {
							float rr = 10-20*random::uniform();
							lengthsToDraw[i] = clamp(rr*5,floorVal,ceilVal);
							colorsToDraw[i]=rr/3;
						}
						colorArg = random::uniform() * 2;
					}
					DrawHelper draw = DrawHelper(args.vg);
					Points pts = Points();

					nvgTranslate(args.vg, box.size.x / 2, box.size.y / 2 + 5);
					pts.linear(16, Vec(0, -box.size.y / 2), Vec(0, 240));
					std::vector<Vec> rThetaVec;
					std::vector<NVGcolor> colors;
					std::vector<Vec> thicknesses;

					for (int i = 0; i < 16; i++) {
						rThetaVec.push_back(Vec(lengthsToDraw[i], 0.f));

						colors.push_back(draw.sincolor(-colorsToDraw[i]/5, {2.2, 1.1, 1.3}));

						thicknesses.push_back(Vec(260 / (17.f), 0));
					}
					draw.drawLines(pts.get(), rThetaVec, colors, thicknesses);
			} else if(drawMode==2) {
					//draw as dots, assuming [x0,y0,x1,y1,...]
					float xx[16] = {};
					float yy[16] = {};
					float colorsToDraw[16] = {1.f};

					int numChannelsToDraw = 16;
					float colorArg;
					float ceilVal=35.f;
					float floorVal = -ceilVal;

					Points pts = Points();

					if (module) {
						numChannelsToDraw = module->numOutputChannels;
						colorArg = module->params[ComputerscareDebug::COLOR].getValue();

						for (int i = 0; i < 8; i++) {
								xx[i]=module->logLines[2*i];
								yy[i]=module->logLines[2*i+1];
								colorsToDraw[i]=module->logLines[2*i]/3;
						}
					}
					else {
						for (int i = 0; i < 8; i++) {
						//float moduleChannelVal=random::uniform() * 10;
								xx[i]=-10+random::uniform() * 20;
								yy[i]=-10+random::uniform() * 20;
								colorsToDraw[i]=-10+random::uniform() * 20;
						}
						//colorArg = random::uniform() * 2;
					}
					DrawHelper draw = DrawHelper(args.vg);
					

					nvgTranslate(args.vg, box.size.x / 2, box.size.y / 3 + 5);


					for(int i = 0; i < 8; i++) {
						float scale = 2;
						float x = clamp(3*xx[i],-30.f,30.f);
						float y = clamp(6*yy[i],-160.f,160.f);
						pts.addPoint(x,y);
					}
					


					std::vector<Vec> polyVals;
					std::vector<NVGcolor> colors;
					std::vector<Vec> thicknesses;

					for (int i = 0; i < 16; i++) {
						//polyVals.push_back(Vec(lengthsToDraw[i], 0.f));

						colors.push_back(draw.sincolor(-colorsToDraw[i]/3, {1, 1, 2}));

					//	thicknesses.push_back(Vec(260 / (1 + ch), 0));
					}
					draw.drawDots(pts.get(),colors,5);
				
			}
		}
	}
};
struct HidableSmallSnapKnob : SmallSnapKnob {
	bool visible = true;
	int hackIndex = 0;
	ComputerscareDebug *module;

	HidableSmallSnapKnob() {
		SmallSnapKnob();
	}
	void draw(const DrawArgs &args) override {
		if (module ? (hackIndex == 0 ? module->clockMode == 0 : module->inputMode == 0) : true) {
			Widget::draw(args);
		}
	};
};
////////////////////////////////////
struct VerticalListOfNumbers : Widget {

	std::string value;
	std::string fontPath = "res/fonts/RobotoMono-Regular.ttf";
	ComputerscareDebug * module;

	VerticalListOfNumbers() {
	};

	std::string makeTextList(int textMode) {
		std::string thisVal = "";
		std::string thisLine = "";
		int numOutputChannels = floor(random::uniform()*15 + 1);

		if(module) {
			numOutputChannels = module->numOutputChannels;
		}

		if(textMode==1) {
			//list of 16 polyphonic floats
			for ( unsigned int ch = 0; ch < NUM_LINES; ch++ )
			{
				if (ch < numOutputChannels) {
					float val = 0.f;
					if(module) {
						val = module->logLines[ch];
					} else {
						val = 10-20*random::uniform();
					}
					thisLine = val >= 0 ? "+" : "";
					thisLine += std::to_string(val);
					thisLine = thisLine.substr(0, 9);
				}
				else {
					thisLine = "";
				}

				thisVal += (ch > 0 ? "\n" : "") + thisLine;
			}
		}
		else if(textMode==2) {
			//complex rect
			for ( unsigned int ch = 0; ch < NUM_LINES; ch+=2 )
			{
				if (ch < numOutputChannels) {
					float re = 0.f;
					float im = 0.f;
					if(module) {
						re = module->logLines[ch];
						im = module->logLines[ch+1];
					} else {
						re = 10-20*random::uniform();
						im = 10-20*random::uniform();
					}

					thisLine=std::to_string(re).substr(0, 4);
					thisLine+=im >=0 ? "+" : "";
					thisLine+=std::to_string(im).substr(0, im < 0 ? 5 : 4)+"i";
				}
				else {
					thisLine = "";
				}

				thisVal += (ch > 0 ? "\n" : "") + thisLine;
			}
		}
		return thisVal;
	}

	void draw(const DrawArgs &ctx) override
	{
		// Background
		NVGcolor backgroundColor = nvgRGB(0x10, 0x00, 0x10);
		NVGcolor StrokeColor = nvgRGB(0xC0, 0xC7, 0xDE);
		nvgBeginPath(ctx.vg);
		nvgRoundedRect(ctx.vg, -1.0, -1.0, box.size.x + 2, box.size.y + 2, 4.0);
		nvgFillColor(ctx.vg, StrokeColor);
		nvgFill(ctx.vg);
		nvgBeginPath(ctx.vg);
		nvgRoundedRect(ctx.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
		nvgFillColor(ctx.vg, backgroundColor);
		nvgFill(ctx.vg);
	}
	void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
		if (layer == 1) {

			int textMode = module ? module->params[ComputerscareDebug::TEXT_MODE].getValue() : 1;
			std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontPath));

			if(textMode!=0) {
				float fontSize = 14.f;
				nvgFontSize(args.vg, fontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 0.8f);
				nvgTextLineHeight(args.vg, 1.08f);

				std::string textToDraw = this->makeTextList(textMode);
				Vec textPos = Vec(2.0f, 11.0f);
				NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
				nvgFillColor(args.vg, textColor);
				nvgTextBox(args.vg, textPos.x, textPos.y, 80, textToDraw.c_str(), NULL);
			}
		}
		Widget::drawLayer(args, layer);
	}
};
struct ConnectedSmallLetter : SmallLetterDisplay {
	ComputerscareDebug *module;
	int index;
	ConnectedSmallLetter(int dex) {
		index = dex;
		value = std::to_string(dex + 1);
	}
	void draw(const DrawArgs &ctx) override {
		if (module) {
			int cm = module->clockMode;
			int im = module->inputMode;
			int cc = module->clockChannel;
			int ic = module->inputChannel;

			// both:pink
			// clock: green
			// input:yellow
			baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
			if (cm == 0 && im == 0 && cc == index && ic == index)  {
				baseColor = COLOR_COMPUTERSCARE_PINK;
			}
			else {
				if (cm == 0 && cc == index) {
					baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
				}
				if (im == 0 && ic == index) {
					baseColor = COLOR_COMPUTERSCARE_YELLOW;
				}
			}
		}
		SmallLetterDisplay::draw(ctx);
	}
};
struct ComputerscareDebugWidget : ModuleWidget {
//int drawMode = 0;
	bool showValues = true;
	int textMode = 0;

	ComputerscareDebugWidget(ComputerscareDebug *module) {
		setModule(module);
		if(module) {
		//	drawMode = module->params[ComputerscareDebug::DRAW_MODE].getValue();
			textMode=module->params[ComputerscareDebug::TEXT_MODE].getValue();
		}
		
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareDebugPanel.svg")));

		DebugViz *display = new DebugViz();
		display->module = module;
		display->box.pos = Vec(6, 36);
		display->box.size = Vec(box.size.x, 400);
		addChild(display);

		addInput(createInput<InPort>(Vec(2, 335), module, ComputerscareDebug::TRG_INPUT));
		addInput(createInput<InPort>(Vec(61, 335),  module, ComputerscareDebug::VAL_INPUT));
		addInput(createInput<InPort>(Vec(31, 335), module, ComputerscareDebug::CLR_INPUT));

		addParam(createParam<ComputerscareClockButton>(Vec(2, 321), module, ComputerscareDebug::MANUAL_TRIGGER));

		addParam(createParam<ComputerscareResetButton>(Vec(32, 320), module, ComputerscareDebug::MANUAL_CLEAR_TRIGGER));

		addParam(createParam<ThreeVerticalXSwitch>(Vec(2, 279), module, ComputerscareDebug::WHICH_CLOCK));
		addParam(createParam<ThreeVerticalXSwitch>(Vec(66, 279), module, ComputerscareDebug::SWITCH_VIEW));

		HidableSmallSnapKnob *clockKnob = createParam<HidableSmallSnapKnob>(Vec(6, 305), module, ComputerscareDebug::CLOCK_CHANNEL_FOCUS);
		clockKnob->module = module;
		clockKnob->hackIndex = 0;
		addParam(clockKnob);

		HidableSmallSnapKnob *inputKnob = createParam<HidableSmallSnapKnob>(Vec(66, 305), module, ComputerscareDebug::INPUT_CHANNEL_FOCUS);
		inputKnob->module = module;
		inputKnob->hackIndex = 1;
		addParam(inputKnob);

		addOutput(createOutput<OutPort>(Vec(56, 1), module, ComputerscareDebug::POLY_OUTPUT));

		//channel number labels
		for (int i = 0; i < 16; i++) {
			ConnectedSmallLetter *sld = new ConnectedSmallLetter(i);
			sld->fontSize = 15;
			sld->textAlign = 1;
			sld->box.pos = Vec(-4, 33.8 + 15.08 * i);
			sld->box.size = Vec(28, 20);
			sld->module = module;
			addChild(sld);
		}
		
		addLabeledKnob<ScrambleSnapKnob>("Algo", 4, 324, module, ComputerscareDebug::DRAW_MODE, 0, 0, true);
		addLabeledKnob<ScrambleSnapKnob>("Text", 64, 316, module, ComputerscareDebug::TEXT_MODE, 0, 0, true);

		VerticalListOfNumbers *stringDisplay = createWidget<VerticalListOfNumbers>(Vec(15, 34));
		stringDisplay->box.size = Vec(73, 245);
		stringDisplay->module = module;
		addChild(stringDisplay);

		ComputerscareResizeHandle *leftHandle = new ComputerscareResizeHandle();

		ComputerscareResizeHandle *rightHandle = new ComputerscareResizeHandle();
		rightHandle->right = true;
		this->rightHandle = rightHandle;
		addChild(leftHandle);
		addChild(rightHandle);

		debug = module;
	}

template <typename BASE>
void addLabeledKnob(std::string label, int x, int y, ComputerscareDebug *module, int paramIndex, float labelDx, float labelDy, bool snap = false) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 14;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		if (snap) {
			addParam(createParam<BASE>(Vec(x, y), module, paramIndex));
		}
		else {
			addParam(createParam<BASE>(Vec(x, y), module, paramIndex));

		}
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);

	}

	void appendContextMenu(Menu *menu) override;

	ParamSelectMenu *drawModeMenu;
	ParamSelectMenu *textModeMenu;
	

	ComputerscareResizeHandle *leftHandle;
	ComputerscareResizeHandle *rightHandle;

	SmallLetterDisplay* smallLetterDisplay;
	ComputerscareDebug *debug;
};
struct DebugOutputRangeItem : MenuItem {
	ComputerscareDebug *debug;
	int outputRangeEnum;
	void onAction(const event::Action &e) override {
		debug->outputRangeEnum = outputRangeEnum;
		printf("outputRangeEnum %i\n", outputRangeEnum);
	}
	void step() override {
		rightText = CHECKMARK(debug->outputRangeEnum == outputRangeEnum);
		MenuItem::step();
	}
};
void ComputerscareDebugWidget::appendContextMenu(Menu *menu)
{
	ComputerscareDebug *debug = dynamic_cast<ComputerscareDebug *>(this->module);

	//spacer
	menu->addChild(new MenuEntry);

  drawModeMenu = new ParamSelectMenu();
  drawModeMenu->text = "Draw Mode";
  drawModeMenu->param = debug->paramQuantities[ComputerscareDebug::DRAW_MODE];
  drawModeMenu->options = debug->drawModes;
  menu->addChild(drawModeMenu);

  textModeMenu = new ParamSelectMenu();
  textModeMenu->text = "Text Mode";
  textModeMenu->param = debug->paramQuantities[ComputerscareDebug::TEXT_MODE];
  textModeMenu->options = debug->textModes;
  menu->addChild(textModeMenu);

	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	menu->addChild(construct<MenuLabel>());
	menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Random Generator Range (Internal In)"));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, "  0v ... +10v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 0));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, " -5v ...  +5v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 1));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, "  0v ...  +5v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 2));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, "  0v ...  +1v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 3));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, " -1v ...  +1v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 4));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, "-10v ... +10v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 5));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, " -2v ...  +2v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 6));
	menu->addChild(construct<DebugOutputRangeItem>(&MenuItem::text, "  0v ...  +2v", &DebugOutputRangeItem::debug, debug, &DebugOutputRangeItem::outputRangeEnum, 7));

}
Model *modelComputerscareDebug = createModel<ComputerscareDebug, ComputerscareDebugWidget>("computerscare-debug");
