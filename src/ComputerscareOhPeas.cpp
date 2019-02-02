#include "Computerscare.hpp"
#include "dtpulse.hpp"
#include "dsp/digital.hpp"
#include "window.hpp"
#include "dsp/filter.hpp"

#include <string>
#include <sstream>
#include <iomanip>

#define NUM_LINES 16

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

	std::string defaultStrValue = "0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n";
	std::string strValue = "0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n0.000000\n";

	float logLines[NUM_LINES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	PeasTextField* textField;

	int lineCounter = 0;
	int numDivisions = 12;
  int globalTranspose = 0;
  std::string numDivisionsString = "N";
  SmallLetterDisplay* numDivisionsDisplay;
  SmallLetterDisplay* globalTransposeDisplay;

	SchmittTrigger clockTrigger;
	SchmittTrigger clearTrigger;
	SchmittTrigger manualClockTrigger;
  	SchmittTrigger manualClearTrigger;

  	Quantizer quantizers[numChannels];
  	Quantizer quant;
  	std::vector<float> vvv = {0.f, 0.4f, 0.7f, 0.95f};

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
    this->numDivisionsDisplay->value = std::to_string(this->numDivisions);
    this->globalTransposeDisplay->value = std::to_string(this->globalTranspose);
    
  }
	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void ComputerscareOhPeas::step() {
	float A,B,C,D,Q,a,b,c,d,octavePart;

	int numDivisionsKnobValue = floor(params[NUM_DIVISIONS].value);
  int iTranspose = floor(numDivisionsKnobValue * params[GLOBAL_TRANSPOSE].value);

  //int globalTransposeKnobValue = (int) clamp(roundf(params[GLOBAL_TRANSPOSE].value), -fNumDiv, fNumDiv);

	if(numDivisionsKnobValue != numDivisions) {
    printf("%i, %i, %i, %i\n",numDivisionsKnobValue,numDivisions,iTranspose,globalTranspose);
		//what a hack!!!
    if(numDivisionsKnobValue != 0){
      
      numDivisions = numDivisionsKnobValue;
      setQuant();
    }
		
	}
  if(iTranspose != globalTranspose) {
        printf("%i, %i, %i, %i\n",numDivisionsKnobValue,numDivisions,iTranspose,globalTranspose);

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
  std::string value = module->textField->text;
  Quantizer q = Quantizer(value,12,0);

  if(true) {
  	printf("no parse error\n");
  	module->setQuant();
    //module->textFields[this->rowIndex]->inError=false;
    
      //module->setNextAbsoluteSequence(this->rowIndex);
      //module->updateDisplayBlink(this->rowIndex);
      //whoKnowsLaundry(value);
  }
  else {
  	printf("Parse Error\n");
    //module->textFields[this->rowIndex]->inError=true;
  }

}

struct ComputerscareOhPeasWidget : ModuleWidget {

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
		  	
		ParamWidget* rootKnob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(30,yy)), module, ComputerscareOhPeas::GLOBAL_TRANSPOSE ,  -1.f, 1.f, 0.0f);   
  		addParam(rootKnob);
  		
  		ParamWidget* numDivisionKnob =  ParamWidget::create<MediumSnapKnob>(mm2px(Vec(10,yy)), module, ComputerscareOhPeas::NUM_DIVISIONS ,  1.f, 24.f, 12.0f);   
  		addParam(numDivisionKnob);
  		   
  		  textFieldTemp = Widget::create<PeasTextField>(mm2px(Vec(x,y+20)));
	      textFieldTemp->setModule(module);
	      textFieldTemp->box.size = mm2px(Vec(44, 7));
	      textFieldTemp->multiline = false;
	      textFieldTemp->color = nvgRGB(0xC0, 0xE7, 0xDE);
        textFieldTemp->text = "221222";
	      addChild(textFieldTemp);
	      module->textField = textFieldTemp;

     	  ndd = new SmallLetterDisplay();
        ndd->box.pos = mm2px(Vec(2,yy));
        ndd->box.size = Vec(7, 7);
        ndd->value = "Y";
        ndd->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
        addChild(ndd);
        module->numDivisionsDisplay = ndd;

        gtd = new SmallLetterDisplay();
        gtd->box.pos = mm2px(Vec(22,yy));
        gtd->box.size = Vec(7, 7);
        gtd->value = "Y";
        gtd->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
        addChild(gtd);
        module->globalTransposeDisplay = gtd;

    		for(int i = 0; i < numChannels; i++) {


    			xx = x + dx*i;
    			//if(i %2) {
    				addInput(Port::create<InPort>(mm2px(Vec(xx, y)), Port::INPUT, module, ComputerscareOhPeas::CHANNEL_INPUT+i));
    			/*}
    			else {
    				addInput(Port::create<PointingUpPentagonPort>(mm2px(Vec(xx, y)), Port::INPUT, module, ComputerscareOhPeas::CHANNEL_INPUT+i));
    			}*/


    			ParamWidget* scaleTrimKnob =  ParamWidget::create<SmallKnob>(mm2px(Vec(xx+2,y+34)), module, ComputerscareOhPeas::SCALE_TRIM +i,  -1.f, 1.f, 0.0f);   
    			addParam(scaleTrimKnob);
    			
    			addInput(Port::create<InPort>(mm2px(Vec(xx, y+40)), Port::INPUT, module, ComputerscareOhPeas::SCALE_CV+i));

    			ParamWidget* scaleKnob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(xx,y+50)), module, ComputerscareOhPeas::SCALE_VAL +i,  -1.f, 1.f, 0.0f);   
    			addParam(scaleKnob);

    			ParamWidget* offsetTrimKnob =  ParamWidget::create<SmallKnob>(mm2px(Vec(xx+2,y+64)), module, ComputerscareOhPeas::OFFSET_TRIM +i,  -1.f, 1.f, 0.0f);   
    			addParam(offsetTrimKnob);
    			
    			addInput(Port::create<InPort>(mm2px(Vec(xx, y+70)), Port::INPUT, module, ComputerscareOhPeas::OFFSET_CV+i));


    			ParamWidget* offsetKnob =  ParamWidget::create<SmoothKnob>(mm2px(Vec(xx,y+80)), module, ComputerscareOhPeas::OFFSET_VAL +i,  -5.f, 5.f, 0.0f);   
    			addParam(offsetKnob);

    			addOutput(Port::create<OutPort>(mm2px(Vec(xx , y+93)), Port::OUTPUT, module, ComputerscareOhPeas::SCALED_OUTPUT + i));

       		addOutput(Port::create<OutPort>(mm2px(Vec(xx , y+105)), Port::OUTPUT, module, ComputerscareOhPeas::QUANTIZED_OUTPUT + i));

  	}
  }
  SmallLetterDisplay* ndd;
  SmallLetterDisplay* gtd;

  PeasTextField* textFieldTemp;
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelComputerscareOhPeas = Model::create<ComputerscareOhPeas, ComputerscareOhPeasWidget>("computerscare", "computerscare-ohpeas", "Oh Peas!", UTILITY_TAG);
