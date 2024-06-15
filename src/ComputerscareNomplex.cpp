#include "Computerscare.hpp"

#include "dtpulse.hpp"


#include <string>
#include <sstream>
#include <iomanip>


struct ComputerscareNomplexPumbers;

struct ComputerscareNomplexPumbers : Module
{
    enum ParamIds
    {
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





    ComputerscareNomplexPumbers()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

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
    void process(const ProcessArgs &args) override;

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


void ComputerscareNomplexPumbers::process(const ProcessArgs &args)
{
   
}


struct ComputerscareNomplexPumbersWidget : ModuleWidget
{
    float randAmt = 0.f;

    ComputerscareNomplexPumbersWidget(ComputerscareNomplexPumbers *module)
    {
        setModule(module);
        //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareNomplexPumbersPanel.svg")));
        box.size = Vec(10 * 15, 380);
        {
            //
            ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
            panel->box.size = box.size;
            panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareNomplexCumbersPanel.svg")));

            //module->panelRef = panel;
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

        



       /* for (int i = 0; i < numChannels; i++)
        {

            xx = x + dx * i + randAmt * (2 * random::uniform() - .5);
            y += randAmt * (random::uniform() - .5);
            addInput(createInput<InPort>(mm2px(Vec(xx, y - 0.8)), module, ComputerscareOhPeas::CHANNEL_INPUT + i));

            addParam(createParam<SmallKnob>(mm2px(Vec(xx + 2, y + 34)), module, ComputerscareOhPeas::SCALE_TRIM + i));

            addInput(createInput<InPort>(mm2px(Vec(xx, y + 40)),  module, ComputerscareOhPeas::SCALE_CV + i));

            addParam(createParam<SmoothKnob>(mm2px(Vec(xx, y + 50)), module, ComputerscareOhPeas::SCALE_VAL + i));

            addParam(createParam<ComputerscareDotKnob>(mm2px(Vec(xx + 2, y + 64)), module, ComputerscareOhPeas::OFFSET_TRIM + i));

            addInput(createInput<InPort>(mm2px(Vec(xx, y + 70)),  module, ComputerscareOhPeas::OFFSET_CV + i));


            addParam(createParam<SmoothKnob>(mm2px(Vec(xx, y + 80)), module, ComputerscareOhPeas::OFFSET_VAL + i));

            addOutput(createOutput<OutPort>(mm2px(Vec(xx, y + 93)), module, ComputerscareOhPeas::SCALED_OUTPUT + i));

            addOutput(createOutput<InPort>(mm2px(Vec(xx + 1, y + 108)),  module, ComputerscareOhPeas::QUANTIZED_OUTPUT + i));

        }*/
       
        peas = module;
    }


    ComputerscareNomplexPumbers *peas;
   

};
Model *modelComputerscareNomplexPumbers = createModel<ComputerscareNomplexPumbers, ComputerscareNomplexPumbersWidget>("computerscare-nomplex-pumbers");
