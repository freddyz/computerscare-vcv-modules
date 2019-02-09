#include "Computerscare.hpp"
#include "dtpulse.hpp"
#include "dsp/digital.hpp"
#include "window.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareOhPeas;

const int numChannels= 4;

class PeasTextField : public LedDisplayTextField {

public:
  int fontSize = 16;
  int rowIndex=0;
  bool inError = false;
  PeasTextField() : LedDisplayTextField() {}
  void setModule(ComputerscareOhPeas* _module) {
    module = _module;
  }
  virtual void onTextChange() override;
  int getTextPosition(Vec mousePos) override {
    bndSetFont(font->handle);
    int textPos = bndIconLabelTextPosition(gVg, textOffset.x, textOffset.y,
      box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
      -1, fontSize, text.c_str(), mousePos.x, mousePos.y);
    bndSetFont(gGuiFont->handle);
    return textPos;
  }
  void draw(NVGcontext *vg) override {
    nvgScissor(vg, 0, 0, box.size.x, box.size.y);

    // Background
    nvgFontSize(vg, fontSize);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 10.0);
    
    if(inError) {
      nvgFillColor(vg, COLOR_COMPUTERSCARE_PINK);
    }
    else {
      nvgFillColor(vg, nvgRGB(0x00, 0x00, 0x00));
    }
     nvgFill(vg);

    // Text
    if (font->handle >= 0) {
      bndSetFont(font->handle);

      NVGcolor highlightColor = color;
      highlightColor.a = 0.5;
      int begin = min(cursor, selection);
      int end = (this == gFocusedWidget) ? max(cursor, selection) : -1;
      //bndTextField(vg,textOffset.x,textOffset.y+2, box.size.x, box.size.y, -1, 0, 0, const char *text, int cbegin, int cend);
      bndIconLabelCaret(vg, textOffset.x, textOffset.y - 3,
        box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
        -1, color, fontSize, text.c_str(), highlightColor, begin, end);

      bndSetFont(gGuiFont->handle);
    }

    nvgResetScissor(vg);
  };

private:
  ComputerscareOhPeas* module;
};

struct ComputerscareOhPeas : Module {
	enum ParamIds {
		GLOBAL_TRANSPOSE,
		NUM_DIVISIONS,
		SCALE_TRIM,
		SCALE_VAL = SCALE_TRIM+numChannels,
		OFFSET_TRIM = SCALE_VAL+numChannels,
		OFFSET_VAL = OFFSET_TRIM+numChannels,
		NUM_PARAMS=OFFSET_VAL+numChannels
		
	};
	enum InputIds {
		CHANNEL_INPUT,
		SCALE_CV=CHANNEL_INPUT+numChannels,
		OFFSET_CV=SCALE_CV+numChannels,
		NUM_INPUTS=OFFSET_CV+numChannels
	};
	enum OutputIds {
		SCALED_OUTPUT,
		QUANTIZED_OUTPUT=SCALED_OUTPUT+numChannels,
		NUM_OUTPUTS=QUANTIZED_OUTPUT+numChannels
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	PeasTextField* textField;

	int numDivisions = 12;
  int globalTranspose = 0;
  bool evenQuantizeMode = true;
  std::string numDivisionsString = "";
  SmallLetterDisplay* numDivisionsDisplay;
  SmallLetterDisplay* globalTransposeDisplay;

  Quantizer quant;

	ComputerscareOhPeas() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		quant = Quantizer("221222",12,0);

	}
	void step() override;
		json_t *toJson() override
  {
		json_t *rootJ = json_object();
    
    json_t *sequencesJ = json_array();
    for (int i = 0; i < 1; i++) {
      json_t *sequenceJ = json_string(textField->text.c_str());
      json_array_append_new(sequencesJ, sequenceJ);
    }
    json_object_set_new(rootJ, "sequences", sequencesJ);

    return rootJ;
  } 
  
