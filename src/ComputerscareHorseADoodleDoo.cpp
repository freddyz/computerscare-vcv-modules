#include "Computerscare.hpp"

#include "dtpulse.hpp"

#include <iostream>
#include <string>
#include <sstream>

struct ComputerscareHorseADoodleDoo;

const std::string HorseAvailableModes[4] = {"Each channel outputs independent pulse & CV sequence", "All channels triggered by Ch. 1 sequence", "Trigger Cascade:\nEach channel is triggered by the previous channel's trigger sequence", "EOC Cascade:\nEach channel is triggered by the previous channel's EOC"};
const std::string HorseAvailableGateModes[2] = {"Pass through the clock signal for each gate", "Variable gates"};


struct HorseModeParam : ParamQuantity {
	std::string getDisplayValueString() override {
		int val = getValue();
		return HorseAvailableModes[val];
	}
};

struct HorseGateModeParam : ParamQuantity {
	std::string getDisplayValueString() override {
		int val = getValue();
		return HorseAvailableGateModes[val];
	}
};


struct HorseSequencer {
	float pattern = 0.f;
	int numSteps = 8;
	int currentStep = -1;
	float density = 0.5f;
	float phase = 0.f;
	float phase2 = 0.f;
	float gatePhase = 0.f;

	float pendingPattern = 0.f;
	int pendingNumSteps = 8;
	float pendingDensity = 0.5f;
	float pendingPhase = 0.f;
	float pendingPhase2 = 0.f;
	float pendingGatePhase = 0.f;
	bool pendingChange = 0;
	bool forceChange = 0;



	int primes[16] = {30011, 36877, 26627, 32833, 66797, 95153, 66553, 84857, 32377, 79589, 25609, 20113, 70991, 86533, 21499, 32491};
	int otherPrimes[16] = {80651, 85237, 11813, 22343, 19543, 28027, 9203, 39521, 42853, 58411, 33811, 76771, 10939, 22721, 17851, 10163};
	int channel = 0;

	std::vector<int> absoluteSequence;
	std::vector<float> cvSequence;
	std::vector<float> cv2Sequence;
	std::vector<float> gateLengthSequence;

	std::vector<int> timeToNextStep;


