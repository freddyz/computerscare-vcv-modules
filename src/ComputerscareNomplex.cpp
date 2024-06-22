#include "Computerscare.hpp"

#include <array>

std::vector<std::string> wrapModeDescriptions;



struct ComputerscareNomplexPumbers : ComputerscareMenuParamModule
{
    enum ParamIds
    {   
        WRAP_MODE,
        COMPOLY_CHANNELS_RECT_IN,
        COMPOLY_CHANNELS_POLAR_IN,
        REAL_INPUT_OFFSET,
        REAL_INPUT_TRIM,
        IMAGINARY_INPUT_OFFSET,
        IMAGINARY_INPUT_TRIM,
        MODULUS_INPUT_OFFSET,
        MODULUS_INPUT_TRIM,
        ARGUMENT_INPUT_OFFSET,
        ARGUMENT_INPUT_TRIM,
        NUM_PARAMS
    };
    enum InputIds
    {
        REAL_IN,
        IMAGINARY_IN,
        MODULUS_IN,
        ARGUMENT_IN,
        NUM_INPUTS
    };
    enum OutputIds
    {
        RECT_IN_RECT_OUT,
        RECT_IN_POLAR_OUT=RECT_IN_RECT_OUT+2,
        POLAR_IN_RECT_OUT=RECT_IN_POLAR_OUT+2,
        POLAR_IN_POLAR_OUT=POLAR_IN_RECT_OUT+2,
        NUM_OUTPUTS=POLAR_IN_POLAR_OUT+2
    };
    enum LightIds
    {
        BLINK_LIGHT,
        NUM_LIGHTS
    };



    enum wrapModes {
        WRAP_NORMAL,
        WRAP_CYCLE,
        WRAP_MINIMAL,
        WRAP_STALL
    };

    int compolyChannelsRectIn = 16;
    int compolyChannelsPolarIn = 16;

    ComputerscareNomplexPumbers()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        wrapModeDescriptions.push_back("Normal (Standard Polyphonic Behavior)");
        wrapModeDescriptions.push_back("Cycle (Repeat Channels)");
        wrapModeDescriptions.push_back("Minimal (Pad with 0v)");
        wrapModeDescriptions.push_back("Stall (Pad with final voltage)");

        configMenuParam(WRAP_MODE, 0.f, "Polyphonic Wrapping Mode", wrapModeDescriptions);
        getParamQuantity(WRAP_MODE)->randomizeEnabled = false;
        getParamQuantity(WRAP_MODE)->resetEnabled = false;

        configParam<AutoParamQuantity>(COMPOLY_CHANNELS_RECT_IN, 0.f, 16.f, 0.f, "Compoly Channels for Rectangular Input");
        getParamQuantity(COMPOLY_CHANNELS_RECT_IN)->randomizeEnabled = false;

        configParam<AutoParamQuantity>(COMPOLY_CHANNELS_POLAR_IN, 0.f, 16.f, 0.f, "Compoly Channels for Polar Input");
        getParamQuantity(COMPOLY_CHANNELS_POLAR_IN)->randomizeEnabled = false;

        configInput(REAL_IN,"Real");
        configInput(IMAGINARY_IN,"Imaginary");
        configInput(MODULUS_IN,"Modulus (Length)");
        configInput(ARGUMENT_IN,"Argument in Radians");

        configParam(REAL_INPUT_OFFSET,-10.f,10.f,0.f,"Real Input Offset");
        configParam(REAL_INPUT_TRIM,-2.f,2.f,1.f,"Real Input Attenuversion");
        configParam(IMAGINARY_INPUT_OFFSET,-10.f,10.f,0.f,"Imaginary Input Offset");
        configParam(IMAGINARY_INPUT_TRIM,-2.f,2.f,1.f,"Imaginary Input Attenuversion");

        configParam(MODULUS_INPUT_OFFSET,-10.f,10.f,0.f,"Modulus Input Offset");
        configParam(MODULUS_INPUT_TRIM,-2.f,2.f,1.f,"Modulus Input Attenuversion");
        configParam(ARGUMENT_INPUT_OFFSET,-10.f,10.f,0.f,"Argument Input Offset"," radians");
        configParam(ARGUMENT_INPUT_TRIM,-2.f,2.f,1.f,"Argument Input Attenuversion");

        configOutput(RECT_IN_RECT_OUT, "Rectangular Input, Rectangular #1-8");
        configOutput(RECT_IN_RECT_OUT + 1, "Rectangular Input, Rectangular #9-16");

        configOutput(RECT_IN_POLAR_OUT, "Rectangular Input, Polar #1-8");
        configOutput(RECT_IN_POLAR_OUT + 1, "Rectangular Input, Polar #9-16");