  void fromJson(json_t *rootJ) override
  {
    json_t *sequencesJ = json_object_get(rootJ, "sequences");
    if (sequencesJ) {
      for (int i = 0; i < 1; i++) {
        json_t *sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ)
          textField->text = json_string_value(sequenceJ);
      }
    }
  }


	void setQuant() {
		std::string value = this->textField->text;
		this->quant = Quantizer(value,this->numDivisions,this->globalTranspose);
    this->setNumDivisionsString();
	}
  void setNumDivisionsString() {
    std::string transposeString =  (this->globalTranspose > 0 ? "+" : "" ) + std::to_string(this->globalTranspose);
    this->numDivisionsDisplay->value = std::to_string(this->numDivisions);
    this->globalTransposeDisplay->value = transposeString;
    
  }
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void ComputerscareOhPeas::step() {
	float A,B,C,D,Q,a,b,c,d;

	int numDivisionsKnobValue = floor(params[NUM_DIVISIONS].value);
  int iTranspose = floor(numDivisionsKnobValue * params[GLOBAL_TRANSPOSE].value);

  //int globalTransposeKnobValue = (int) clamp(roundf(params[GLOBAL_TRANSPOSE].value), -fNumDiv, fNumDiv);

	if(numDivisionsKnobValue != numDivisions) {
    //printf("%i, %i, %i, %i\n",numDivisionsKnobValue,numDivisions,iTranspose,globalTranspose);
		//what a hack!!!
    if(numDivisionsKnobValue != 0){
      
      numDivisions = numDivisionsKnobValue;
      setQuant();
    }
		
	}
  if(iTranspose != globalTranspose) {
        //printf("%i, %i, %i, %i\n",numDivisionsKnobValue,numDivisions,iTranspose,globalTranspose);

    globalTranspose = iTranspose;
    setQuant();
  }
	for(int i = 0; i < numChannels; i++) {
		
		a = params[SCALE_VAL+i].value;
		
		b = params[SCALE_TRIM+i].value;
		B = inputs[SCALE_CV+i].value;
		A = inputs[CHANNEL_INPUT+i].value;

		c = params[OFFSET_TRIM+i].value;
		C = inputs[OFFSET_CV+i].value;
		d = params[OFFSET_VAL+i].value;

		D = (b*B + a)*A + (c*C + d);

		Q = quant.quantizeEven(D,iTranspose);

		outputs[SCALED_OUTPUT + i].value = D;
		outputs[QUANTIZED_OUTPUT + i].value = Q;
	}
}

////////////////////////////////////
struct StringDisplayWidget3 : TransparentWidget {

  std::string *value;
  std::shared_ptr<Font> font;

  StringDisplayWidget3() {
    font = Font::load(assetPlugin(plugin, "res/Oswald-Regular.ttf"));
  };

  void draw(NVGcontext *vg) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x10, 0x00, 0x10);
    NVGcolor StrokeColor = nvgRGB(0xC0, 0xC7, 0xDE);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -1.0, -1.0, box.size.x+2, box.size.y+2, 4.0);
    nvgFillColor(vg, StrokeColor);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(vg, backgroundColor);
    nvgFill(vg);    
    
    // text 
    nvgFontSize(vg, 15);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    std::stringstream to_display;   
    to_display << std::setw(8) << *value;

    Vec textPos = Vec(6.0f, 12.0f);   
    NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
    nvgFillColor(vg, textColor);
 	nvgTextBox(vg, textPos.x, textPos.y,80,to_display.str().c_str(), NULL);

  }
};

void PeasTextField::onTextChange() {
  	module->setQuant();
}
struct SetScaleMenuItem : MenuItem {
  ComputerscareOhPeas *peas;
  std::string scale="221222";
  SetScaleMenuItem(std::string scaleInput) {
    scale=scaleInput;
  }
  void onAction(EventAction &e) override {
    peas->textField->text = scale;
    peas->setQuant();
  }
};
struct SetQuantizationModeMenuItem : MenuItem {
 ComputerscareOhPeas *peas;
 bool mode = true;
  SetQuantizationModeMenuItem(bool evenMode) {
    mode=evenMode;
  }
  void onAction(EventAction &e) override {
    peas->evenQuantizeMode = mode;
  }
  void step() override {
    rightText = CHECKMARK(peas->evenQuantizeMode == mode);
    MenuItem::step();
  }
};

