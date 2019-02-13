#include "Computerscare.hpp"

struct ComputerscareIso;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareIso : Module {
	enum ParamIds {
		KNOB,
		TOGGLES = KNOB + numKnobs,
		NUM_PARAMS = TOGGLES+numToggles
		
	};
	enum InputIds {
		CHANNEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS=POLY_OUTPUT + numOutputs
	};
	enum LightIds {
		NUM_LIGHTS
	};


	ComputerscareIso()  {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	
	    for (int i = 0; i < numKnobs; i++) {
				params[KNOB + i].config(0.0f, 10.0f, 0.0f);
				params[KNOB+i].config(0.f, 10.f, 0.f, "Channel "+std::to_string(i+1) + " Voltage", " Volts");
		}
		params[TOGGLES].config(0.0f, 1.0f, 0.0f);
		outputs[POLY_OUTPUT].setChannels(16);
	}
	void step() override {
		for (int i = 0; i < numKnobs; i++) {
			outputs[POLY_OUTPUT].setVoltage(params[KNOB+i].getValue(),i);
		}
	}

};

struct ComputerscareIsoWidget : ModuleWidget {
	ComputerscareIsoWidget(ComputerscareIso *module) {
		
		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareIsoPanel.svg")));

		float outputY = 334;
		box.size = Vec(150, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
				panel->box.size = box.size;
			 	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComputerscareIsoPanel.svg")));
	    		


	    		addChild(panel);
		}

		addParam(createParam<IsoButton>(Vec(10, 5), module, ComputerscareIso::TOGGLES));
		addParam(createParam<ComputerscareClockButton>(Vec(10,40),module,ComputerscareIso::TOGGLES+1));
		addParam(createParam<ComputerscareResetButton>(Vec(55,40),module,ComputerscareIso::TOGGLES+2));

		addLabeledKnob("1",20,77,module,0,0);
		addLabeledKnob("2",84,86,module,1,2);
		
		addLabeledKnob("3",30, 157,module,2,1);
		addLabeledKnob("4",62, 157, module, 3,1);
		

		addLabeledKnob("5",98, 167, module,4,2);
		addLabeledKnob("6",68, 197, module,5,0);
		addLabeledKnob("7",68, 237, module,6,3);
		addLabeledKnob("8",168, 237, module,7,3);
		addLabeledKnob("9",68, 277, module,8,3);
		addLabeledKnob("10",168, 277, module,9,4);

		addOutput(createOutput<OutPort>(Vec(33, outputY), module, ComputerscareIso::POLY_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(63, outputY), module, ComputerscareIso::POLY_OUTPUT+1));
		addOutput(createOutput<InPort>(Vec(93, outputY), module, ComputerscareIso::POLY_OUTPUT+2));

}
void addLabeledKnob(std::string label,int x, int y, ComputerscareIso *module,int index,int type) {
		smallLetterDisplay = new SmallLetterDisplay();
      smallLetterDisplay->box.pos = Vec(x+12,y-10);
      smallLetterDisplay->box.size = Vec(60, 30);
      smallLetterDisplay->value = label;
      //smallLetterDisplay->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      addChild(smallLetterDisplay);
      float ru = random::uniform();
      if(type == 0)  {
      addParam(createParam<SmoothKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
  		}
  		else if(type ==1) {
  			      addParam(createParam<SmallKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));

  		}
  		else if(type==2) {
  			      addParam(createParam<BigSmoothKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));

  		}
  		else if (type==3) {
  			addParam(createParam<LrgKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
  		}
  		else if (type==4) {
  			addParam(createParam<BigSmoothKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
  		}

  		
  		else  {
  			      addParam(createParam<MediumSnapKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));

  		}

}
SmallLetterDisplay* smallLetterDisplay;
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscareIso = Model::create<ComputerscareIso, ComputerscareIsoWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscareIso = createModel<ComputerscareIso, ComputerscareIsoWidget>("Isopig");
