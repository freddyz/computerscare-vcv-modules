#include "Computerscare.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>

#define NUM_LINES 16

struct ComputerscareRouter : Module {
	enum ParamIds {
    STEPS_PARAM,
    MANUAL_CLOCK_PARAM,
    EDIT_PARAM,
    EDIT_PREV_PARAM,
    ENUMS(SWITCHES,100),
	NUM_PARAMS
	};  
	enum InputIds {
		TRG_INPUT,
		ENUMS(INPUT_JACKS, 10),
		RANDOMIZE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUTS,    
		NUM_OUTPUTS = OUTPUTS + 10
	};
  enum LightIds {
		SWITCH_LIGHTS,
    	NUM_LIGHTS = SWITCH_LIGHTS + 200
	};

  SchmittTrigger switch_triggers[10][10];

  SchmittTrigger nextAddressRead;
  SchmittTrigger nextAddressEdit;
  SchmittTrigger prevAddressEdit;
  SchmittTrigger clockTrigger;
  SchmittTrigger randomizeTrigger;

  int address = 0;
  int editAddress = 0;
  int numAddresses = 2;
  bool switch_states[16][10][10] = 
  {{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0}}
};
   
  
  float input_values[10] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
  float sums[10] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0}; 
  
ComputerscareRouter() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;



	json_t *toJson() override
  {
		json_t *rootJ = json_object();
    
    // button states
		json_t *button_statesJ = json_array();
    for(int k = 0; k < 16; k++) {
		for (int i = 0; i < 10; i++)
    {
			for (int j = 0; j < 10; j++)
      {
        json_t *button_stateJ = json_integer((int) switch_states[k][i][j]);
			   json_array_append_new(button_statesJ, button_stateJ);
        }
		  }
    }
		json_object_set_new(rootJ, "buttons", button_statesJ);
    
    return rootJ;
  } 
  
  void fromJson(json_t *rootJ) override
  {
    // button states
		json_t *button_statesJ = json_object_get(rootJ, "buttons");
		if (button_statesJ)
    {
          for(int k = 0; k < 16; k++) {

			for (int i = 0; i < 10; i++)
      {
        for (int j = 0; j < 10; j++)
        {
				  json_t *button_stateJ = json_array_get(button_statesJ, k*100+i*10 + j);
				  if (button_stateJ)
					  switch_states[k][i][j] = !!json_integer_value(button_stateJ);
        }
      }
    }
		}  
  } 
  	void onRandomize() override {
  		randomizePatchMatrix();
  	}
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

	void randomizePatchMatrix()
	{
		int randomIndex;
		for (int i = 0; i < 10; i++) 
		{
			randomIndex = floor(randomUniform()*10);

			for (int j = 0; j < 10; j++) 
			{
				if(j==randomIndex) 
					switch_states[editAddress][j][i] = 1;
				
				else 
					switch_states[editAddress][j][i]=0;
				
			//fully randomize everything.  a bit intense for its use in patching
			//switch_states[editAddress][i][j] =  (randomUniform() > 0.5f);
			}			
		}	

	}; // end randomize()
	void onReset() override
	{
		for(int k =0; k < 16; k++) {

				
		for (int i = 0; i < 10; i++) 
		{
			for (int j = 0; j < 10; j++) 
		{
			switch_states[k][i][j] =  0;
			}			
		}	
	}
	}; // end randomize()

};


void ComputerscareRouter::step() {

  for ( int i = 0 ; i < 10 ; i++)
  {
   sums[i] = 0.0;
  }
  // deal with buttons
  for (int i = 0 ; i < 10 ; i++)
  {
   for (int j = 0 ; j < 10 ; j++)
   {
     if (switch_triggers[i][j].process(params[SWITCHES+j*10 + i].value))
     {
		   switch_states[editAddress][i][j] = !switch_states[editAddress][i][j];
	   }
     lights[SWITCH_LIGHTS + i + j * 10].value  = (switch_states[editAddress][i][j]) ? 1.0 : 0.0;
   		lights[SWITCH_LIGHTS + i + j * 10+100].value  = (switch_states[address][i][j]) ? 1.0 : 0.0;
   	
   }
  }

  if(randomizeTrigger.process(inputs[RANDOMIZE_INPUT].value / 2.f)) {
  	randomizePatchMatrix();
  }
  if(nextAddressEdit.process(params[EDIT_PARAM].value) ) {
    editAddress = editAddress + 1;
    editAddress = editAddress % 16;
    numAddresses =  (int) clamp(roundf(params[STEPS_PARAM].value /*+ inputs[STEPS_INPUT].value*/), 1.0f, 16.0f);
  }
  if(prevAddressEdit.process(params[EDIT_PREV_PARAM].value) ) {
    editAddress = editAddress - 1;
    editAddress = editAddress + 16;
    editAddress = editAddress % 16;
    numAddresses =  (int) clamp(roundf(params[STEPS_PARAM].value /*+ inputs[STEPS_INPUT].value*/), 1.0f, 16.0f);
  }

  if(nextAddressRead.process(params[MANUAL_CLOCK_PARAM].value) || clockTrigger.process(inputs[TRG_INPUT].value / 2.f)) {
    numAddresses =  (int) clamp(roundf(params[STEPS_PARAM].value /*+ inputs[STEPS_INPUT].value*/), 1.0f, 16.0f);

    address = address + 1;
    address = address % numAddresses;
  }

  for (int i = 0 ; i < 10 ; i++)
  {
    input_values[i] = inputs[INPUT_JACKS + i].value;
  }
  
  // add inputs 
  
  for (int i = 0 ; i < 10 ; i++)
  {
   for (int j = 0 ; j < 10 ; j++)
   {
   	// todo: toggle for each output of how to combine
   	// sum, average, and, or etc
     if (switch_states[address][j][i]) sums[i] += input_values[j];
   }
  }
  /// outputs  
  for (int i = 0 ; i < 10 ; i++)
  {
    outputs[OUTPUTS + i].value = sums[i];    
  }  
}