	HorseSequencer() {

	}
	HorseSequencer(float patt, int steps, float dens, int ch, float phi, float phi2, float gatePhi) {
		numSteps = steps;
		density = dens;
		pattern = patt;
		channel = ch;
		phase = phi;
		phase2 = phi2;
		gatePhase = gatePhi;
		makeAbsolute();
	}
	void makeAbsolute() {
		std::vector<int> newSeq;
		std::vector<float> newCV;
		std::vector<float> newCV2;
		std::vector<float> newGateLength;

		newSeq.resize(0);
		newCV.resize(0);
		newCV2.resize(0);
		newGateLength.resize(0);



		float cvRoot = 0.f;
		float cv2Root = 0.f;
		float trigConst = 2 * M_PI / ((float)numSteps);

		for (int i = 0; i < numSteps; i++) {
			float val = 0.f;
			float cvVal = 0.f;
			float cv2Val = 0.f;
			float gateLengthVal = 0.f;
			int glv = 0;
			float arg = pattern + ((float) i) * trigConst;
			for (int k = 0; k < 4; k++) {
				int trgArgIndex = ((i + 1) * (k + 1)) % 16;
				int trgThetaIndex = (otherPrimes[0] + i) % 16;

				int cvArgIndex = ((i + 11) * (k + 1) + 201) % 16;
				int cvThetaIndex = (otherPrimes[3] + i - 7) % 16;

				int cv2ArgIndex = ((i + 12) * (k + 2) + 31) % 16;
				int cv2ThetaIndex = (otherPrimes[6] + i - 17) % 16;

				int gateLengthArgIndex = ((i + 13) * (k + 3) + 101) % 16;
				int gateThetaIndex = (otherPrimes[4] + 3 * i - 17) % 16;

				val += std::sin(primes[trgArgIndex] * arg + otherPrimes[trgThetaIndex]);
				cvVal += std::sin(primes[cvArgIndex] * arg + otherPrimes[cvThetaIndex] + phase);

				cv2Val += std::sin(primes[cv2ArgIndex] * arg + otherPrimes[cv2ThetaIndex] + phase2);
				gateLengthVal += std::sin(primes[gateLengthArgIndex] * arg + otherPrimes[gateThetaIndex] + gatePhase);

			}
			newSeq.push_back(val < (density - 0.5) * 4 * 2 ? 1 : 0);
			newCV.push_back(cvRoot + (cvVal + 4) / .8);
			newCV2.push_back(cv2Root + (cv2Val + 4) / .8);

			//quantized to 16 lengths
			newGateLength.push_back(std::floor(8 + 2 * gateLengthVal));
		}
		//printVector(newSeq);
		absoluteSequence = newSeq;
		cvSequence = newCV;
		cv2Sequence = newCV2;
		gateLengthSequence = newGateLength;

		setTimeToNextStep();
	}
	void checkAndArm(float patt, int steps, float dens, float phi, float phi2, float gatePhi) {

		if (pattern != patt || numSteps != steps || density != dens || phase != phi || phase2 != phi2 || gatePhase != gatePhi) {
			pendingPattern = patt;
			pendingNumSteps = steps;
			pendingDensity = dens;
			pendingPhase = phi;
			pendingPhase2 = phi2;
			pendingGatePhase = gatePhi;
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
		pendingPhase = phase;
		pendingPhase2 = phase2;
		pendingGatePhase = gatePhase;
	}
	void change(float patt, int steps, float dens, float phi, float phi2, float gatePhi) {
		numSteps = std::max(1, steps);
		density = std::fmax(0, dens);
		pattern = patt;
		phase = phi;
		phase2 = phi2;
		gatePhase = gatePhi;
		currentStep = 0;
		makeAbsolute();

	}
	void tick() {
		currentStep++;
		currentStep %= numSteps;
		if ((currentStep == 0 && pendingChange) || forceChange) {
			change(pendingPattern, pendingNumSteps, pendingDensity, pendingPhase, pendingPhase2, pendingGatePhase);
			pendingChange = false;
			forceChange = false;
			currentStep = 0;
		}
	}
	void setTimeToNextStep() {
		timeToNextStep.resize(0);
		timeToNextStep.resize(numSteps);
		int counter = 0;
		int timeIndex = 0;
		for (unsigned int i = 0; i < numSteps * 2; i++) {
			if (absoluteSequence[i % numSteps]) {
				timeToNextStep[timeIndex % numSteps] = counter;
				timeIndex = i;
				counter = 1;
			}
			else {
				timeToNextStep[i % numSteps] = 0;
				counter++;
			}
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
	float getCV2() {
		return cv2Sequence[currentStep];
	}
	float getGateLength() {
		return gateLengthSequence[currentStep];
	}
	int getTimeToNextStep() {
		return timeToNextStep[currentStep];
	}
	int tickAndGet() {
		tick();
		return get();
	}
	int getNumSteps() {
		return numSteps;
	}
};

struct ComputerscareHorseADoodleDoo : ComputerscareMenuParamModule {
	int counter = 0;
	float currentValues[16] = {0.f};
	bool atFirstStepPoly[16] = {false};
	int previousStep[16] = { -1};
	bool shouldSetEOCHigh[16] = {false};
	bool shouldOutputPulse[16] = {false};

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
		MODE_KNOB,
		MANUAL_RESET_BUTTON,
		PATTERN_SPREAD,
		STEPS_SPREAD,
		DENSITY_SPREAD,
		MANUAL_CLOCK_BUTTON,
		CV_SCALE,
		CV_OFFSET,
		CV_PHASE,
		GATE_MODE,
		GATE_LENGTH_SCALE,
		GATE_LENGTH_OFFSET,
		GATE_LENGTH_PHASE,
		CV2_SCALE,
		CV2_OFFSET,
		CV2_PHASE,
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
		CV2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	rack::dsp::SchmittTrigger clockInputTrigger[16];
	rack::dsp::SchmittTrigger resetInputTrigger[16];

	rack::dsp::SchmittTrigger clockManualTrigger;
	rack::dsp::SchmittTrigger globalManualResetTrigger;

	dsp::Timer syncTimer[16];

	dsp::PulseGenerator gatePulse[16];

	float lastPatternKnob = 0.f;
	int lastStepsKnob = 2;
	float lastDensityKnob = 0.f;
	int lastPolyKnob = 0;
	float lastPhaseKnob = 0.f;
	float lastGatePhaseKnob = 0.f;

	float cvOffset = 0.f;
	float cvScale = 1.f;

	float lastPhase2Knob = 0.f;
	float cv2Offset = 0.f;
	float cv2Scale = 1.f;

	float gateLengthOffset = 0.f;
	float gateLengthScale = 1.f;

	int mode = 1;

	int seqVal[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	float cvVal[16] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
	float cv2Val[16] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

	int clockChannels[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	int resetChannels[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

	bool changePending[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

	float gateTimeFactor[16] = {.999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f, .999f};

	float syncTime[16] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
	HorseSequencer seq[16];

	struct HorsePatternParamQ: ParamQuantity {
		virtual std::string getPatternString() {
			return dynamic_cast<ComputerscareHorseADoodleDoo*>(module)->getPatternDisplay();
		}
		std::string getDisplayValueString() override {
			float val = getValue();
			return std::to_string(val) + "\n" + getPatternString();
		}
	};
	struct HorsePatternSpreadParam: ParamQuantity {
		virtual std::string getAllPolyChannelsPatternDisplayString() {
			return dynamic_cast<ComputerscareHorseADoodleDoo*>(module)->getPatternDisplay(true, false);
		}
		std::string getDisplayValueString() override {
			float val = getValue();
			return std::to_string(100 * val) + "%\n" + getAllPolyChannelsPatternDisplayString();
		}
	};

	struct HorseStepsSpreadParam: ParamQuantity {
		std::string newLineSepIntVector(std::vector<int> vec) {
			std::string out = "";
			for (unsigned int i = 0; i < vec.size(); i++) {
				out = out + std::to_string(vec[i]) + "\n";
			}
			return out;
		}
		virtual std::string allStepsDisplay() {
			return dynamic_cast<ComputerscareHorseADoodleDoo*>(module)->getAllStepsDisplay();
		}
		std::string getDisplayValueString() override {
			float val = getValue();
			return std::to_string(100 * val) + "%\n" + allStepsDisplay();
		}
	};

	struct HorseDensitySpreadParam: ParamQuantity {
		virtual std::string getAllPolyChannelsDisplayString() {
			return dynamic_cast<ComputerscareHorseADoodleDoo*>(module)->getAllDensityDisplay();
		}
		std::string getDisplayValueString() override {
			float val = getValue();
			return std::to_string(100 * val) + "%\n" + getAllPolyChannelsDisplayString();
		}
	};

	struct HorseResetParamQ: ParamQuantity {
		virtual std::string getResetTransportDisplay() {
			return dynamic_cast<ComputerscareHorseADoodleDoo*>(module)->getResetTransportDisplay();
		}
		std::string getDisplayValueString() override {
			return "\n" + getResetTransportDisplay();
		}
	};

	ComputerscareHorseADoodleDoo()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam<HorsePatternParamQ>(PATTERN_KNOB, 0.f, 10.f, 0.f, "Pattern");
		configParam(STEPS_KNOB, 2.f, 64.f, 8.f, "Number of Steps");
		configParam(DENSITY_KNOB, 0.f, 1.f, 0.5f, "Density", "%", 0, 100);

		configParam(PATTERN_TRIM, -1.f, 1.f, 0.f, "Pattern CV Trim");
		configParam(STEPS_TRIM, -1.f, 1.f, 0.f, "Steps CV Trim");
		configParam(DENSITY_TRIM, -1.f, 1.f, 0.f, "Density CV Trim");


		configParam<HorsePatternSpreadParam>(PATTERN_SPREAD, 0.f, 1.f, 0.5f, "Pattern Spread", "", 0, 100);
		configParam<HorseStepsSpreadParam>(STEPS_SPREAD, -1.f, 1.f, 0.f, "Steps Spread", "", 0, 100);
		configParam<HorseDensitySpreadParam>(DENSITY_SPREAD, -1.f, 1.f, 0.f, "Density Spread", "", 0, 100);

		configParam<AutoParamQuantity>(POLY_KNOB, 0.f, 16.f, 0.f, "Polyphony");

		configParam<HorseModeParam>(MODE_KNOB, 0.f, 3.f, 0.f, "Mode");

		configParam<HorseModeParam>(GATE_MODE, 0.f, 1.f, 1.f, "Gate Mode");

		configParam<HorseResetParamQ>(MANUAL_RESET_BUTTON, 0.f, 1.f, 0.f, "Reset all Sequences");
		configParam(MANUAL_CLOCK_BUTTON, 0.f, 1.f, 0.f, "Advance all Sequences");

		configMenuParam(CV_SCALE, -2.f, 2.f, 1.f, "CV Scale", 2);
		configMenuParam(CV_OFFSET, -10.f, 10.f, 0.f, "CV Offset", 2);
		configMenuParam(CV_PHASE, -3.14159f, 3.14159f, 0.f, "CV Variation", 2);

		configMenuParam(CV2_SCALE, -2.f, 2.f, 1.f, "CV2 Scale", 2);
		configMenuParam(CV2_OFFSET, -10.f, 10.f, 0.f, "CV2 Offset", 2);
		configMenuParam(CV2_PHASE, -3.14159f, 3.14159f, 0.f, "CV2 Variation", 2);

		configMenuParam(GATE_LENGTH_SCALE, 0.f, 2.f, 1.f, "Gate Length Scaling", 2);
		configMenuParam(GATE_LENGTH_OFFSET, 0.f, 1.f, 0.f, "Gate Length Minimum", 2);
		configMenuParam(GATE_LENGTH_PHASE, -3.14159f, 3.14159f, 0.f, "Gate Length Variation", 2);

		getParamQuantity(POLY_KNOB)->randomizeEnabled = false;
		getParamQuantity(POLY_KNOB)->resetEnabled = false;

		getParamQuantity(MODE_KNOB)->randomizeEnabled = false;

		getParamQuantity(PATTERN_SPREAD)->randomizeEnabled = false;
		getParamQuantity(STEPS_SPREAD)->randomizeEnabled = false;
		getParamQuantity(DENSITY_SPREAD)->randomizeEnabled = false;

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(PATTERN_CV, "Pattern CV");
		configInput(STEPS_CV, "Number of Steps CV");
		configInput(DENSITY_CV, "Density CV");

		configOutput(TRIGGER_OUTPUT, "Trigger Sequence");
		configOutput(EOC_OUTPUT, "End of Cycle");
		configOutput(CV_OUTPUT, "CV Sequence");
		configOutput(CV2_OUTPUT, "2nd CV Sequence");

		for (int i = 0; i < 16; i++) {
			seq[i] = HorseSequencer(0.f, 8, 0.f, i, 0.f, 0.f, 0.f);
			previousStep[i] = -1;
		}


	}

	std::vector<int> getAllSteps() {
		std::vector<int> out = {};
		for (int i = 0; i < polyChannels; i++) {
			out.push_back(seq[i].getNumSteps());
		}
		return out;
	}
	std::string getAllPatternValueDisplay(std::string sep = "\n") {
		std::string out = "";

		int channelToDisplay;
		for (int i = 0; i < polyChannels; i++) {

			channelToDisplay = (mode == 1) ? 0 : i;
			out += "ch " + string::f("%*d", 2, i + 1) + ": ";
			if (seq[i].pendingChange) {
				out = out + std::to_string(seq[channelToDisplay].pendingPattern);
				out = out + " (" + std::to_string(seq[channelToDisplay].pattern) + ")";
			}
			else {
				out = out + std::to_string(seq[channelToDisplay].pattern);
			}
			out += sep;
		}
		return out;
	}

	std::string getAllStepsDisplay(std::string sep = "\n") {
		std::string out = "";
		for (int i = 0; i < polyChannels; i++) {
			out += "ch " + string::f("%*d", 2, i + 1) + ": ";
			if (seq[i].pendingChange) {
				out = out + std::to_string(seq[i].pendingNumSteps);
				out = out + " (" + std::to_string(seq[i].numSteps) + ")";
			}
			else {
				out = out + std::to_string(seq[i].getNumSteps());
			}
			out += sep;
		}
		return out;
	}
	std::string getAllDensityDisplay(std::string sep = "\n") {
		std::string out = "";
		for (int i = 0; i < polyChannels; i++) {

			out += "ch " + string::f("%*d", 2, i + 1) + ": ";
			if (seq[i].pendingChange) {
				out = out + string::f("%.*g", 3, 100 * seq[i].pendingDensity) + "%";
				out = out + " (" + string::f("%.*g", 3, 100 * seq[i].density) + "%)";
			}
			else {
				out = out + string::f("%.*g", 3, 100 * seq[i].density) + "%";
			}
			out += sep;
		}
		return out;
	}
	std::string getResetTransportDisplay(std::string sep = "\n") {
		std::string out = "";
		for (int i = 0; i < polyChannels; i++) {

			out += "ch " + string::f("%*d", 2, i + 1) + ": ";
			out = out + string::f("%*d", 4, seq[i].currentStep + 1);
			out = out + " / " + string::f("%*d", 4, seq[i].numSteps);

			out += sep;
		}
		return out;
	}
	std::string getPatternDisplay(bool showPatternValue = false, bool showTransport = true, std::string sep = "\n") {
		std::string out = "";
		int channelToDisplay;
		for (int i = 0; i < polyChannels; i++) {
			channelToDisplay = (mode == 1) ? 0 : i;
			out += "ch " + string::f("%*d", 2, i + 1) + ": ";
			int current = seq[channelToDisplay].currentStep;

			if (showPatternValue) {
				out += std::to_string(seq[channelToDisplay].pattern) + " ";
			}
			for (int j = 0; j < seq[channelToDisplay].numSteps; j++) {

				bool highStep = seq[channelToDisplay].absoluteSequence[j] == 1;

				out += (showTransport && current == j) ? (highStep ? "☺" : "☹") : ( highStep ? "x" : "_");
				out += j % 192 == 191 ? "\n" : "";
			}

			out += sep;
		}
		return out;
	}

	void setMode(int newMode) {
		params[MODE_KNOB].setValue(newMode);
	}
	void setGateMode(int newGateMode) {
		params[GATE_MODE].setValue(newGateMode);
	}

	void checkKnobChanges() {

		int pattNum = inputs[PATTERN_CV].getChannels();
		int stepsNum = inputs[STEPS_CV].getChannels();
		int densityNum = inputs[DENSITY_CV].getChannels();

		int clockNum = inputs[CLOCK_INPUT].getChannels();
		int resetNum = inputs[RESET_INPUT].getChannels();

		cvScale = params[CV_SCALE].getValue();
		cvOffset = params[CV_OFFSET].getValue();

		cv2Scale = params[CV2_SCALE].getValue();
		cv2Offset = params[CV2_OFFSET].getValue();

		gateLengthOffset = params[GATE_LENGTH_OFFSET].getValue();
		gateLengthScale = params[GATE_LENGTH_SCALE].getValue();

		mode = params[MODE_KNOB].getValue();
		lastStepsKnob = std::floor(params[STEPS_KNOB].getValue());
		lastPolyKnob = std::floor(params[POLY_KNOB].getValue());

		lastPhaseKnob = params[CV_PHASE].getValue();
		lastPhase2Knob = params[CV2_PHASE].getValue();
		lastGatePhaseKnob = params[GATE_LENGTH_PHASE].getValue();

		polyChannels = lastPolyKnob == 0 ? std::max(clockNum, std::max(pattNum, std::max(stepsNum, densityNum))) : lastPolyKnob;

		for (int i = 0; i < 16; i++) {
			clockChannels[i] = std::max(1, std::min(i + 1, clockNum));
			resetChannels[i] = std::max(1, std::min(i + 1, resetNum));
		}

		outputs[TRIGGER_OUTPUT].setChannels(polyChannels);
		outputs[EOC_OUTPUT].setChannels(polyChannels);
		outputs[CV_OUTPUT].setChannels(polyChannels);
		outputs[CV2_OUTPUT].setChannels(polyChannels);

		for (int i = 0; i < polyChannels; i++) {
			float patternVal = params[PATTERN_KNOB].getValue() + params[PATTERN_TRIM].getValue() * inputs[PATTERN_CV].getVoltage(pattNum == 1 ? 0 : fmin(i, pattNum));
			int stepsVal = std::floor(params[STEPS_KNOB].getValue() + params[STEPS_TRIM].getValue() * inputs[STEPS_CV].getVoltage(stepsNum == 1 ? 0 : fmin(i, stepsNum)));
			float densityVal = params[DENSITY_KNOB].getValue() + params[DENSITY_TRIM].getValue() * inputs[DENSITY_CV].getVoltage(densityNum == 1 ? 0 : fmin(i, densityNum)) / 10;

			patternVal += i * params[PATTERN_SPREAD].getValue();
			stepsVal += std::floor(params[STEPS_SPREAD].getValue() * i * stepsVal);
			densityVal += params[DENSITY_SPREAD].getValue() * i / 10;

			stepsVal = std::max(2, stepsVal);
			densityVal = std::fmax(0, std::fmin(1, densityVal));

			seq[i].checkAndArm(patternVal, stepsVal, densityVal, lastPhaseKnob, lastPhase2Knob, lastGatePhaseKnob);
		}
	}
	void processChannel(int ch, bool clocked, bool reset, bool clockInputHigh, int overrideMode = 0, bool overriddenTriggerHigh = false) {
		bool eocHigh = false;

		int gateMode = params[GATE_MODE].getValue();

		if (reset) {
			seq[ch].armChange();
		}

		if (clocked /*&& !reset*/) {
			if (overrideMode == 1) {
				seqVal[ch] = seq[ch].tickAndGet();
				if (overriddenTriggerHigh) {
					cvVal[ch] = seq[ch].getCV();
					cv2Val[ch] = seq[ch].getCV2();
				}
				seqVal[ch] = overriddenTriggerHigh;
			}
			else if (overrideMode == 2 || overrideMode == 3) {
				if (overriddenTriggerHigh) {
					seqVal[ch] = seq[ch].tickAndGet();
					cvVal[ch] = seq[ch].getCV();
					cv2Val[ch] = seq[ch].getCV2();
				}
			}
			else {
				// no override, operate as normal.  Tick sequencer every clock, and tick CV if the trigger is high
				seqVal[ch] = seq[ch].tickAndGet();
				if (seqVal[ch]) {
					cvVal[ch] = seq[ch].getCV();
					cv2Val[ch] = seq[ch].getCV2();
				}
			}





			atFirstStepPoly[ch] =  (seq[ch].currentStep == 0);

			shouldSetEOCHigh[ch] = atFirstStepPoly[ch] && previousStep[ch] != 0;
			shouldOutputPulse[ch] = seqVal[ch] == 1 && (previousStep[ch] != seq[ch].currentStep);

			previousStep[ch] = seq[ch].currentStep;

			if (gateMode == 1 && shouldOutputPulse[ch]) {
				int len = seq[ch].getGateLength();
				int ttns = seq[ch].getTimeToNextStep();
				float timeLeft = syncTime[0] * ttns;
				float gateLengthFactor =  math::clamp(gateLengthOffset + gateLengthScale * ((float)len) / 16, 0.05f, 0.95f);
				float ms =  timeLeft * gateLengthFactor;
				if (ch == 0 || ch == 1) {
					//DEBUG("ch:%i,step:%i,len:%i,ttns:%i,ms:%f", ch, seq[ch].currentStep, len, ttns, ms);
				}

				gatePulse[ch].reset();
				gatePulse[ch].trigger(ms);
			}


		}

		if (true || inputs[CLOCK_INPUT].isConnected()) {

			if (gateMode == 0) {
				//clock pass-through mode
				outputs[TRIGGER_OUTPUT].setVoltage((clockInputHigh && shouldOutputPulse[ch]) ? 10.0f : 0.0f, ch);

			}
			else if (gateMode == 1) {

				//gate mode
				bool gateHigh = gatePulse[ch].process(APP->engine->getSampleTime());
				outputs[TRIGGER_OUTPUT].setVoltage(gateHigh ? 10.0f : 0.0f, ch);

			}
			//DEBUG("before output:%f",cvVal);
			outputs[CV_OUTPUT].setVoltage(cvScale * cvVal[ch] + cvOffset, ch);
			outputs[CV2_OUTPUT].setVoltage(cv2Scale * cv2Val[ch] + cv2Offset, ch);
			//outputs[EOC_OUTPUT].setVoltage((currentTriggerIsHigh && atFirstStepPoly[ch]) ? 10.f : 0.0f, ch);
		}
		else {

		}

		//if (atFirstStepPoly[ch]) {
		outputs[EOC_OUTPUT].setVoltage((clockInputHigh && shouldSetEOCHigh[ch]) ? 10.f : 0.0f, ch);
		//}
	}
	void setSyncTime(int channel, float time) {
		//DEBUG("ch:%i,time:%f", channel, time);
		syncTime[channel] = time;
	}
	void process(const ProcessArgs &args) override {
		ComputerscarePolyModule::checkCounter();
		bool manualReset = globalManualResetTrigger.process(params[MANUAL_RESET_BUTTON].getValue());
		bool manualClock = clockManualTrigger.process(params[MANUAL_CLOCK_BUTTON].getValue());
		bool currentClock[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		bool currentReset[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		bool isHigh[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		float currentSyncTime;
		for (int i = 0; i < 16; i++) {
			currentClock[i] = manualClock || clockInputTrigger[i].process(inputs[CLOCK_INPUT].getVoltage(i));
			currentReset[i] = resetInputTrigger[i].process(inputs[RESET_INPUT].getVoltage(i)) || manualReset;
			isHigh[i] = manualClock || clockInputTrigger[i].isHigh();

			currentSyncTime = syncTimer[i].process(args.sampleTime);
			if (currentClock[i]) {
				syncTimer[i].reset();
				setSyncTime(i, currentSyncTime);
			}
		}

		if (mode == 0) {
			//each poly channel processes independent trigger and cv
			for (int i = 0; i < 16; i++) {
				processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1]);
			}
		}
		else if (mode == 1) {
			// all poly channels 2-16 CV only changes along with channel 1 trigger
			// what to do with the triggers for these channels?
			// force to 1 channel gate output?
			for (int i = 0; i < 16; i++) {
				if (i == 0) {
					processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1]);
				}
				else {
					processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1], mode, seqVal[0]);
				}
			}
		} else if (mode == 2) {
			// trigger cascade
			for (int i = 0; i < 16; i++) {
				if (i == 0) {
					processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1]);
				}
				else {
					processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1], mode, seqVal[i - 1]);
				}
			}
		}
		else if (mode == 3) {
			// eoc cascade: previous channels EOC clocks next channels CV and trigger
			for (int i = 0; i < 16; i++) {
				if (i == 0) {
					processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1]);
				}
				else {
					processChannel(i, currentClock[clockChannels[i] - 1], currentReset[resetChannels[i] - 1], isHigh[clockChannels[i] - 1], mode, shouldSetEOCHigh[i - 1]);
				}
			}
		}

	}
	void checkPoly() override {
		checkKnobChanges();
	}
	void paramsFromJson(json_t* rootJ) override {
		// There was no GATE_MODE param prior to v2, so set the value to 0 (clock passthrough)
		setGateMode(0);
		Module::paramsFromJson(rootJ);
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
		else {
			value = std::to_string((random::u32() % 64) + 1);
		}
		SmallLetterDisplay::draw(args);
	}
};


