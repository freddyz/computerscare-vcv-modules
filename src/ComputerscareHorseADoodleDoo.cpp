#include "Computerscare.hpp"

#include "dtpulse.hpp"

struct ComputerscareHorseADoodleDoo;

struct HorseSequencer {
	float pattern = 0.f;
	int numSteps = 8;
	int currentStep = -1;
	float density = 0.5f;

	float pendingPattern = 0.f;
	int pendingNumSteps = 8;
	float pendingDensity = 0.5f;
	bool pendingChange = 0;
	bool forceChange = 0;



	int primes[16] = {30011, 36877, 26627, 32833, 66797, 95153, 66553, 84857, 32377, 79589, 25609, 20113, 70991, 86533, 21499, 32491};
	int otherPrimes[16] = {80651, 85237, 11813, 22343, 19543, 28027, 9203, 39521, 42853, 58411, 33811, 76771, 10939, 22721, 17851, 10163};
	int channel = 0;


	std::vector<std::vector<int>> octets = {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 1, 0, 0}, {0, 1, 0, 1}, {0, 1, 1, 0}, {0, 1, 1, 1}, {1, 0, 0, 0}, {1, 0, 0, 1}, {1, 0, 1, 0}, {1, 0, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 1}, {1, 1, 1, 0}, {1, 1, 1, 1}};
	std::vector<int> somethin = {1, 0, 0, 1};
	std::vector<int> absoluteSequence;
	std::vector<float> cvSequence;

	HorseSequencer() {

	}
	HorseSequencer(float patt, int steps, float dens, int ch) {
		numSteps = steps;
		density = dens;
		pattern = patt;
		channel = ch;
		makeAbsolute();
	}
	void makeAbsolute() {
		std::vector<int> newSeq;
		std::vector<float> newCV;

		newSeq.resize(0);
		newCV.resize(0);
		/*for (int i = 0; i < 16; i++) {
			int dex = ((int)std::floor(pattern * primes[i]) + otherPrimes[i]) % 16;

			thisOct = octets[dex];
			//vector1.insert( vector1.end(), vector2.begin(), vector2.end() );
			newSeq.insert(newSeq.end(), thisOct.begin(), thisOct.end());
			//absoluteSequence.push_back(dex < 8 ? 0 : 1);
		}*/


		float cvRange = std::sin(primes[9] * pattern - otherPrimes[3]);
		int cvRoot = 0;//std::floor(6*(1+std::sin(primes[5]*pattern-otherPrimes[2])));
		float trigConst = 2 * M_PI / ((float)numSteps);

		for (int i = 0; i < numSteps; i++) {
			float val = 0.f;
			float cvVal = 0.f;
			float arg = pattern + ((float) i) * trigConst;
			for (int k = 0; k < 4; k++) {
				val += std::sin(primes[((i + 1) * (k + 1)) % 16] * arg + otherPrimes[(otherPrimes[0] + i) % 16]);
				cvVal += std::sin(primes[((i + 11) * (k + 1) + 201) % 16] * arg + otherPrimes[(otherPrimes[3] + i - 7) % 16]);
				//cvVal+=i/12;
			}
			newSeq.push_back(val < (density - 0.5) * 4 * 2 ? 1 : 0);
			newCV.push_back(cvRoot + (cvVal + 4) / .8);
		}
		printVector(newSeq);
		absoluteSequence = newSeq;
		cvSequence = newCV;
	}
	void checkAndArm(float patt, int steps, float dens) {
		if (pattern != patt || numSteps != steps || density != dens) {
			pendingPattern = patt;
			pendingNumSteps = steps;
			pendingDensity = dens;
			pendingChange = true;
		}
	}
	void armChange() {
		forceChange = true;
	}
	void disarm() {
		pendingChange = false;
		pendingPattern = pattern;
		pendingNumSteps = numSteps;
		pendingDensity = density;
	}
	void change(float patt, int steps, float dens) {
		numSteps = steps;
		density = dens;
		pattern = patt;
		currentStep = 0;
		makeAbsolute();
	}
	void tick() {
		currentStep++;
		currentStep %= numSteps;
		if ((currentStep == 0 && pendingChange) || forceChange) {
			change(pendingPattern, pendingNumSteps, pendingDensity);
			pendingChange = false;
			forceChange = false;
			currentStep = 0;
		}
	}
	void reset() {
		currentStep = 0;
	}
	int get() {
		return absoluteSequence[currentStep];
	}
	float getCV() {
		return cvSequence[currentStep];
	}
	int tickAndGet() {
		tick();
		return get();
	}
};