struct ComputerscareOhPeasWidget : ModuleWidget {
  float randAmt = 0.f;
	ComputerscareOhPeasWidget(ComputerscareOhPeas *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/ComputerscareOhPeasPanel.svg")));
/*
		addInput(Port::create<InPort>(Vec(3, 330), Port::INPUT, module, ComputerscareOhPeas::TRG_INPUT));
		addInput(Port::create<InPort>(Vec(33, 330), Port::INPUT, module, ComputerscareOhPeas::VAL_INPUT));
		addInput(Port::create<InPort>(Vec(63, 330), Port::INPUT, module, ComputerscareOhPeas::CLR_INPUT));
	
		addParam(ParamWidget::create<LEDButton>(Vec(6, 290), module, ComputerscareOhPeas::MANUAL_TRIGGER, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<LEDButton>(Vec(66, 290), module, ComputerscareOhPeas::MANUAL_CLEAR_TRIGGER, 0.0, 1.0, 0.0));

		StringDisplayWidget3 *display = new StringDisplayWidget3();
		  display->box.pos = Vec(1,24);
		  display->box.size = Vec(88, 250);
		  display->value = &module->strValue;
		  addChild(display);
*/

		double x = 1;
		double y = 7;
		//double dy = 18.4;
		double dx = 9.95;
		double xx;
		double yy=18;

  		
  		ParamWidget* numDivisionKnob =  ParamWidget::create<MediumSnapKnob>(mm2px(Vec(11,yy-2)), module, ComputerscareOhPeas::NUM_DIVISIONS ,  1.f, 24.f, 12.0f);   
  		addParam(numDivisionKnob);
  		          
    ParamWidget* rootKnob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(21,yy-2)), module, ComputerscareOhPeas::GLOBAL_TRANSPOSE ,  -1.f, 1.f, 0.0f);   
      addParam(rootKnob);

  		  textFieldTemp = Widget::create<PeasTextField>(mm2px(Vec(x,y+24)));
	      textFieldTemp->setModule(module);
	      textFieldTemp->box.size = mm2px(Vec(44, 7));
	      textFieldTemp->multiline = false;
	      textFieldTemp->color = nvgRGB(0xC0, 0xE7, 0xDE);
        textFieldTemp->text = "221222";
	      addChild(textFieldTemp);
	      module->textField = textFieldTemp;

     	  ndd = new SmallLetterDisplay();
        ndd->box.pos = mm2px(Vec(2,yy));
        ndd->box.size = mm2px(Vec(9, 7));
        ndd->value = "";
        ndd->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
        addChild(ndd);
        module->numDivisionsDisplay = ndd;

        transposeDisplay = new SmallLetterDisplay();
        transposeDisplay->box.pos = mm2px(Vec(30,yy));
        transposeDisplay->box.size = mm2px(Vec(11, 7));
        transposeDisplay->letterSpacing = 2.f;
        transposeDisplay->value = "";
        transposeDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
        addChild(transposeDisplay);
        module->globalTransposeDisplay = transposeDisplay;

    		for(int i = 0; i < numChannels; i++) {

    			xx = x + dx*i+randAmt*(2*randomUniform()-.5);
          y+=randAmt*(randomUniform()-.5);
    			addInput(Port::create<InPort>(mm2px(Vec(xx, y-0.8)), Port::INPUT, module, ComputerscareOhPeas::CHANNEL_INPUT+i));

          ParamWidget* scaleTrimKnob =  ParamWidget::create<SmallKnob>(mm2px(Vec(xx+2,y+34)), module, ComputerscareOhPeas::SCALE_TRIM +i,  -1.f, 1.f, 0.0f);   
          addParam(scaleTrimKnob);
    			
    			addInput(Port::create<InPort>(mm2px(Vec(xx, y+40)), Port::INPUT, module, ComputerscareOhPeas::SCALE_CV+i));

    			ParamWidget* scaleKnob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(xx,y+50)), module, ComputerscareOhPeas::SCALE_VAL +i,  -1.f, 1.f, 0.0f);   
    			addParam(scaleKnob);

    			ParamWidget* offsetTrimKnob =  ParamWidget::create<ComputerscareDotKnob>(mm2px(Vec(xx+2,y+64)), module, ComputerscareOhPeas::OFFSET_TRIM +i,  -1.f, 1.f, 0.0f);   
    			addParam(offsetTrimKnob);
    			
    			addInput(Port::create<InPort>(mm2px(Vec(xx, y+70)), Port::INPUT, module, ComputerscareOhPeas::OFFSET_CV+i));


    			ParamWidget* offsetKnob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(xx,y+80)), module, ComputerscareOhPeas::OFFSET_VAL +i,  -5.f, 5.f, 0.0f);   
    			addParam(offsetKnob);

    			addOutput(Port::create<OutPort>(mm2px(Vec(xx , y+93)), Port::OUTPUT, module, ComputerscareOhPeas::SCALED_OUTPUT + i));

       		addOutput(Port::create<InPort>(mm2px(Vec(xx+1 , y+108)), Port::OUTPUT, module, ComputerscareOhPeas::QUANTIZED_OUTPUT + i));

  	}
    module->setQuant();
  }
   SmallLetterDisplay* trimPlusMinus;
  SmallLetterDisplay* ndd;
  SmallLetterDisplay* transposeDisplay;

  PeasTextField* textFieldTemp;
  Menu *createContextMenu() override;

};