struct setModeItem : MenuItem
{
	ComputerscareHorseADoodleDoo *horse;
	int mySetVal;
	setModeItem(int setVal)
	{
		mySetVal = setVal;
	}

	void onAction(const event::Action &e) override
	{
		horse->setMode(mySetVal);
	}
	void step() override {
		rightText = CHECKMARK(horse->params[ComputerscareHorseADoodleDoo::MODE_KNOB].getValue() == mySetVal);
		MenuItem::step();
	}
};
struct setGateModeItem : MenuItem
{
	ComputerscareHorseADoodleDoo *horse;
	int mySetVal;
	setGateModeItem(int setVal)
	{
		mySetVal = setVal;
	}

	void onAction(const event::Action &e) override
	{
		horse->setGateMode(mySetVal);
	}
	void step() override {
		rightText = CHECKMARK(horse->params[ComputerscareHorseADoodleDoo::GATE_MODE].getValue() == mySetVal);
		MenuItem::step();
	}
};

struct ModeChildMenu : MenuItem {
	ComputerscareHorseADoodleDoo *horse;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "How the polyphonic channels are triggered"));

		for (unsigned int i = 0; i < 4; i++) {
			setModeItem *menuItem = new setModeItem(i);
			//ParamSettingItem *menuItem = new ParamSettingItem(i,ComputerscareGolyPenerator::ALGORITHM);

			menuItem->text = HorseAvailableModes[i];
			menuItem->horse = horse;
			menuItem->box.size.y = 40;
			menu->addChild(menuItem);
		}

		return menu;
	}

};
struct GateModeChildMenu : MenuItem {
	ComputerscareHorseADoodleDoo *horse;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Gate Output"));