struct ComputerscareHorseADoodleDoo : ComputerscarePolyModule {
	int counter = 0;
	ComputerscareSVGPanel* panelRef;
	float currentValues[16] = {0.f};
	bool atFirstStepPoly[16] = {false};
	enum ParamIds {
		PATTERN_KNOB,
		PATTERN_TRIM,
		STEPS_KNOB,
		STEPS_TRIM,
		DENSITY_KNOB,
		DENSITY_TRIM,
		WEIRDNESS_KNOB,
		WEIRDNESS_TRIM,
		POLY_KNOB,
		NUM_PARAMS

	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		PATTERN_CV,
		STEPS_CV,
		DENSITY_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIGGER_OUTPUT,
		EOC_OUTPUT,
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	rack::dsp::SchmittTrigger clockInputTrigger[16];
	rack::dsp::SchmittTrigger resetInputTrigger[16];

	rack::dsp::SchmittTrigger clockManualTrigger;
	rack::dsp::SchmittTrigger resetManualTrigger;

	float lastPatternKnob = 0.f;
	int lastStepsKnob = 2;
	float lastDensityKnob = 0.f;
	int lastPolyKnob = 0;



	int seqVal[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float cvVal[16] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

	int clockChannels[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	int resetChannels[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

	bool changePending[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

	HorseSequencer seq[16];

	ComputerscareHorseADoodleDoo()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(PATTERN_KNOB, 0.f, 10.f, 0.f, "Pattern");
		configParam(STEPS_KNOB, 2.f, 64.f, 8.f, "Number of Steps");
		configParam(DENSITY_KNOB, 0.f, 1.f, 0.5f, "Density", "%", 0, 100);

		configParam(PATTERN_TRIM, -1.f, 1.f, 0.f, "Pattern CV Trim");
		configParam(STEPS_TRIM, -1.f, 1.f, 0.f, "Steps CV Trim");
		configParam(DENSITY_TRIM, -1.f, 1.f, 0.f, "Density CV Trim");

		configParam(POLY_KNOB, 0.f, 16.f, 0.f, "Polyphony");


		for (int i = 0; i < 16; i++) {
			seq[i] = HorseSequencer(0.f, 8, 0.f, i);
		}


	}


	void checkKnobChanges() {

		int pattNum = inputs[PATTERN_CV].getChannels();
		int stepsNum = inputs[STEPS_CV].getChannels();
		int densityNum = inputs[DENSITY_CV].getChannels();

		int clockNum = inputs[CLOCK_INPUT].getChannels();
		int resetNum = inputs[RESET_INPUT].getChannels();


		lastStepsKnob = std::floor(params[STEPS_KNOB].getValue());
		lastPolyKnob = std::floor(params[POLY_KNOB].getValue());

		polyChannels = lastPolyKnob == 0 ? std::max(clockNum, std::max(pattNum, std::max(stepsNum, densityNum))) : lastPolyKnob;

		for (int i = 0; i < 16; i++) {
			clockChannels[i] = std::max(1, std::min(i, clockNum));
			resetChannels[i] = std::max(1, std::min(i, resetNum));
		}

		outputs[TRIGGER_OUTPUT].setChannels(polyChannels);
		outputs[CV_OUTPUT].setChannels(polyChannels);

		for (int i = 0; i < polyChannels; i++) {
			float patternVal = params[PATTERN_KNOB].getValue() + params[PATTERN_TRIM].getValue() * inputs[PATTERN_CV].getVoltage(fmin(i, pattNum));
			int stepsVal = std::floor(params[STEPS_KNOB].getValue() + params[STEPS_TRIM].getValue() * inputs[STEPS_CV].getVoltage(fmin(i, stepsNum)));
			float densityVal = params[DENSITY_KNOB].getValue() + params[DENSITY_TRIM].getValue() * inputs[DENSITY_CV].getVoltage(fmin(i, densityNum)) / 10;
			seq[i].checkAndArm(patternVal, stepsVal, densityVal);
		}
	}
	void processChannel(int ch, bool clocked, bool reset, bool clockInputHigh) {

		if (reset) {
			seq[ch].armChange();
			seqVal[ch] = seq[ch].get();
			if (seqVal[ch]) {
				cvVal[ch] = seq[ch].getCV();
			}
			atFirstStepPoly[ch] =  true;
		}

		if (clocked && !reset) {
			seqVal[ch] = seq[ch].tickAndGet();
			if (seqVal[ch]) {
				cvVal[ch] = seq[ch].getCV();
			}
			atFirstStepPoly[ch] =  (seq[ch].currentStep == 0);
			if (seqVal[ch]) {
				cvVal[ch] = seq[ch].getCV();
			}
			/*if (atFirstStepPoly[ch] && changePending[ch]) {
				applyChange(ch);
				seqVal[ch] = seq[ch].get();
				if (seqVal[ch]) {
					cvVal[ch] = seq[ch].getCV();
				}
			}*/

		}
		if (inputs[CLOCK_INPUT].isConnected()) {
			outputs[TRIGGER_OUTPUT].setVoltage((clockInputHigh && seqVal[ch] == 1) ? 10.0f : 0.0f, ch);
			//DEBUG("before output:%f",cvVal);
			outputs[CV_OUTPUT].setVoltage(cvVal[ch], ch);
			//outputs[EOC_OUTPUT].setVoltage((currentTriggerIsHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
		}
		else {

		}

		if (atFirstStepPoly[ch]) {
			outputs[EOC_OUTPUT].setVoltage((clockInputHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
		}
	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();

		/*counter++;
		if (counter > 601) {
			checkKnobChanges();
			counter = 0;
		}*/


		bool currentClock[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		bool currentReset[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		bool isHigh[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		for (int i = 0; i < 16; i++) {
			currentClock[i] = clockInputTrigger[i].process(inputs[CLOCK_INPUT].getVoltage(i));
			currentReset[i] = resetInputTrigger[i].process(inputs[RESET_INPUT].getVoltage(i));
			isHigh[i] = clockInputTrigger[i].isHigh();

		}
		for (int i = 0; i < 16; i++) {
			processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1]);
		}

	}
	void checkPoly() override {
			checkKnobChanges();
			//polyChannels = params[POLY_CHANNELS].getValue();
			//outputs[POLY_OUTPUT].setChannels(polyChannels);
	}
};

struct NumStepsOverKnobDisplay : SmallLetterDisplay
{
	ComputerscareHorseADoodleDoo *module;
	int knobConnection = 1;
	NumStepsOverKnobDisplay(int type)
	{
		letterSpacing = 1.f;
		knobConnection = type;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		if (module)
		{
			std::string str = "";
			if (knobConnection == 1) {
				str = std::to_string(module->lastStepsKnob);
			}
			else if (knobConnection == 2) {
				str = module->lastPolyKnob == 0 ? "A" : std::to_string(module->lastPolyKnob);
			}
			value = str;
		}
		SmallLetterDisplay::draw(args);
	}
};

struct HorseDisplay : TransparentWidget {
	ComputerscareHorseADoodleDoo *module;
	int ch = 0;

	HorseDisplay() {
	}

	void drawHorse(const DrawArgs &args, float x = 0.f) {

		float dy = 380 / (float)(module->seq[0].numSteps);
		float mid = module->seq[ch].numSteps / 2;

		float dh = 0.2;//multiplicitive on original height
		float zDistance = 20;




		for (int i = 0; i < module->seq[ch].numSteps; i++) {
			nvgBeginPath(args.vg);
			float xx = 65;
			float yy = i * dy;

			float ip = i / module->seq[ch].numSteps;
			float width = 15.f;
			float height = dy;

			if (module->seq[ch].absoluteSequence[i] == 1 || i == module->seq[ch].currentStep) {

				float xCloseTop = xx;
				float yCloseTop = yy;
				float xFarTop = xx - zDistance;
				float yFarTop = 100;
				float xFarBottom = xFarTop;
				float yFarBottom = yFarTop;
				float xCloseBottom = xCloseTop;
				float yCloseBottom = yCloseTop + height;


				// left side wall
				if (i == module->seq[ch].currentStep) {
					nvgFillColor(args.vg, COLOR_COMPUTERSCARE_BLUE );
				}
				else {
					nvgFillColor(args.vg, COLOR_COMPUTERSCARE_RED);
				}

				nvgStrokeColor(args.vg, BLACK);
				nvgStrokeWidth(args.vg, .2);


				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, xCloseTop, yCloseTop);
				nvgLineTo(args.vg, xFarTop, yFarTop);
				nvgLineTo(args.vg, xFarBottom, yFarBottom);
				nvgLineTo(args.vg, xCloseBottom, yCloseBottom);
				nvgLineTo(args.vg, xCloseTop, yCloseTop);
				nvgClosePath(args.vg);
				//nvgRect(args.vg, xx,yy,width,height);
				nvgFill(args.vg);
				nvgStroke(args.vg);



				//top
				nvgStrokeWidth(args.vg, 1.f);
				if (i == module->seq[ch].currentStep) {
					nvgFillColor(args.vg, COLOR_COMPUTERSCARE_YELLOW);
				}
				else {
					nvgFillColor(args.vg, nvgRGB(0xE2, 0x22, 0x12));
				}
				nvgBeginPath(args.vg);
				nvgRect(args.vg, xx, yy, 10.f, dy);
				nvgClosePath(args.vg);
				//nvgRect(args.vg, xx,yy,width,height);
				nvgFill(args.vg);
				nvgStroke(args.vg);




				//nvgRestore(args.vg);

				//nvgReset(args.vg);



			}
			else {
				//nvgFillColor(args.vg,COLOR_COMPUTERSCARE_TRANSPARENT);
			}
			//nvgFillColor(args.vg, module->seq.absoluteSequence[i] == 1 ? COLOR_COMPUTERSCARE_RED : COLOR_COMPUTERSCARE_TRANSPARENT);



		}
	}

	void draw(const DrawArgs &args) override {
		if (!module) {
			//drawHorse(args, 3);
		}
		else {
			drawHorse(args, 3);
		}
	}
};
struct ComputerscareHorseADoodleDooWidget : ModuleWidget {
	ComputerscareHorseADoodleDooWidget(ComputerscareHorseADoodleDoo *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareHorseADoodleDooPanel.svg")));
		box.size = Vec(5 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareHorseADoodleDooPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}

		addInputBlock("Pattern", 0, 100, module, 0,  ComputerscareHorseADoodleDoo::PATTERN_CV, 0);
		addInputBlock("Length", 0, 150, module, 2,  ComputerscareHorseADoodleDoo::STEPS_CV, 1);
		addInputBlock("Density", 0, 200, module, 4,  ComputerscareHorseADoodleDoo::DENSITY_CV, 0);

		channelWidget = new PolyOutputChannelsWidget(Vec(1,250),module,ComputerscareHorseADoodleDoo::POLY_KNOB);

		addChild(channelWidget);

		horseDisplay = new HorseDisplay();
		horseDisplay->module = module;

		addChild(horseDisplay);

		int outputY = 269;
		int dy = 30;

		int outputX = 32;
		addInput(createInput<InPort>(Vec(2, outputY), module, ComputerscareHorseADoodleDoo::CLOCK_INPUT));
		addInput(createInput<InPort>(Vec(2, outputY + dy), module, ComputerscareHorseADoodleDoo::RESET_INPUT));



		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY), module, ComputerscareHorseADoodleDoo::TRIGGER_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY + dy), module, ComputerscareHorseADoodleDoo::EOC_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY + dy * 2), module, ComputerscareHorseADoodleDoo::CV_OUTPUT));

	}