////////////////////////////////////
struct NumberDisplayWidget3 : TransparentWidget {

  int *value;
  std::shared_ptr<Font> font;

  NumberDisplayWidget3() {
    font = Font::load(assetPlugin(plugin, "res/digital-7.ttf"));
  };

  void draw(NVGcontext *vg) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x00, 0x00, 0x00);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(vg, backgroundColor);
    nvgFill(vg);    
    
    // text 
    nvgFontSize(vg, 13);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    std::stringstream to_display;   
    to_display << std::setw(3) << *value;

    Vec textPos = Vec(6.0f, 17.0f);   
    NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
    nvgFillColor(vg, textColor);
    nvgText(vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
  }
};



struct ComputerscareRouterWidget : ModuleWidget {

	ComputerscareRouterWidget(ComputerscareRouter *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareRouter.svg")));

  int top_row = 75;
  int row_spacing = 25; 
  int column_spacing = 25;

	for (int i = 0 ; i < 10 ; i++)
  {
	 addInput(Port::create<InPort>(Vec(3, i * row_spacing + top_row), Port::INPUT, module, ComputerscareRouter::INPUT_JACKS + i));  
   	 addOutput(Port::create<InPort>(Vec(33 + i * column_spacing , top_row + 10 * row_spacing), Port::OUTPUT, module, ComputerscareRouter::OUTPUTS + i));
	   for(int j = 0 ; j < 10 ; j++ )
	   {
	     addParam(ParamWidget::create<LEDButton>(Vec(35 + column_spacing * j, top_row + row_spacing * i), module, ComputerscareRouter::SWITCHES + i + j * 10, 0.0, 1.0, 0.0));
	     
	     addChild(ModuleLightWidget::create<LargeLight<GreenLight>>(Vec(35 + column_spacing * j +1.4, top_row + row_spacing * i +1.4 ), module, ComputerscareRouter::SWITCH_LIGHTS  + i + j * 10));
	   	 addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(35 + column_spacing * j + 4.3, top_row + row_spacing * i + 4.3), module, ComputerscareRouter::SWITCH_LIGHTS  + i + j * 10+100));

	   }
		} 

	//clock input
  	addInput(Port::create<InPort>(Vec(3, 12), Port::INPUT, module, ComputerscareRouter::TRG_INPUT));
  	 //manual clock button
 	addParam(ParamWidget::create<LEDButton>(Vec(7 , 41), module, ComputerscareRouter::MANUAL_CLOCK_PARAM, 0.0, 1.0, 0.0)); 


  	//randomize input
  	addInput(Port::create<InPort>(Vec(270, 0), Port::INPUT, module, ComputerscareRouter::RANDOMIZE_INPUT));
  	


 	//active step display
  NumberDisplayWidget3 *display = new NumberDisplayWidget3();
  display->box.pos = Vec(30,40);
  display->box.size = Vec(50, 20);
  display->value = &module->address;
  addChild(display);

  // number of steps
  NumberDisplayWidget3 *stepsDisplay = new NumberDisplayWidget3();
  stepsDisplay->box.pos = Vec(150,40);
  stepsDisplay->box.size = Vec(50, 20);
  stepsDisplay->value = &module->numAddresses;
  addChild(stepsDisplay);
  
  //number-of-steps dial.   Discrete - 16 positions
  ParamWidget* stepsKnob =  ParamWidget::create<LrgKnob>(Vec(108,30), module, ComputerscareRouter::STEPS_PARAM, 1.0f, 16.0f, 2.0f);
  addParam(stepsKnob);

  //editAddress button
  addParam(ParamWidget::create<LEDButton>(Vec(227 , 41), module, ComputerscareRouter::EDIT_PARAM, 0.0, 1.0, 0.0));

  //editAddressPrevious button
  addParam(ParamWidget::create<LEDButton>(Vec(208 , 41), module, ComputerscareRouter::EDIT_PREV_PARAM, 0.0, 1.0, 0.0));
  
  // currently editing step #:
  NumberDisplayWidget3 *displayEdit = new NumberDisplayWidget3();
  displayEdit->box.pos = Vec(245,40);
  displayEdit->box.size = Vec(50, 20);
  displayEdit->value = &module->editAddress;
  addChild(displayEdit);
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareRouter = Model::create<ComputerscareRouter, ComputerscareRouterWidget>("computerscare", "computerscare-patch-sequencer", "Father & Son Patch Sequencer", UTILITY_TAG,SEQUENCER_TAG);