        configOutput(POLAR_IN_RECT_OUT, "Polar Input, Rectangular #1-8");
        configOutput(POLAR_IN_RECT_OUT + 1, "Polar Input, Rectangular #9-16");

        configOutput(POLAR_IN_POLAR_OUT, "Polar Input, Polar #1-8");
        configOutput(POLAR_IN_POLAR_OUT + 1, "Polar Input, Polar #9-16");
    }


    void checkPoly() override {

        int rectInputCompolyphonyKnobSetting = params[COMPOLY_CHANNELS_RECT_IN].getValue();

        int numRealInputChannels = inputs[REAL_IN].getChannels();
        int numImaginaryInputChannels = inputs[IMAGINARY_IN].getChannels();
 
        if (numRealInputChannels > 0 || numImaginaryInputChannels > 0 ) {
            if (rectInputCompolyphonyKnobSetting == 0) {
                compolyChannelsRectIn = std::max(numRealInputChannels,numImaginaryInputChannels);
            }
            else {
                compolyChannelsRectIn = rectInputCompolyphonyKnobSetting;
            }
        } else {
            compolyChannelsRectIn = rectInputCompolyphonyKnobSetting == 0 ? 1 : rectInputCompolyphonyKnobSetting;
        }
   



        int polarInputCompolyphonyKnobSetting = params[COMPOLY_CHANNELS_POLAR_IN].getValue();

        int numModulusInputChannels = inputs[MODULUS_IN].getChannels();
        int numArgumentInputChannels = inputs[ARGUMENT_IN].getChannels();
 
        if (numModulusInputChannels > 0 || numArgumentInputChannels > 0 ) {
            if (polarInputCompolyphonyKnobSetting == 0) {
                compolyChannelsPolarIn = std::max(numModulusInputChannels,numArgumentInputChannels);
            }
            else {
                compolyChannelsPolarIn = polarInputCompolyphonyKnobSetting;
            }
        } else {
            compolyChannelsPolarIn = polarInputCompolyphonyKnobSetting == 0 ? 1 : polarInputCompolyphonyKnobSetting;
        }

    }

    std::array<int,2> getInputChannels(int index, int numFirst,int numSecond) {
        int aInputCh = 1;
        int bInputCh = 1;

        int wrapMode = params[WRAP_MODE].getValue();

        if(wrapMode == WRAP_NORMAL) {
            /*
                If monophonic, copy ch1 to all
                Otherwise use the poly channels
            */
            if(numFirst==1) {
                aInputCh=0;
            } else {
                aInputCh=index;
            }
            if(numSecond==1) {
                bInputCh=0;
            } else {
                bInputCh=index;
            }

        } else if(wrapMode == WRAP_CYCLE) {
            // Cycle through the poly channels
            aInputCh = index % numFirst;
            bInputCh = index % numSecond;

        } else if(wrapMode == WRAP_MINIMAL) {
            // Do not copy channel 1 if monophonic
            aInputCh=index;
            bInputCh=index;

        } else if(wrapMode == WRAP_STALL) {
            // Go up to the maximum channel, and then use the final value for the rest
            aInputCh=index>=numFirst ? numFirst-1 : index;
            bInputCh=index>=numSecond ? numSecond-1 : index;
        }

        return {aInputCh,bInputCh};

    }

    void process(const ProcessArgs &args) override {
        ComputerscarePolyModule::checkCounter();

        int numRealInputChannels = inputs[REAL_IN].getChannels();
        int numImaginaryInputChannels = inputs[IMAGINARY_IN].getChannels();
        int numModulusInputChannels = inputs[MODULUS_IN].getChannels();
        int numArgumentInputChannels = inputs[ARGUMENT_IN].getChannels();

        int maxRectInput = std::max(numRealInputChannels,numImaginaryInputChannels);
        int maxPolarInput = std::max(numModulusInputChannels,numArgumentInputChannels);

        int numRectInOutputChannels =  compolyChannelsRectIn * 2;
        int numPolarInOutputChannels = compolyChannelsPolarIn * 2;

        int numRectInOutChannels1 = numRectInOutputChannels >= 16 ? 16 : numRectInOutputChannels;
        int numRectInOutChannels2 = numRectInOutputChannels >= 16 ? numRectInOutputChannels-16 : 0;

        outputs[RECT_IN_RECT_OUT+0].setChannels(numRectInOutChannels1);
        outputs[RECT_IN_RECT_OUT+1].setChannels(numRectInOutChannels2);

        outputs[RECT_IN_POLAR_OUT+0].setChannels(numRectInOutChannels1);
        outputs[RECT_IN_POLAR_OUT+1].setChannels(numRectInOutChannels2);

        int numPolarInOutChannels1 = numPolarInOutputChannels >= 16 ? 16 : numPolarInOutputChannels;
        int numPolarInOutChannels2 = numPolarInOutputChannels >= 16 ? numPolarInOutputChannels-16 : 0;

        outputs[POLAR_IN_RECT_OUT+0].setChannels(numPolarInOutChannels1);
        outputs[POLAR_IN_RECT_OUT+1].setChannels(numPolarInOutChannels2);

        outputs[POLAR_IN_POLAR_OUT+0].setChannels(numPolarInOutChannels1);
        outputs[POLAR_IN_POLAR_OUT+1].setChannels(numPolarInOutChannels2);

        float realOffsetKnob = params[REAL_INPUT_OFFSET].getValue();
        float realTrimKnob = params[REAL_INPUT_TRIM].getValue();

        float imaginaryOffsetKnob = params[IMAGINARY_INPUT_OFFSET].getValue();
        float imaginaryTrimKnob = params[IMAGINARY_INPUT_TRIM].getValue();

        float modulusOffsetKnob = params[MODULUS_INPUT_OFFSET].getValue();
        float modulusTrimKnob = params[MODULUS_INPUT_TRIM].getValue();

        float argumentOffsetKnob = params[ARGUMENT_INPUT_OFFSET].getValue();
        float argumentTrimKnob = params[ARGUMENT_INPUT_TRIM].getValue();

        for(int rectInputCh = 0; rectInputCh < numRectInOutputChannels; rectInputCh++) {
            int outputBlock = rectInputCh > 7 ? 1 : 0;

            std::array<int,2> channelIndices = getInputChannels(rectInputCh,numRealInputChannels,numImaginaryInputChannels);

            int realInputCh=channelIndices[0];
            int imInputCh=channelIndices[1];

            float x = inputs[REAL_IN].getVoltage(realInputCh)*realTrimKnob + realOffsetKnob;
            float y = inputs[IMAGINARY_IN].getVoltage(imInputCh)*imaginaryTrimKnob + imaginaryOffsetKnob;

            outputs[RECT_IN_RECT_OUT + outputBlock].setVoltage(x,rectInputCh*2 % 16);
            outputs[RECT_IN_RECT_OUT + outputBlock].setVoltage(y,(rectInputCh*2+1) % 16);

            float r = std::hypot(x,y);
            float arg = std::atan2(y,x);

            outputs[RECT_IN_POLAR_OUT + outputBlock].setVoltage(r,rectInputCh*2 % 16);
            outputs[RECT_IN_POLAR_OUT + outputBlock].setVoltage(arg,(rectInputCh*2+1) % 16);
        }

        for(int polarInputCh = 0; polarInputCh < compolyChannelsPolarIn; polarInputCh++) {
            int outputBlock = polarInputCh > 7 ? 1 : 0;

            std::array<int,2> channelIndices = getInputChannels(polarInputCh,numModulusInputChannels,numArgumentInputChannels);

            int modInputChannel=channelIndices[0];
            int argInputChannel=channelIndices[1];

            float r = inputs[MODULUS_IN].getVoltage(modInputChannel)*modulusTrimKnob + modulusOffsetKnob;
            float theta = inputs[ARGUMENT_IN].getVoltage(argInputChannel)*argumentTrimKnob + argumentOffsetKnob;

            outputs[POLAR_IN_POLAR_OUT + outputBlock].setVoltage(r,polarInputCh*2 % 16);
            outputs[POLAR_IN_POLAR_OUT + outputBlock].setVoltage(theta,(polarInputCh*2+1) % 16);

            float x = r*std::cos(theta);
            float y = r*std::sin(theta);

            outputs[POLAR_IN_RECT_OUT + outputBlock].setVoltage(x,polarInputCh*2 % 16);
            outputs[POLAR_IN_RECT_OUT + outputBlock].setVoltage(y,(polarInputCh*2+1) % 16);
        }  
    }

    json_t *dataToJson() override {
        json_t *rootJ = json_object();

       // json_t *sequenceJ = json_string(currentFormula.c_str());

       // json_object_set_new(rootJ, "sequences", sequenceJ);

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override {
        /*std::string val;
        json_t *textJ = json_object_get(rootJ, "sequences");
        if (textJ) {
            currentFormula = json_string_value(textJ);
            manualSet = true;
        }*/

    }

   };



