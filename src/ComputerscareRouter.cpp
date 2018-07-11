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
    ENUMS(SWITCHES,100),
	NUM_PARAMS
	};  
	enum InputIds {
		TRG_INPUT,
		ENUMS(INPUT_JACKS, 10),
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
  SchmittTrigger clockTrigger;

  int address = 0;
  int editAddress = 0;
  int numAddresses = 2;
  bool switch_states[10][10][10] = 
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
    for(int k = 0; k < 10; k++) {
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
          for(int k = 0; k < 10; k++) {

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
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
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

  if(nextAddressEdit.process(params[EDIT_PARAM].value) ) {
    editAddress = editAddress + 1;
    editAddress = editAddress % numAddresses;
  }

  if(nextAddressRead.process(params[MANUAL_CLOCK_PARAM].value) || clockTrigger.process(inputs[TRG_INPUT].value / 2.f)) {
    numAddresses =  (int) clamp(roundf(params[STEPS_PARAM].value /*+ inputs[STEPS_INPUT].value*/), 1.0f, 16.0f);

    address = address + 1;
    address = address % numAddresses;
  }
  // get inputs
  for (int i = 0 ; i < 10 ; i++)
  {
    input_values[i] = inputs[INPUT_JACKS + i].value;
  }
  
  // add inputs 
  
  for (int i = 0 ; i < 10 ; i++)
  {
   for (int j = 0 ; j < 10 ; j++)
   {
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
    NVGcolor StrokeColor = nvgRGB(0x00, 0x47, 0x7e);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -1.0, -1.0, box.size.x+2, box.size.y+2, 4.0);
    nvgFillColor(vg, StrokeColor);
    nvgFill(vg);
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
     addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(35 + column_spacing * j + 8, top_row + row_spacing * i + 8), module, ComputerscareRouter::SWITCH_LIGHTS  + i + j * 10));
   	 addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(35 + column_spacing * j + 0, top_row + row_spacing * i + 0), module, ComputerscareRouter::SWITCH_LIGHTS  + i + j * 10+100));

   }
	} 
  //address button
 	 addParam(ParamWidget::create<LEDButton>(Vec(15 , 50), module, ComputerscareRouter::MANUAL_CLOCK_PARAM, 0.0, 1.0, 0.0)); 

  //editAddress button
 	 addParam(ParamWidget::create<LEDButton>(Vec(160 , 50), module, ComputerscareRouter::EDIT_PARAM, 0.0, 1.0, 0.0)); 

  NumberDisplayWidget3 *display = new NumberDisplayWidget3();
  display->box.pos = Vec(40,40);
  display->box.size = Vec(50, 20);
  display->value = &module->address;
  addChild(display);

  NumberDisplayWidget3 *stepsDisplay = new NumberDisplayWidget3();
  stepsDisplay->box.pos = Vec(40,10);
  stepsDisplay->box.size = Vec(50, 20);
  stepsDisplay->value = &module->numAddresses;
  addChild(stepsDisplay);

  addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(120,40), module, ComputerscareRouter::STEPS_PARAM, 1.0f, 16.0f, 16.0f));



  addInput(Port::create<InPort>(Vec(20, 40), Port::INPUT, module, ComputerscareRouter::TRG_INPUT));

  NumberDisplayWidget3 *displayEdit = new NumberDisplayWidget3();
  displayEdit->box.pos = Vec(185,40);
  displayEdit->box.size = Vec(50, 20);
  displayEdit->value = &module->editAddress;
  addChild(displayEdit);

		//addParam(ParamWidget::create<Davies1900hBlackKnob>(Vec(28, 287), module, ComputerscareRouter::PITCH_PARAM, -3.0, 3.0, 0.0));



	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareRouter = Model::create<ComputerscareRouter, ComputerscareRouterWidget>("computerscare", "ComputerscareRouter", "Router", UTILITY_TAG);