		for (unsigned int i = 0; i < 2; i++) {
			setGateModeItem *menuItem = new setGateModeItem(i);
			//ParamSettingItem *menuItem = new ParamSettingItem(i,ComputerscareGolyPenerator::ALGORITHM);

			menuItem->text = HorseAvailableGateModes[i];
			menuItem->horse = horse;
			menuItem->box.size.y = 40;
			menu->addChild(menuItem);
		}

		return menu;
	}

};

struct ComputerscareHorseADoodleDooWidget : ModuleWidget {
	ComputerscareHorseADoodleDooWidget(ComputerscareHorseADoodleDoo *module) {

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareHorseADoodleDooPanel.svg")));
		box.size = Vec(6 * 15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
			panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareHorseADoodleDooPanel.svg")));
			addChild(panel);

		}

		addInputBlock("Pattern", 10, 100, module, 0,  ComputerscareHorseADoodleDoo::PATTERN_CV, 0, ComputerscareHorseADoodleDoo::PATTERN_SPREAD);
		addInputBlock("Length", 10, 150, module, 2,  ComputerscareHorseADoodleDoo::STEPS_CV, 1, ComputerscareHorseADoodleDoo::STEPS_SPREAD, false);
		addInputBlock("Density", 10, 200, module, 4,  ComputerscareHorseADoodleDoo::DENSITY_CV, 0, ComputerscareHorseADoodleDoo::DENSITY_SPREAD, false);
		addParam(createParam<ScrambleSnapKnobNoRandom>(Vec(4, 234), module, ComputerscareHorseADoodleDoo::MODE_KNOB));

		/*for (int i = 0; i < 1; i++) {
			horseDisplay = new HorseDisplay(i);
			horseDisplay->module = module;

			addChild(horseDisplay);
		}*/

		int inputY = 264;
		int outputY = 260;



		int dy = 30;

		int outputX = 42;

		addParam(createParam<ComputerscareClockButton>(Vec(2, inputY - 6), module, ComputerscareHorseADoodleDoo::MANUAL_CLOCK_BUTTON));
		addInput(createInput<InPort>(Vec(2, inputY + 10), module, ComputerscareHorseADoodleDoo::CLOCK_INPUT));

		addParam(createParam<ComputerscareResetButton>(Vec(2, inputY + dy + 16), module, ComputerscareHorseADoodleDoo::MANUAL_RESET_BUTTON));

		addInput(createInput<InPort>(Vec(2, inputY + 2 * dy), module, ComputerscareHorseADoodleDoo::RESET_INPUT));


		channelWidget = new PolyOutputChannelsWidget(Vec(outputX + 18, inputY - 22), module, ComputerscareHorseADoodleDoo::POLY_KNOB);
		addChild(channelWidget);

		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY), module, ComputerscareHorseADoodleDoo::TRIGGER_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY + dy), module, ComputerscareHorseADoodleDoo::EOC_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY + dy * 2), module, ComputerscareHorseADoodleDoo::CV_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(outputX, outputY + dy * 3), module, ComputerscareHorseADoodleDoo::CV2_OUTPUT));

	}


	void addInputBlock(std::string label, int x, int y, ComputerscareHorseADoodleDoo *module, int knobIndex,  int inputIndex, int knobType, int scrambleIndex, bool allowScrambleRandom = true) {

		smallLetterDisplay = new SmallLetterDisplay();
		smallLetterDisplay->box.size = Vec(5, 10);
		smallLetterDisplay->letterSpacing = 0.5;
		smallLetterDisplay->fontSize = 16;
		smallLetterDisplay->value = label;
		smallLetterDisplay->textAlign = 1;
		smallLetterDisplay->box.pos = Vec(x - 4, y - 15);


		if (knobType == 0) {//smooth
			addParam(createParam<SmoothKnob>(Vec(x, y), module, knobIndex));
			//trim knob


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
				//addParam(createParam<SmallKnob>(Vec(x + 30, y), module, knobIndex + 1));
				//addInput(createInput<TinyJack>(Vec(x + 40, y), module, inputIndex));
				//addParam(createParam<ScrambleKnob>(Vec(x+30, y+20), module, scrambleIndex));

			}
		}
		addParam(createParam<SmallKnob>(Vec(x + 32, y + 5), module, knobIndex + 1));
		addInput(createInput<TinyJack>(Vec(x + 54, y + 6), module, inputIndex));
		if (allowScrambleRandom) {
			addParam(createParam<ScrambleKnob>(Vec(x + 55, y - 15), module, scrambleIndex));
		}
		else {
			addParam(createParam<ScrambleKnobNoRandom>(Vec(x + 55, y - 15), module, scrambleIndex));
		}

	}

	void appendContextMenu(Menu* menu) override {
		ComputerscareHorseADoodleDoo* horse = dynamic_cast<ComputerscareHorseADoodleDoo*>(this->module);

		struct CV1Submenu : MenuItem {
			ComputerscareHorseADoodleDoo* module;
			Menu *createChildMenu() override {
				Menu *submenu = new Menu;

				submenu->addChild(construct<MenuLabel>(&MenuLabel::text, "Configuration of the 1st Control Voltage (CV) Pattern"));

				MenuParam* cvScaleParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::CV_SCALE], 2);
				submenu->addChild(cvScaleParamControl);

				MenuParam* cvOffsetParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::CV_OFFSET], 2);
				submenu->addChild(cvOffsetParamControl);

				MenuParam* cvPhaseParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::CV_PHASE], 2);
				submenu->addChild(cvPhaseParamControl);

				return submenu;
			}
		};
		struct CV2Submenu : MenuItem {
			ComputerscareHorseADoodleDoo* module;
			Menu *createChildMenu() override {
				Menu *submenu = new Menu;

				submenu->addChild(construct<MenuLabel>(&MenuLabel::text, "Configuration of the 2nd Control Voltage (CV2) Pattern"));

				MenuParam* cvScaleParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::CV2_SCALE], 2);
				submenu->addChild(cvScaleParamControl);

				MenuParam* cvOffsetParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::CV2_OFFSET], 2);
				submenu->addChild(cvOffsetParamControl);

				MenuParam* cvPhaseParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::CV2_PHASE], 2);
				submenu->addChild(cvPhaseParamControl);

				return submenu;
			}
		};
		struct GateLengthSubmenu : MenuItem {
			ComputerscareHorseADoodleDoo* module;
			Menu *createChildMenu() override {
				Menu *submenu = new Menu;

				submenu->addChild(construct<MenuLabel>(&MenuLabel::text, "Configuration of the Pattern of Gate Lengths"));
				MenuParam* gateScaleParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::GATE_LENGTH_SCALE], 2);
				submenu->addChild(gateScaleParamControl);

				MenuParam* gateOffsetParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::GATE_LENGTH_OFFSET], 2);
				submenu->addChild(gateOffsetParamControl);

				MenuParam* gatePhaseParamControl = new MenuParam(module->paramQuantities[ComputerscareHorseADoodleDoo::GATE_LENGTH_PHASE], 2);
				submenu->addChild(gatePhaseParamControl);

				return submenu;
			}
		};

		menu->addChild(new MenuEntry);
		ModeChildMenu *modeMenu = new ModeChildMenu();
		modeMenu->text = "Polyphonic Triggering Mode";
		modeMenu->rightText = RIGHT_ARROW;
		modeMenu->horse = horse;
		menu->addChild(modeMenu);

		GateModeChildMenu *gateModeMenu = new GateModeChildMenu();
		gateModeMenu->text = "Gate Output Mode";
		gateModeMenu->rightText = RIGHT_ARROW;
		gateModeMenu->horse = horse;
		menu->addChild(gateModeMenu);

		menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));

		CV1Submenu *cv1 = new CV1Submenu();
		cv1->text = "CV 1 Configuration";
		cv1->rightText = RIGHT_ARROW;
		cv1->module = horse;
		menu->addChild(cv1);


		CV2Submenu *cv2 = new CV2Submenu();
		cv2->text = "CV 2 Configuration";
		cv2->rightText = RIGHT_ARROW;
		cv2->module = horse;
		menu->addChild(cv2);



		GateLengthSubmenu *gateMenu = new GateLengthSubmenu();
		gateMenu->text = "Gate Length Configuration";
		gateMenu->rightText = RIGHT_ARROW;
		gateMenu->module = horse;
		menu->addChild(gateMenu);

	}
	PolyOutputChannelsWidget* channelWidget;
	NumStepsOverKnobDisplay* numStepsKnob;
	SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareHorseADoodleDoo = createModel<ComputerscareHorseADoodleDoo, ComputerscareHorseADoodleDooWidget>("computerscare-horse-a-doodle-doo");