struct ComputerscareNomplexPumbersWidget : ModuleWidget
{

    ComputerscareNomplexPumbersWidget(ComputerscareNomplexPumbers *module)
    {
        setModule(module);
        box.size = Vec(10 * 15, 380);
        {
            ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
            panel->box.size = box.size;
            panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareNomplexCumbersPanel.svg")));

            addChild(panel);

        }

     

        const int rectInSectionY = 70;
        const int polarInSectionY = 220;

        const int leftInputX = 25;
        const int rightInputX = 100;

        const int output1X = 50;
        const int output2X = 100;

        Vec offsetRelPos = Vec(0,-25.f);
        Vec trimRelPos = Vec(-20,0.f);

        Vec channelsKnobRelPos = Vec(-25.f,0);


        addInput(createInput<InPort>(Vec(leftInputX, rectInSectionY), module, ComputerscareNomplexPumbers::REAL_IN));
        addParam(createParam<SmoothKnob>(Vec(leftInputX, rectInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::REAL_INPUT_OFFSET));
        addParam(createParam<SmallKnob>(Vec(leftInputX, rectInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::REAL_INPUT_TRIM));
        
        
        addInput(createInput<InPort>(Vec(rightInputX, rectInSectionY), module, ComputerscareNomplexPumbers::IMAGINARY_IN));
        addParam(createParam<SmoothKnob>(Vec(rightInputX, rectInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::IMAGINARY_INPUT_OFFSET));
        addParam(createParam<SmallKnob>(Vec(rightInputX, rectInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::IMAGINARY_INPUT_TRIM));

        addOutput(createOutput<OutPort>(Vec(output1X, rectInSectionY+30), module, ComputerscareNomplexPumbers::RECT_IN_RECT_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, rectInSectionY+30), module, ComputerscareNomplexPumbers::RECT_IN_RECT_OUT+1 ));

        addOutput(createOutput<OutPort>(Vec(output1X, rectInSectionY+70), module, ComputerscareNomplexPumbers::RECT_IN_POLAR_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, rectInSectionY+70), module, ComputerscareNomplexPumbers::RECT_IN_POLAR_OUT+1 ));

        rectInChannelWidget = new PolyOutputChannelsWidget(Vec(output1X, rectInSectionY+30).plus(channelsKnobRelPos), module, ComputerscareNomplexPumbers::COMPOLY_CHANNELS_RECT_IN,&module->compolyChannelsRectIn);

        addChild(rectInChannelWidget);

        addInput(createInput<InPort>(Vec(leftInputX, polarInSectionY), module, ComputerscareNomplexPumbers::MODULUS_IN));
        addParam(createParam<SmoothKnob>(Vec(leftInputX, polarInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::MODULUS_INPUT_OFFSET));
        addParam(createParam<SmallKnob>(Vec(leftInputX, polarInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::MODULUS_INPUT_TRIM));
        

        addInput(createInput<InPort>(Vec(rightInputX, polarInSectionY), module, ComputerscareNomplexPumbers::ARGUMENT_IN));
        addParam(createParam<SmoothKnob>(Vec(rightInputX, polarInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::ARGUMENT_INPUT_OFFSET));
        addParam(createParam<SmallKnob>(Vec(rightInputX, polarInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::ARGUMENT_INPUT_TRIM));

        addOutput(createOutput<OutPort>(Vec(output1X, polarInSectionY+30), module, ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, polarInSectionY+30), module, ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT+1 ));

        addOutput(createOutput<OutPort>(Vec(output1X, polarInSectionY+70), module, ComputerscareNomplexPumbers::POLAR_IN_POLAR_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, polarInSectionY+70), module, ComputerscareNomplexPumbers::POLAR_IN_POLAR_OUT+1 ));

        polarInChannelWidget = new PolyOutputChannelsWidget(Vec(output1X, polarInSectionY+30).plus(channelsKnobRelPos), module, ComputerscareNomplexPumbers::COMPOLY_CHANNELS_POLAR_IN,&module->compolyChannelsPolarIn);

        addChild(polarInChannelWidget);
       
        nomplex = module;
    }

    void appendContextMenu(Menu* menu) override {
        ComputerscareNomplexPumbers* pumbersModule = dynamic_cast<ComputerscareNomplexPumbers*>(this->nomplex);


        wrapModeMenu = new ParamSelectMenu();
        wrapModeMenu->text = "Polyphonic Wrap Mode";
        wrapModeMenu->rightText = RIGHT_ARROW;
        wrapModeMenu->param = pumbersModule->paramQuantities[ComputerscareNomplexPumbers::WRAP_MODE];
        wrapModeMenu->options = wrapModeDescriptions;

        menu->addChild(new MenuEntry);
        menu->addChild(wrapModeMenu);

    }

    PolyOutputChannelsWidget* rectInChannelWidget;
    PolyOutputChannelsWidget* polarInChannelWidget;

    ParamSelectMenu *wrapModeMenu;
    ComputerscareNomplexPumbers *nomplex;
   

};
Model *modelComputerscareNomplexPumbers = createModel<ComputerscareNomplexPumbers, ComputerscareNomplexPumbersWidget>("computerscare-nomplex-pumbers");
