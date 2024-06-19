#include "Computerscare.hpp"

std::vector<std::string> wrapModeDescriptions;



struct ComputerscareNomplexPumbers : ComputerscareMenuParamModule
{
    enum ParamIds
    {   
        WRAP_MODE,
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



    ComputerscareNomplexPumbers()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        wrapModeDescriptions.push_back("Normal (Standard Polyphonic Behavior)");
        wrapModeDescriptions.push_back("Cycle (Repeat Channels)");
        wrapModeDescriptions.push_back("Minimal (Pad with 0v)");
        wrapModeDescriptions.push_back("Stall (Pad with final voltage)");

        configMenuParam(WRAP_MODE, 0.f, "Polyphonic Wrapping Mode", wrapModeDescriptions);
        getParamQuantity(WRAP_MODE)->randomizeEnabled = false;

        configInput(REAL_IN,"Real");
        configInput(IMAGINARY_IN,"Imaginary");
        configInput(MODULUS_IN,"Modulus (Length)");
        configInput(ARGUMENT_IN,"Argument in Radians");

        configOutput(RECT_IN_RECT_OUT, "Rectangular Input, Rectangular #1-8");
        configOutput(RECT_IN_RECT_OUT + 1, "Rectangular Input, Rectangular #9-16");

        configOutput(RECT_IN_POLAR_OUT, "Rectangular Input, Polar #1-8");
        configOutput(RECT_IN_POLAR_OUT + 1, "Rectangular Input, Polar #9-16");

        configOutput(POLAR_IN_RECT_OUT, "Polar Input, Rectangular #1-8");
        configOutput(POLAR_IN_RECT_OUT + 1, "Polar Input, Rectangular #9-16");

        configOutput(POLAR_IN_POLAR_OUT, "Polar Input, Polar #1-8");
        configOutput(POLAR_IN_POLAR_OUT + 1, "Polar Input, Polar #9-16");
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
        int numRealInputChannels = inputs[REAL_IN].getChannels();
        int numImaginaryInputChannels = inputs[IMAGINARY_IN].getChannels();
        int numModulusInputChannels = inputs[MODULUS_IN].getChannels();
        int numArgumentInputChannels = inputs[ARGUMENT_IN].getChannels();

        int maxRectInput = std::max(numRealInputChannels,numImaginaryInputChannels);
        int maxPolarInput = std::max(numModulusInputChannels,numArgumentInputChannels);

        int numRectInOutputChannels = maxRectInput * 2;
        int numPolarInOutputChannels = maxPolarInput * 2;

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

        for(int rectInputCh = 0; rectInputCh < maxRectInput; rectInputCh++) {
            int outputBlock = rectInputCh > 7 ? 1 : 0;

            std::array<int,2> channelIndices = getInputChannels(rectInputCh,numRealInputChannels,numImaginaryInputChannels);

            int realInputCh=channelIndices[0];
            int imInputCh=channelIndices[1];

            float x = inputs[REAL_IN].getVoltage(realInputCh);
            float y = inputs[IMAGINARY_IN].getVoltage(imInputCh);

            outputs[RECT_IN_RECT_OUT + outputBlock].setVoltage(x,rectInputCh*2 % 16);
            outputs[RECT_IN_RECT_OUT + outputBlock].setVoltage(y,(rectInputCh*2+1) % 16);

            float r = std::hypot(x,y);
            float arg = std::atan2(y,x);

            outputs[RECT_IN_POLAR_OUT + outputBlock].setVoltage(r,rectInputCh*2 % 16);
            outputs[RECT_IN_POLAR_OUT + outputBlock].setVoltage(arg,(rectInputCh*2+1) % 16);
        }

        for(int polarInputCh = 0; polarInputCh < maxPolarInput; polarInputCh++) {
            int outputBlock = polarInputCh > 7 ? 1 : 0;

            std::array<int,2> channelIndices = getInputChannels(polarInputCh,numModulusInputChannels,numArgumentInputChannels);

            int modInputChannel=channelIndices[0];
            int argInputChannel=channelIndices[1];

            float r = inputs[MODULUS_IN].getVoltage(modInputChannel);
            float theta = inputs[ARGUMENT_IN].getVoltage(argInputChannel);

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

        const int leftInputX = 25;
        const int rightInputX = 85;

        const int output1X = 50;
        const int output2X = 90;


        addInput(createInput<InPort>(Vec(leftInputX, 50), module, ComputerscareNomplexPumbers::REAL_IN));
        addInput(createInput<InPort>(Vec(rightInputX, 50), module, ComputerscareNomplexPumbers::IMAGINARY_IN));

        addOutput(createOutput<OutPort>(Vec(output1X, 80), module, ComputerscareNomplexPumbers::RECT_IN_RECT_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, 80), module, ComputerscareNomplexPumbers::RECT_IN_RECT_OUT+1 ));

        addOutput(createOutput<OutPort>(Vec(output1X, 120), module, ComputerscareNomplexPumbers::RECT_IN_POLAR_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, 120), module, ComputerscareNomplexPumbers::RECT_IN_POLAR_OUT+1 ));

        addInput(createInput<InPort>(Vec(leftInputX, 200), module, ComputerscareNomplexPumbers::MODULUS_IN));
        addInput(createInput<InPort>(Vec(rightInputX, 200), module, ComputerscareNomplexPumbers::ARGUMENT_IN));

        addOutput(createOutput<OutPort>(Vec(output1X, 230), module, ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, 230), module, ComputerscareNomplexPumbers::POLAR_IN_RECT_OUT+1 ));

        addOutput(createOutput<OutPort>(Vec(output1X, 270), module, ComputerscareNomplexPumbers::POLAR_IN_POLAR_OUT ));
        addOutput(createOutput<OutPort>(Vec(output2X, 270), module, ComputerscareNomplexPumbers::POLAR_IN_POLAR_OUT+1 ));
       
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

    ParamSelectMenu *wrapModeMenu;
    ComputerscareNomplexPumbers *nomplex;
   

};
Model *modelComputerscareNomplexPumbers = createModel<ComputerscareNomplexPumbers, ComputerscareNomplexPumbersWidget>("computerscare-nomplex-pumbers");