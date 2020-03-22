#include "Computerscare.hpp"

#include "dtpulse.hpp"

struct ComputerscareHorseADoodleDoo;

struct HorseSequencer {
	float pattern = 0.f;
	int numSteps = 8;
	int currentStep = -1;
	float density = 0.f;
	int primes[16] = {30011, 36877, 26627, 32833, 66797, 95153, 66553, 84857, 32377, 79589, 25609, 20113, 70991, 86533, 21499, 32491};
	int otherPrimes[16] = {80651, 85237, 11813, 22343, 19543, 28027, 9203, 39521, 42853, 58411, 33811, 76771, 10939, 22721, 17851, 10163};

	std::vector<std::vector<int>> octets = {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 1, 0, 0}, {0, 1, 0, 1}, {0, 1, 1, 0}, {0, 1, 1, 1}, {1, 0, 0, 0}, {1, 0, 0, 1}, {1, 0, 1, 0}, {1, 0, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 1}, {1, 1, 1, 0}, {1, 1, 1, 1}};
	std::vector<int> somethin = {1, 0, 0, 1};
	std::vector<int> absoluteSequence;
	std::vector<float> cvSequence;

	Quantizer quant;
	HorseSequencer(float patt, int steps, float dens) {
		numSteps = steps;
		density = dens;
		pattern = patt;
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
		quant = Quantizer("3223",12,0);

		float cvRange = std::sin(primes[9]*pattern-otherPrimes[3]);
		int cvRoot = 0;//std::floor(6*(1+std::sin(primes[5]*pattern-otherPrimes[2])));

		for (int i = 0; i < numSteps; i++) {
			float val = 0.f;
			float cvVal = 0.f;
			for (int k = 0; k < 4; k++) {
				val += std::sin(primes[((i + 1) * (k + 1)) % 16] * pattern + otherPrimes[(otherPrimes[0] + i) % 16]);
				cvVal+=std::sin(primes[((i + 11) * (k + 1)+201) % 16] * pattern + otherPrimes[(otherPrimes[3] + i-7) % 16]);
				//cvVal+=i/12;
			}
			newSeq.push_back(val < (density - 0.5) * 4 * 2 ? 1 : 0);
			newCV.push_back(cvRoot+(cvVal+4)/.8);
		}
		printVector(newSeq);
		absoluteSequence = newSeq;
		cvSequence = newCV;
	}
	void change(float patt, int steps, float dens) {
		numSteps = steps;
		density = dens;
		pattern = patt;
		currentStep = 0;
		DEBUG("changed to %f,%i,%f", pattern, numSteps, density);
		makeAbsolute();
	}
	void tick() {
		currentStep++;
		currentStep %= numSteps;
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

struct ComputerscareHorseADoodleDoo : Module {
	int counter = 0;
	int numChannels = 1;
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


	rack::dsp::SchmittTrigger clockInputTrigger;
	rack::dsp::SchmittTrigger resetInputTrigger;

	rack::dsp::SchmittTrigger clockManualTrigger;
	rack::dsp::SchmittTrigger resetManualTrigger;

	float lastPatternKnob = 0.f;
	int lastStepsKnob = 2;
	float lastDensityKnob = 0.f;

	float pendingPattern = 0.f;
	int pendingNumSteps = 8;
	float pendingDensity = 0.f;



	int seqVal = 0.f;
	float cvVal = 0.f;

	bool changePending = true;

	HorseSequencer seq = HorseSequencer(0.f, 8, 0.f);

	ComputerscareHorseADoodleDoo()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(PATTERN_KNOB, 0.f, 10.f, 0.f, "Pattern");
		configParam(STEPS_KNOB, 2.f, 64.f, 8.f, "Number of Steps");
		configParam(DENSITY_KNOB, 0.f, 1.f, 0.5f, "Density", "%", 0, 100);

		configParam(PATTERN_TRIM, -1.f, 1.f, 0.f, "Pattern CV Trim");
		configParam(STEPS_TRIM, -1.f, 1.f, 0.f, "Steps CV Trim");
		configParam(DENSITY_TRIM, -1.f, 1.f, 0.f, "Density CV Trim");

		seq = HorseSequencer(0.f, 8, 0.f);


	}
	void patternKnobChanged(float newPattern, int newNumSteps, float newDensity) {
		//seq = HorseSequencer(pattern, numSteps, density);
		//DEBUG("TeeHee pattern:%f,steps:%i,density:%f",newPattern,newNumSteps,newDensity);
		pendingPattern = newPattern;
		pendingNumSteps = newNumSteps;
		pendingDensity = newDensity;
		changePending = true;
	}
	void applyChange() {
		//DEBUG("Change time");
		seq.change(pendingPattern, fmax(2, pendingNumSteps), pendingDensity);
		changePending = false;
	}


	void checkKnobChanges() {
		float patternVal = params[PATTERN_KNOB].getValue() + params[PATTERN_TRIM].getValue() * inputs[PATTERN_CV].getVoltage();
		int stepsVal = std::floor(params[STEPS_KNOB].getValue() + params[STEPS_TRIM].getValue() * inputs[STEPS_CV].getVoltage());
		float densityVal = params[DENSITY_KNOB].getValue() + params[DENSITY_TRIM].getValue() * inputs[DENSITY_CV].getVoltage() / 10;


		if (patternVal != lastPatternKnob || stepsVal != lastStepsKnob || densityVal != lastDensityKnob) {
			patternKnobChanged(patternVal, stepsVal, densityVal);
		}
		lastPatternKnob = patternVal;
		lastStepsKnob = stepsVal;
		lastDensityKnob = densityVal;
	}
	void process(const ProcessArgs &args) override {
		counter++;
		if (counter > 601) {
			checkKnobChanges();
			counter = 0;
		}

		bool clockInputHigh = clockInputTrigger.isHigh();
		bool clocked = clockInputTrigger.process(inputs[CLOCK_INPUT].getVoltage());

		bool reset = resetInputTrigger.process(inputs[RESET_INPUT].getVoltage());

		if(reset) {
			if(changePending) {
				applyChange();
			}
			seq.reset();
			seqVal=seq.get();
			if(seqVal) {
				cvVal=seq.getCV();
			}
			for (int ch = 0; ch < numChannels; ch++) {
				atFirstStepPoly[ch] =  true;
			}

		}

		if (clocked && !reset) {
			seqVal = seq.tickAndGet();
			if (seqVal) {
				cvVal = seq.getCV();
			}

			for (int ch = 0; ch < numChannels; ch++) {
				atFirstStepPoly[ch] =  (seq.currentStep == 0);
			}
			if (atFirstStepPoly[0] && changePending) {
				applyChange();
				seqVal = seq.get();
				if (seqVal) {
					cvVal = seq.getCV();
				}
			}

		}
		if (inputs[CLOCK_INPUT].isConnected()) {
			for (int ch = 0; ch < numChannels; ch++) {
				outputs[TRIGGER_OUTPUT].setVoltage((clockInputHigh && seqVal == 1) ? 10.0f : 0.0f, ch);
				//DEBUG("before output:%f",cvVal);
				outputs[CV_OUTPUT].setVoltage(cvVal, ch);
				//outputs[EOC_OUTPUT].setVoltage((currentTriggerIsHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
			}
		}
		else {
			/* for (int ch = 0; ch < numChannels; ch++) {
			   outputs[TRG_OUTPUT + i].setVoltage((globalGateIn && activePolyStep[i][ch]) ? 10.0f : 0.0f, ch);
			   outputs[FIRST_STEP_OUTPUT + i].setVoltage((globalGateIn && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
			 }*/
		}


		//if (outputs[EOC_OUTPUT].isConnected()) {
		for (int ch = 0; ch < numChannels; ch++) {

			if (atFirstStepPoly[ch]) {

				outputs[EOC_OUTPUT].setVoltage((clockInputHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);

			}
		}
		//outputs[EOC_OUTPUT].setVoltage((currentTriggerIsHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
		//}



	}

};
struct NumStepsOverKnobDisplay : SmallLetterDisplay
{
	ComputerscareHorseADoodleDoo *module;
	NumStepsOverKnobDisplay()
	{
		letterSpacing=1.f;
		SmallLetterDisplay();
	};
	void draw(const DrawArgs &args)
	{
		if (module)
		{
			std::string str = std::to_string(module->lastStepsKnob);
			value = str;
		}
		SmallLetterDisplay::draw(args);
	}
};

struct HorseDisplay : TransparentWidget {
	ComputerscareHorseADoodleDoo *module;

	HorseDisplay() {
	}

	void drawHorse(const DrawArgs &args, float x=0.f) {

		float dy=380/(float)(module->seq.numSteps);
		for(int i = 0; i<module->seq.numSteps; i++) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 60,i*dy, 15.f, dy);
			nvgFillColor(args.vg, module->seq.absoluteSequence[i] ==1 ? COLOR_COMPUTERSCARE_RED : COLOR_COMPUTERSCARE_TRANSPARENT);
			nvgFill(args.vg);
			if(i==module->seq.currentStep) {
				nvgStrokeWidth(args.vg,3.f);
				nvgStrokeColor(args.vg,COLOR_COMPUTERSCARE_BLUE );
				nvgStroke(args.vg);
			}
			
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

		horseDisplay = new HorseDisplay();
		horseDisplay->module=module;

		addChild(horseDisplay);

		int outputY = 254;
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
		smallLetterDisplay->fontSize = 21;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;
		if (knobType == 0) {
			addParam(createParam<SmoothKnob>(Vec(x, y), module, knobIndex));
		}
		else if (knobType == 1) {

			numStepsKnob = new NumStepsOverKnobDisplay();
			numStepsKnob->box.size = Vec(20, 20);
			numStepsKnob->box.pos = Vec(x - 2.5 , y + 1.f);
			numStepsKnob->fontSize = 26;
			numStepsKnob->textAlign = 18;
			numStepsKnob->textColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
			numStepsKnob->breakRowWidth = 20;
			numStepsKnob->module = module;
			addParam(createParam<MediumDotSnapKnob>(Vec(x, y), module, knobIndex));
			addChild(numStepsKnob);
		}

		//trim knob
		addParam(createParam<SmallKnob>(Vec(x + 30, y), module, knobIndex + 1));
		addInput(createInput<TinyJack>(Vec(x + 40, y), module, inputIndex));

		smallLetterDisplay->box.pos = Vec(x, y - 12);


		addChild(smallLetterDisplay);

	}
	HorseDisplay* horseDisplay;
	NumStepsOverKnobDisplay* numStepsKnob;
	InputBlockBackground* background;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareHorseADoodleDoo = createModel<ComputerscareHorseADoodleDoo, ComputerscareHorseADoodleDooWidget>("computerscare-horse-a-doodle-doo");
