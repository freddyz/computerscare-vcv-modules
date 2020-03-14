#include "Computerscare.hpp"

#include "dtpulse.hpp"

struct ComputerscareHorseADoodleDoo;

struct HorseSequencer {
	float pattern=0.f;
	int numSteps=8;
	int currentStep=-1;
	float density=0.f;
	int primes[16] = {30011,36877,26627,32833,66797,95153,66553,84857,32377,79589,25609,20113,70991,86533,21499,32491};
	int otherPrimes[16] = {80651,85237,11813,22343,19543,28027,9203,39521,42853,58411,33811,76771,10939,22721,17851,10163};
	std::vector<std::vector<int>> octets={{0,0,0,0},{0,0,0,1},{0,0,1,0},{0,0,1,1},{0,1,0,0},{0,1,0,1},{0,1,1,0},{0,1,1,1},{1,1,0,0},{1,1,0,1},{1,1,1,0},{1,1,1,1}};
	std::vector<int> absoluteSequence;
	HorseSequencer(float patt,int steps,float dens) {
		numSteps=steps;
		density=dens;
		pattern=patt;
		makeAbsolute();
	}
	void makeAbsolute() {
		absoluteSequence.resize(0);
		for(int i = 0; i < 16; i++) {
			int dex = ((int)std::floor(pattern*primes[i])+otherPrimes[i])%16;
			absoluteSequence.push_back(dex < 8 ? 0 : 1);
		}
	}
	void tick() {
		currentStep++;
		currentStep %= numSteps;
	}
	void reset() {
		currentStep=0;
	}
	int get() {
		return absoluteSequence[currentStep];
	}
	int tickAndGet() {
		tick();
		return get();
	}
};

struct ComputerscareHorseADoodleDoo : Module {
	int counter = 0;
	int numChannels=1;
	ComputerscareSVGPanel* panelRef;
	float currentValues[16]={0.f};
	enum ParamIds {
		PATTERN_KNOB,
		STEPS_KNOB,
		DENSITY_KNOB,
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
		REST_OUTPUT,
		NUM_OUTPUTS 
	};
	enum LightIds {
		NUM_LIGHTS
	};


	rack::dsp::SchmittTrigger clockInputTrigger;
  	rack::dsp::SchmittTrigger resetInputTrigger;

  	rack::dsp::SchmittTrigger clockManualTrigger;
  	rack::dsp::SchmittTrigger resetManualTrigger;

  	float lastPatternKnob=0.f;
  	float lastStepsKnob=0.f;
  	float lastDensityKnob=0.f;

  	int seqVal= 0.f;

  	HorseSequencer seq = HorseSequencer(0.f,8,0.f);

	ComputerscareHorseADoodleDoo()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(PATTERN_KNOB,0.f,10.f,0.f,"Pattern");
		configParam(STEPS_KNOB,2.f,64.f,8.f,"Number of Steps");
		configParam(DENSITY_KNOB,-2.f,2.f,0.f,"Density");
		
		seq = HorseSequencer(0.f,8,0.f);


	}
	void patternKnobChanged(float pattern,int numSteps,float density) {
		seq=HorseSequencer(pattern,numSteps,density);
	}

	void process(const ProcessArgs &args) override {
		counter++;
		if(counter > 8) {
			counter = 0;
			float patternKnob=params[PATTERN_KNOB].getValue();
			float stepsKnob=params[STEPS_KNOB].getValue();
			float densityKnob=params[DENSITY_KNOB].getValue();
			if(patternKnob != lastPatternKnob || stepsKnob != lastStepsKnob || densityKnob != lastDensityKnob) {
				patternKnobChanged(patternKnob,stepsKnob,densityKnob);
			}
			lastPatternKnob=patternKnob;
			lastStepsKnob=stepsKnob;
			lastDensityKnob=densityKnob;
		}

		bool clockInputHigh = clockInputTrigger.isHigh();
		bool clocked = clockInputTrigger.process(inputs[CLOCK_INPUT].getVoltage());

		if(clocked) {
			seqVal = seq.tickAndGet();
		}
		if (inputs[CLOCK_INPUT].isConnected()) {
	      for (int ch = 0; ch < numChannels; ch++) {
	        outputs[TRIGGER_OUTPUT].setVoltage((clockInputHigh && seqVal==1/*activePolyStep[i][ch]*/) ? 10.0f : 0.0f, ch);
	        //outputs[EOC_OUTPUT].setVoltage((currentTriggerIsHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
	      }
	    }
	    else {
	     /* for (int ch = 0; ch < numChannels; ch++) {
	        outputs[TRG_OUTPUT + i].setVoltage((globalGateIn && activePolyStep[i][ch]) ? 10.0f : 0.0f, ch);
	        outputs[FIRST_STEP_OUTPUT + i].setVoltage((globalGateIn && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
	      }*/
	    }


	}

};

struct ComputerscareHorseADoodleDooWidget : ModuleWidget {
	ComputerscareHorseADoodleDooWidget(ComputerscareHorseADoodleDoo *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareHorseADoodleDooPanel.svg")));
		box.size = Vec(4 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareHorseADoodleDooPanel.svg")));

			//module->panelRef = panel;

			addChild(panel);

		}
		float xx;
		float yy;
//		    ParamWidget* stepsKnob =  createParam<LrgKnob>(Vec(108, 30), module, ComputerscarePatchSequencer::STEPS_PARAM);


			addInput(createInput<InPort>(Vec(18, 184), module, ComputerscareHorseADoodleDoo::CLOCK_INPUT));
			addInput(createInput<InPort>(Vec(18, 204), module, ComputerscareHorseADoodleDoo::RESET_INPUT));


		addLabeledKnob("Pattern",5,90,module,0,-2,0);
		addLabeledKnob("Num Steps",5,140,module,1,0,0);
		addLabeledKnob("Density",10,250,module,2,0,0);

		addOutput(createOutput<PointingUpPentagonPort>(Vec(38, 194), module, ComputerscareHorseADoodleDoo::TRIGGER_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(38, 224), module, ComputerscareHorseADoodleDoo::EOC_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(38, 254), module, ComputerscareHorseADoodleDoo::REST_OUTPUT));

	}
	void addLabeledKnob(std::string label, int x, int y, ComputerscareHorseADoodleDoo *module, int index, float labelDx, float labelDy) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->fontSize = 21;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;

		addParam(createParam<SmoothKnob>(Vec(x, y), module, index));
		smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);


		addChild(smallLetterDisplay);

	}
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareHorseADoodleDoo = createModel<ComputerscareHorseADoodleDoo, ComputerscareHorseADoodleDooWidget>("computerscare-horse-a-doodle-doo");
