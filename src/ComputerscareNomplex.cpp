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
        WRAP_MINIMUM,
        WRAP_STALL
    };



    ComputerscareNomplexPumbers()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        wrapModeDescriptions.push_back("Normal (Standard Polyphonic Behavior)");
        wrapModeDescriptions.push_back("Cycle (Repeat Channels)");
        wrapModeDescriptions.push_back("Minimum (Pad with 0v)");
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

        for(int rectInputCh = 0; rectInputCh < maxRectInput; rectInputCh++) {
            int outputBlock = rectInputCh > 7 ? 1 : 0;

            int realInputCh,imInputCh;

            int wrapMode = params[WRAP_MODE].getValue();

            if(wrapMode == WRAP_NORMAL) {
                if(numRealInputChannels==1) {
                    realInputCh=0;
                } else {
                    realInputCh=rectInputCh;
                }
                if(numImaginaryInputChannels==1) {
                    imInputCh=0;
                } else {
                    imInputCh=rectInputCh;
                }

            } else if(wrapMode == WRAP_CYCLE) {
                realInputCh = rectInputCh % numRealInputChannels;
                imInputCh = rectInputCh % numImaginaryInputChannels;

            } else if(wrapMode == WRAP_MINIMUM) {
                realInputCh=rectInputCh;
                imInputCh=rectInputCh;
                
            } else if(wrapMode == WRAP_STALL) {
                realInputCh=rectInputCh>=numRealInputChannels ? numRealInputChannels-1 : rectInputCh;
                imInputCh=rectInputCh>=numImaginaryInputChannels ? numImaginaryInputChannels-1 : rectInputCh;
            }

            float x = inputs[REAL_IN].getVoltage(realInputCh);
            float y = inputs[IMAGINARY_IN].getVoltage(imInputCh);

            outputs[RECT_IN_RECT_OUT + outputBlock].setVoltage(x,rectInputCh*2 % 16);
            outputs[RECT_IN_RECT_OUT + outputBlock].setVoltage(y,(rectInputCh*2+1) % 16);
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