void scaleItemAdd(ComputerscareOhPeas* peas, Menu* menu, std::string scale, std::string label) {
  SetScaleMenuItem *menuItem = new SetScaleMenuItem(scale);
  menuItem->text = label;
  menuItem->peas = peas;
  menu->addChild(menuItem);
}

void quantizationModeMenuItemAdd(ComputerscareOhPeas* peas, Menu* menu, bool evenMode, std::string label) {
  SetQuantizationModeMenuItem *menuItem = new SetQuantizationModeMenuItem(evenMode);
  menuItem->text = label;
  menuItem->peas = peas;
  menu->addChild(menuItem);
}
 
Menu *ComputerscareOhPeasWidget::createContextMenu() {
  Menu *menu = ModuleWidget::createContextMenu();
  ComputerscareOhPeas *peas = dynamic_cast<ComputerscareOhPeas*>(module);
  assert(peas);

  MenuLabel *spacerLabel = new MenuLabel();
  menu->addChild(spacerLabel);
  
  /*
  // "closest" quantization mode is quite a bit slower than even
  MenuLabel *quantModeLabel = new MenuLabel();
  quantModeLabel->text = "Quantization Mode";
  menu->addChild(quantModeLabel);

  quantizationModeMenuItemAdd(peas,menu,true,"Even");
  quantizationModeMenuItemAdd(peas,menu,false,"Closest");
  


  MenuLabel *spacerLabel2 = new MenuLabel();
  menu->addChild(spacerLabel2);*/
  

  MenuLabel *modeLabel = new MenuLabel();
  modeLabel->text = "Scale Presets";
  menu->addChild(modeLabel);

  scaleItemAdd(peas,menu,"221222","Major");
  scaleItemAdd(peas,menu,"212212","Natural Minor");
  scaleItemAdd(peas,menu,"2232","Major Pentatonic");
  scaleItemAdd(peas,menu,"3223","Minor Pentatonic");
  scaleItemAdd(peas,menu,"32113","Blues");
  scaleItemAdd(peas,menu,"11111111111","Chromatic");
  scaleItemAdd(peas,menu,"212213","Harmonic Minor");
  scaleItemAdd(peas,menu,"43","Major Triad");
  scaleItemAdd(peas,menu,"34","Minor Triad");
  scaleItemAdd(peas,menu,"33","Diminished Triad");
  scaleItemAdd(peas,menu,"434","Major 7 Tetrachord");
  scaleItemAdd(peas,menu,"433","Dominant 7 Tetrachord");
  scaleItemAdd(peas,menu,"343","Minor 7 Tetrachord");

  return menu;
}

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareOhPeas = Model::create<ComputerscareOhPeas, ComputerscareOhPeasWidget>("computerscare", "computerscare-ohpeas", "Oh Peas!", UTILITY_TAG);
