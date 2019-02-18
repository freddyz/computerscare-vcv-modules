#include "Computerscare.hpp"

struct ComputerscareKnolyPobs;

const int numKnobs = 16;

const int numToggles = 16;
const int numOutputs = 16;

struct ComputerscareKnolyPobs : Module {
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


	ComputerscareKnolyPobs()  {

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

struct ComputerscareKnolyPobsWidget : ModuleWidget {
	ComputerscareKnolyPobsWidget(ComputerscareKnolyPobs *module) {
		
		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareKnolyPobsPanel.svg")));
		box.size = Vec(4*15, 380);
		{
			ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
			panel->box.size = box.size;
		 	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComputerscareKnolyPobsPanel.svg")));
    		
    		//module->panelRef = panel;

    		addChild(panel);

		}
		float xx;
		float yy;
		for(int i = 0; i < numKnobs; i++) {
			xx=0+25*(i%2);
			yy=64 + 18.5*(i-i%2) + 11.3*(i%2);
			addLabeledKnob(std::to_string(i+1),xx,yy,module,i,0);
		}
		
		

		addOutput(createOutput<OutPort>(Vec(16, 34), module, ComputerscareKnolyPobs::POLY_OUTPUT));

}
void addLabeledKnob(std::string label,int x, int y, ComputerscareKnolyPobs *module,int index,int type) {

      smallLetterDisplay = new SmallLetterDisplay();
      smallLetterDisplay->box.size = Vec(60, 30);
      smallLetterDisplay->value = label;
      if(type == 0)  {
	      addParam(createParam<SmoothKnob>(Vec(x,y),module,ComputerscareKnolyPobs::KNOB+index));
	      smallLetterDisplay->box.pos = Vec(x,y-12);
  		}
  		else if(type ==1) {
  			addParam(createParam<SmallKnob>(Vec(x,y),module,ComputerscareKnolyPobs::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+12,y-10);
  		}
  		else if(type==2) {
  			addParam(createParam<BigSmoothKnob>(Vec(x,y),module,ComputerscareKnolyPobs::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+22,y-12);
  		}
  		else if (type==3) {
  			addParam(createParam<LrgKnob>(Vec(x,y),module,ComputerscareKnolyPobs::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+22,y-12);
  		}
  		else if (type==4) {
  			addParam(createParam<BigSmoothKnob>(Vec(x,y),module,ComputerscareKnolyPobs::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+22,y-12);
  		}
  		
  		else  {
  			addParam(createParam<MediumSnapKnob>(Vec(x,y),module,ComputerscareKnolyPobs::KNOB+index));
      		smallLetterDisplay->box.pos = Vec(x+12,y-10);
  		}
      addChild(smallLetterDisplay);

}
SmallLetterDisplay* smallLetterDisplay;
};

Model *modelComputerscareKnolyPobs = createModel<ComputerscareKnolyPobs, ComputerscareKnolyPobsWidget>("KnolyPobs");
