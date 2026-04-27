#include "Computerscare.hpp"
#include "complex/ComplexWidgets.hpp"

#include <array>

std::vector<std::string> wrapModeDescriptions = {"Normal (Standard Polyphonic Behavior)","Cycle (Repeat Channels)","Minimal (Pad with 0v)","Stall (Pad with final voltage)"};


struct ComputerscareNomplexPumbers : ComputerscareComplexBase
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
        RECT_IN_RECT_OUT_MODE,
        RECT_IN_POLAR_OUT_MODE,
        POLAR_IN_RECT_OUT_MODE,
        POLAR_IN_POLAR_OUT_MODE,
        COMPLEX_CONSTANT_A,
        COMPLEX_CONSTANT_B,
        AB_MODE,
        COMPLEX_CONSTANT_U,
        COMPLEX_CONSTANT_V,
        UV_MODE,
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

    

    int compolyChannelsRectIn = 16;
    int compolyChannelsPolarIn = 16;

    ComputerscareNomplexPumbers()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

      

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

        configParam<cpx::CompolyModeParam>(RECT_IN_RECT_OUT_MODE,0.f,3.f,0.f,"Rect Output 1 Mode");
        configParam<cpx::CompolyModeParam>(RECT_IN_POLAR_OUT_MODE,0.f,3.f,0.f,"Rect Output 2 Mode");
        configParam<cpx::CompolyModeParam>(POLAR_IN_RECT_OUT_MODE,0.f,3.f,0.f,"Polar Output 1 Mode");
        configParam<cpx::CompolyModeParam>(POLAR_IN_POLAR_OUT_MODE,0.f,3.f,0.f,"Polar Output 2 Mode");

        configParam(COMPLEX_CONSTANT_A,-10.f,10.f,0.f,"X Input Offset");
        configParam(COMPLEX_CONSTANT_B,-10.f,10.f,0.f,"Y Input Offset");
        configParam(AB_MODE,0.f,1.f,0.f,"AB Mode");

        configParam(COMPLEX_CONSTANT_U,-10.f,10.f,0.f,"Complex U");
        configParam(COMPLEX_CONSTANT_V,-10.f,10.f,0.f,"Complex V");
        configParam(UV_MODE,0.f,1.f,1.f,"UV Mode");

        configOutput<cpx::CompolyPortInfo<RECT_IN_RECT_OUT_MODE,0>>(RECT_IN_RECT_OUT, "Rectangular Input");
        configOutput<cpx::CompolyPortInfo<RECT_IN_RECT_OUT_MODE,1>>(RECT_IN_RECT_OUT + 1, "Rectangular Input");

        configOutput<cpx::CompolyPortInfo<RECT_IN_RECT_OUT_MODE+1,0>>(RECT_IN_POLAR_OUT, "Rectangular Input");
        configOutput<cpx::CompolyPortInfo<RECT_IN_RECT_OUT_MODE+1,1>>(RECT_IN_POLAR_OUT + 1, "Rectangular Input");

        configOutput<cpx::CompolyPortInfo<POLAR_IN_RECT_OUT_MODE,0>>(POLAR_IN_RECT_OUT, "Polar Input");
        configOutput<cpx::CompolyPortInfo<POLAR_IN_RECT_OUT_MODE,1>>(POLAR_IN_RECT_OUT + 1, "Polar Input");

        configOutput<cpx::CompolyPortInfo<POLAR_IN_RECT_OUT_MODE+1,0>>(POLAR_IN_POLAR_OUT, "Polar Input");
        configOutput<cpx::CompolyPortInfo<POLAR_IN_RECT_OUT_MODE+1,1>>(POLAR_IN_POLAR_OUT + 1, "Polar Input");
    }


    void checkPoly() override {

        int rectInputCompolyphonyKnobSetting = params[COMPOLY_CHANNELS_RECT_IN].getValue();

        int numRealInputChannels = inputs[REAL_IN].getChannels();
        int numImaginaryInputChannels = inputs[IMAGINARY_IN].getChannels();

        compolyChannelsRectIn = calcOutputCompolyphony(rectInputCompolyphonyKnobSetting,{{numRealInputChannels,numImaginaryInputChannels}});
 
        int polarInputCompolyphonyKnobSetting = params[COMPOLY_CHANNELS_POLAR_IN].getValue();

        int numModulusInputChannels = inputs[MODULUS_IN].getChannels();
        int numArgumentInputChannels = inputs[ARGUMENT_IN].getChannels();

        compolyChannelsPolarIn = calcOutputCompolyphony(polarInputCompolyphonyKnobSetting,{{numModulusInputChannels,numArgumentInputChannels}});
 
    }

   

    void process(const ProcessArgs &args) override {
        ComputerscarePolyModule::checkCounter();

        int numRealInputChannels = inputs[REAL_IN].getChannels();
        int numImaginaryInputChannels = inputs[IMAGINARY_IN].getChannels();
        int numModulusInputChannels = inputs[MODULUS_IN].getChannels();
        int numArgumentInputChannels = inputs[ARGUMENT_IN].getChannels();

        int out1mode = params[RECT_IN_RECT_OUT_MODE].getValue();
        int out2mode = params[RECT_IN_RECT_OUT_MODE+1].getValue();
        int out3mode = params[RECT_IN_RECT_OUT_MODE+2].getValue();
        int out4mode = params[RECT_IN_RECT_OUT_MODE+3].getValue();

        setOutputChannels(RECT_IN_RECT_OUT,out1mode,compolyChannelsRectIn);
        setOutputChannels(RECT_IN_POLAR_OUT,out2mode,compolyChannelsRectIn);

        setOutputChannels(POLAR_IN_RECT_OUT,out3mode,compolyChannelsPolarIn);
        setOutputChannels(POLAR_IN_POLAR_OUT,out4mode,compolyChannelsPolarIn);

        float xyParamX = params[COMPLEX_CONSTANT_A].getValue();
        float xyParamY = params[COMPLEX_CONSTANT_A+1].getValue();

        float rtParamX = params[COMPLEX_CONSTANT_U].getValue();
        float rtParamY = params[COMPLEX_CONSTANT_U+1].getValue();

        float realOffsetKnob = params[REAL_INPUT_OFFSET].getValue();
        float realTrimKnob = params[REAL_INPUT_TRIM].getValue();

        float imaginaryOffsetKnob = params[IMAGINARY_INPUT_OFFSET].getValue();
        float imaginaryTrimKnob = params[IMAGINARY_INPUT_TRIM].getValue();

        float modulusOffsetKnob = params[MODULUS_INPUT_OFFSET].getValue();
        float modulusTrimKnob = params[MODULUS_INPUT_TRIM].getValue();

        float argumentOffsetKnob = params[ARGUMENT_INPUT_OFFSET].getValue();
        float argumentTrimKnob = params[ARGUMENT_INPUT_TRIM].getValue();

        int wrapMode = params[WRAP_MODE].getValue();

        for(int rectInputCh = 0; rectInputCh < compolyChannelsRectIn; rectInputCh++) {
            std::vector<int> inputChannelIndices = getChannelIndicesFromSeparatedInput(rectInputCh,wrapMode,{numRealInputChannels,numImaginaryInputChannels});

            int realInputCh=inputChannelIndices[0];
            int imInputCh=inputChannelIndices[1];

            float x = inputs[REAL_IN].getVoltage(realInputCh)*realTrimKnob  + xyParamX;
            float y = inputs[IMAGINARY_IN].getVoltage(imInputCh)*imaginaryTrimKnob  + xyParamY;

            float r = std::hypot(x,y);
            float arg = std::atan2(y,x);

            setOutputVoltages(RECT_IN_RECT_OUT,out1mode,rectInputCh,x,y,r,arg);
            setOutputVoltages(RECT_IN_POLAR_OUT,out2mode,rectInputCh,x,y,r,arg);
        }

        for(int polarInputCh = 0; polarInputCh < compolyChannelsPolarIn; polarInputCh++) {
            std::vector<int> inputChannelIndices = getChannelIndicesFromSeparatedInput(polarInputCh,wrapMode,{numModulusInputChannels,numArgumentInputChannels});

            int modInputChannel=inputChannelIndices[0];
            int argInputChannel=inputChannelIndices[1];

            float r0 = inputs[MODULUS_IN].getVoltage(modInputChannel)*modulusTrimKnob + modulusOffsetKnob;
            float theta0 = inputs[ARGUMENT_IN].getVoltage(argInputChannel)*argumentTrimKnob + argumentOffsetKnob;

            float x = r0*std::cos(theta0)+rtParamX;
            float y = r0*std::sin(theta0)+rtParamY;

            float r = std::hypot(x,y);
            float theta = std::atan2(y,x);

            setOutputVoltages(POLAR_IN_RECT_OUT,out3mode,polarInputCh,x,y,r,theta);
            setOutputVoltages(POLAR_IN_POLAR_OUT,out4mode,polarInputCh,x,y,r,theta);

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

struct NomplexWrapModeMenu : MenuItem {
    ParamQuantity* param;
    std::vector<std::string> options;

    Menu* createChildMenu() override {
        Menu* menu = new Menu;
        for (int i = 0; i < (int) options.size(); i++) {
            ParamSettingItem* item = new ParamSettingItem(i, &param->module->params[param->paramId]);
            item->text = options[i];
            menu->addChild(item);
        }
        return menu;
    }

    void step() override {
        int index = clamp((int) param->getValue(), 0, (int) options.size() - 1);
        rightText = "(" + options[index] + ") " + RIGHT_ARROW;
        MenuItem::step();
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
            panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ComputerscareNomplexCumbersPanel.svg")));

            addChild(panel);

        }

     

        const int rectInSectionY = 70;
        const int polarInSectionY = 240;

        const int leftInputX = 25;
        const int rightInputX = 100;

        const int output1X = 50;
        const int output2X = 100;

        Vec offsetRelPos = Vec(0,-25.f);
        Vec trimRelPos = Vec(-20,0.f);

        Vec channelsKnobRelPos = Vec(-50.f,25.f);

        cpx::ComplexXY* xy = new cpx::ComplexXY(module,ComputerscareNomplexPumbers::COMPLEX_CONSTANT_A);
        xy->box.size=Vec(30,30);
        xy->box.pos=Vec(60,30);
        addChild(xy);


        addInput(createInput<InPort>(Vec(leftInputX, rectInSectionY), module, ComputerscareNomplexPumbers::REAL_IN));
        addParam(createParam<SmoothKnob>(Vec(leftInputX, rectInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::COMPLEX_CONSTANT_A));
        addParam(createParam<SmallKnob>(Vec(leftInputX, rectInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::REAL_INPUT_TRIM));
        
        
        addInput(createInput<InPort>(Vec(rightInputX, rectInSectionY), module, ComputerscareNomplexPumbers::IMAGINARY_IN));
        addParam(createParam<SmoothKnob>(Vec(rightInputX, rectInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::COMPLEX_CONSTANT_B));
        addParam(createParam<SmallKnob>(Vec(rightInputX, rectInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::IMAGINARY_INPUT_TRIM));

        cpx::CompolyPortsWidget* outRect1 = new cpx::CompolyPortsWidget(Vec(output1X, rectInSectionY+40),module,ComputerscareNomplexPumbers::RECT_IN_RECT_OUT,ComputerscareNomplexPumbers::RECT_IN_RECT_OUT_MODE);
        addChild(outRect1);


         cpx::CompolyPortsWidget* outRect2 = new cpx::CompolyPortsWidget(Vec(output1X, rectInSectionY+90),module,ComputerscareNomplexPumbers::RECT_IN_POLAR_OUT,ComputerscareNomplexPumbers::RECT_IN_RECT_OUT_MODE+1);
        addChild(outRect2);

        rectInChannelWidget = new PolyOutputChannelsWidget(Vec(output2X+64, rectInSectionY+26).plus(channelsKnobRelPos), module, ComputerscareNomplexPumbers::COMPOLY_CHANNELS_RECT_IN,&module->compolyChannelsRectIn);

        addChild(rectInChannelWidget);

        cpx::ComplexXY* uv = new cpx::ComplexXY(module,ComputerscareNomplexPumbers::COMPLEX_CONSTANT_U);
        uv->box.size=Vec(30,30);
        uv->box.pos=Vec(60,200);
        addChild(uv);

        addInput(createInput<InPort>(Vec(leftInputX, polarInSectionY), module, ComputerscareNomplexPumbers::MODULUS_IN));
        addParam(createParam<SmoothKnob>(Vec(leftInputX, polarInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::MODULUS_INPUT_OFFSET));
        addParam(createParam<SmallKnob>(Vec(leftInputX, polarInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::MODULUS_INPUT_TRIM));
        

        addInput(createInput<InPort>(Vec(rightInputX, polarInSectionY), module, ComputerscareNomplexPumbers::ARGUMENT_IN));
        addParam(createParam<SmoothKnob>(Vec(rightInputX, polarInSectionY).plus(offsetRelPos), module, ComputerscareNomplexPumbers::ARGUMENT_INPUT_OFFSET));
        addParam(createParam<SmallKnob>(Vec(rightInputX, polarInSectionY).plus(trimRelPos), module, ComputerscareNomplexPumbers::ARGUMENT_INPUT_TRIM));

          cpx::CompolyPortsWidget* outPolar1 = new cpx::CompolyPortsWidget(Vec(output1X, polarInSectionY+40),module,ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT,ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT_MODE);
        addChild(outPolar1);


          cpx::CompolyPortsWidget* outPolar2 = new cpx::CompolyPortsWidget(Vec(output1X, polarInSectionY+90),module,ComputerscareNomplexPumbers::POLAR_IN_POLAR_OUT,ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT_MODE+1);
        addChild(outPolar2);

        polarInChannelWidget = new PolyOutputChannelsWidget(Vec(output2X+64, polarInSectionY+30).plus(channelsKnobRelPos), module, ComputerscareNomplexPumbers::COMPOLY_CHANNELS_POLAR_IN,&module->compolyChannelsPolarIn);

        addChild(polarInChannelWidget);
       
        nomplex = module;
    }

    void appendContextMenu(Menu* menu) override {
        ComputerscareNomplexPumbers* pumbersModule = dynamic_cast<ComputerscareNomplexPumbers*>(this->nomplex);


        wrapModeMenu = new NomplexWrapModeMenu();
        wrapModeMenu->text = "Polyphonic Wrap Mode";
        wrapModeMenu->rightText = RIGHT_ARROW;
        wrapModeMenu->param = pumbersModule->paramQuantities[ComputerscareNomplexPumbers::WRAP_MODE];
        wrapModeMenu->options = wrapModeDescriptions;

        menu->addChild(new MenuEntry);
        menu->addChild(wrapModeMenu);

    }

    PolyOutputChannelsWidget* rectInChannelWidget;
    PolyOutputChannelsWidget* polarInChannelWidget;

    NomplexWrapModeMenu *wrapModeMenu;
    ComputerscareNomplexPumbers *nomplex;
   

};
Model *modelComputerscareNomplexPumbers = createModel<ComputerscareNomplexPumbers, ComputerscareNomplexPumbersWidget>("computerscare-nomplex-pumbers");