	void addInputBlock(std::string label, int x, int y, ComputerscareHorseADoodleDoo *module, int knobIndex,  int inputIndex, int knobType) {

		background = new InputBlockBackground();
		background->box.pos = Vec(0, y / 2 - 9);
		background->box.size = Vec(60, 45);

		addChild(background);

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->letterSpacing = 0.5;
		smallLetterDisplay->fontSize = 21;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;
		smallLetterDisplay->box.pos = Vec(x - 4, y - 15);


		if (knobType == 0) {//smooth
			addParam(createParam<SmoothKnob>(Vec(x, y), module, knobIndex));
			//trim knob
			addParam(createParam<SmallKnob>(Vec(x + 30, y), module, knobIndex + 1));
			addInput(createInput<TinyJack>(Vec(x + 40, y), module, inputIndex));
		}
		else if (knobType == 1 || knobType == 2) {
			numStepsKnob = new NumStepsOverKnobDisplay(knobType);
			numStepsKnob->box.size = Vec(20, 20);
			numStepsKnob->box.pos = Vec(x - 2.5 , y + 1.f);
			numStepsKnob->fontSize = 26;
			numStepsKnob->textAlign = 18;
			numStepsKnob->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
			numStepsKnob->breakRowWidth = 20;
			numStepsKnob->module = module;
			addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, knobIndex));
			addChild(numStepsKnob);
			if (knobType == 1) {
				//trim knob
				addParam(createParam<SmallKnob>(Vec(x + 30, y), module, knobIndex + 1));
				addInput(createInput<TinyJack>(Vec(x + 40, y), module, inputIndex));
			}
		}




		addChild(smallLetterDisplay);

	}
	PolyOutputChannelsWidget* channelWidget;
	HorseDisplay* horseDisplay;
	NumStepsOverKnobDisplay* numStepsKnob;
	InputBlockBackground* background;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareHorseADoodleDoo = createModel<ComputerscareHorseADoodleDoo, ComputerscareHorseADoodleDooWidget>("computerscare-horse-a-doodle-doo");
