#include "Computerscare.hpp"

struct ComputerscareIso;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareIso : Module {
	int counter = 0;
	ComputerscareSVGPanel* panelRef;
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
		counter++;
		if(counter > 5012) { 
			//printf("%f \n",random::uniform());
			counter = 0;
			//rect4032
			//south facing high wall
		}
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
		box.size = Vec(15*10, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
		 	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComputerscareIsoPanel.svg")));
    		
    		//module->panelRef = panel;

    		addChild(panel);

		}

		addLabeledKnob("1",100,30,module,0,2);
		addLabeledKnob("2",30,80,module,1,2);
		
		addLabeledKnob("3",30, 157,module,2,1);
		addLabeledKnob("4",62, 157, module, 3,1);
		

		addLabeledKnob("5",98, 107, module,4,0);
		addLabeledKnob("6",98, 140, module,5,0);
		addLabeledKnob("7",98, 176, module,6,0);
		addLabeledKnob("8",98, 205, module,7,0);


		addLabeledKnob("9",28, 197, module,8,2);
		addLabeledKnob("10",88, 277, module,9,2);

		addLabeledKnob("11",28, 237, module,10,1);
		addLabeledKnob("12",28, 277, module,11,1);
		addLabeledKnob("13",28, 317, module,12,1);
		addLabeledKnob("14",68, 237, module,13,1);
		addLabeledKnob("15",68, 277, module,14,1);
		addLabeledKnob("16",68, 317, module,15,1);

		addOutput(createOutput<OutPort>(Vec(33, outputY), module, ComputerscareIso::POLY_OUTPUT));
		addOutput(createOutput<PointingUpPentagonPort>(Vec(63, outputY), module, ComputerscareIso::POLY_OUTPUT+1));
		addOutput(createOutput<InPort>(Vec(93, outputY), module, ComputerscareIso::POLY_OUTPUT+2));

}
void addLabeledKnob(std::string label,int x, int y, ComputerscareIso *module,int index,int type) {
      smallLetterDisplay = new SmallLetterDisplay();
      smallLetterDisplay->box.size = Vec(60, 30);
      smallLetterDisplay->value = label;
      if(type == 0)  {
	      addParam(createParam<SmoothKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
	      smallLetterDisplay->box.pos = Vec(x+22,y+2);
  		}
  		else if(type ==1) {
  			addParam(createParam<SmallKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+12,y-10);
  		}
  		else if(type==2) {
  			addParam(createParam<BigSmoothKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+22,y-12);
  		}
  		else if (type==3) {
  			addParam(createParam<LrgKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+22,y-12);
  		}
  		else if (type==4) {
  			addParam(createParam<BigSmoothKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+22,y-12);
  		}
  		
  		else  {
  			addParam(createParam<MediumSnapKnob>(Vec(x,y),module,ComputerscareIso::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+12,y-10);
  		}
      addChild(smallLetterDisplay);

}
SmallLetterDisplay* smallLetterDisplay;
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.

//Model *modelComputerscareIso = Model::create<ComputerscareIso, ComputerscareIsoWidget>("computerscare", "computerscare-iso", "Isopig", UTILITY_TAG);
Model *modelComputerscareIso = createModel<ComputerscareIso, ComputerscareIsoWidget>("Isopig");
