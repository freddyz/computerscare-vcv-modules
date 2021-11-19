#include "Computerscare.hpp"

#include "dtpulse.hpp"


#include <string>
#include <sstream>
#include <iomanip>

struct ComputerscareOhPeas;

const int numChannels = 4;

struct ComputerscareOhPeas;

struct ComputerscareOhPeas : Module
{
    enum ParamIds
    {
        GLOBAL_TRANSPOSE,
        NUM_DIVISIONS,
        SCALE_TRIM,
        SCALE_VAL = SCALE_TRIM + numChannels,
        OFFSET_TRIM = SCALE_VAL + numChannels,
        OFFSET_VAL = OFFSET_TRIM + numChannels,
        NUM_PARAMS = OFFSET_VAL + numChannels

    };
    enum InputIds
    {
        CHANNEL_INPUT,
        SCALE_CV = CHANNEL_INPUT + numChannels,
        OFFSET_CV = SCALE_CV + numChannels,
        NUM_INPUTS = OFFSET_CV + numChannels
    };
    enum OutputIds
    {
        SCALED_OUTPUT,
        QUANTIZED_OUTPUT = SCALED_OUTPUT + numChannels,
        NUM_OUTPUTS = QUANTIZED_OUTPUT + numChannels
    };
    enum LightIds
    {
        BLINK_LIGHT,
        NUM_LIGHTS
    };



    int numDivisions = 12;
    int globalTranspose = 0;
    bool evenQuantizeMode = true;
    bool manualSet = true;

    int checkCounter = 9999;
    int checkPeriod = 1000;
    std::string currentFormula = "221222";
    std::string lastFormula = "52";


    std::string numDivisionsString = "";
    SmallLetterDisplay *numDivisionsDisplay;
    SmallLetterDisplay *globalTransposeDisplay;

    Quantizer quant;

    ComputerscareOhPeas()
    {

        enum InputIds
        {
            CHANNEL_INPUT,
            SCALE_CV = CHANNEL_INPUT + numChannels,
            OFFSET_CV = SCALE_CV + numChannels,
            NUM_INPUTS = OFFSET_CV + numChannels
        };
        enum OutputIds
        {
            SCALED_OUTPUT,
            QUANTIZED_OUTPUT = SCALED_OUTPUT + numChannels,
            NUM_OUTPUTS = QUANTIZED_OUTPUT + numChannels
        };


        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(GLOBAL_TRANSPOSE, -1.f, 1.f, 0.0f, "Global Transpose");
        configParam(NUM_DIVISIONS, 1.f, 24.f, 12.0f, "Number of Divisions");
        for (int i = 0; i < numChannels; i++)
        {
            std::string chi = "Column " + std::to_string(i + 1);
            configParam( SCALE_TRIM + i, -1.f, 1.f, 0.0f, chi + " Scale CV Amount");
            configParam( SCALE_VAL + i, -2.f, 2.f, 1.0f, chi + " Scale Value");
            configParam( OFFSET_TRIM + i, -1.f, 1.f, 0.0f, chi + " Offset CV Amount");
            configParam( OFFSET_VAL + i, -10.f, 10.f, 0.0f, chi + " Offset Value");

            configInput(CHANNEL_INPUT + i, chi);
            configInput(SCALE_CV + i, chi + " Scale");
            configInput(OFFSET_CV + i, chi + " Offset");

            configOutput(SCALED_OUTPUT + i, chi + " Non-Quantized");
            configOutput(QUANTIZED_OUTPUT + i, chi + " Quantized");

        }

    }
    void process(const ProcessArgs &args) override;

    json_t *dataToJson() override {
        json_t *rootJ = json_object();

        json_t *sequenceJ = json_string(currentFormula.c_str());

        json_object_set_new(rootJ, "sequences", sequenceJ);

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override {
        std::string val;
        json_t *textJ = json_object_get(rootJ, "sequences");
        if (textJ) {
            currentFormula = json_string_value(textJ);
            manualSet = true;
        }

    }

    void setQuant()
    {
        this->quant = Quantizer(this->currentFormula.c_str(), this->numDivisions, this->globalTranspose);
    }
    void checkForChange() {
        if (lastFormula != currentFormula) {
            setQuant();
        }
        lastFormula = currentFormula;
    }
    // For more advanced Module features, read Rack's engine.hpp header file
    // - toJson, fromJson: serialization of internal data
    // - onSampleRateChange: event triggered by a change of sample rate
    // - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void ComputerscareOhPeas::process(const ProcessArgs &args)
{
    if (checkCounter > checkPeriod) {
        checkForChange();
        checkCounter = 0;
    }
    checkCounter++;

    float A, B, C, D, Q, a, b, c, d;

    int numDivisionsKnobValue = floor(params[NUM_DIVISIONS].getValue());
    int iTranspose = floor(numDivisionsKnobValue * params[GLOBAL_TRANSPOSE].getValue());
    int numInputChannels;
    int numScaleCVChannels;
    int numOffsetCVChannels;

    if (numDivisionsKnobValue != numDivisions)
    {
        //what a hack!!!
        if (numDivisionsKnobValue != 0)
        {
            numDivisions = numDivisionsKnobValue;
            setQuant();
        }

    }
    if (iTranspose != globalTranspose)
    {
        globalTranspose = iTranspose;
        setQuant();
    }
    for (int i = 0; i < numChannels; i++)
    {
        if (outputs[SCALED_OUTPUT + i].isConnected() || outputs[QUANTIZED_OUTPUT + i].isConnected()) {
            numInputChannels = inputs[CHANNEL_INPUT + i].getChannels();
            numScaleCVChannels = inputs[SCALE_CV + i].getChannels();
            numOffsetCVChannels = inputs[OFFSET_CV + i].getChannels();
            outputs[SCALED_OUTPUT + i].setChannels(numInputChannels);
            outputs[QUANTIZED_OUTPUT + i].setChannels(numInputChannels);
            for (int ch = 0; ch < std::max(numInputChannels, 1); ch++) {


                a = params[SCALE_VAL + i].getValue();

                b = params[SCALE_TRIM + i].getValue();
                B = inputs[SCALE_CV + i].getVoltage(numScaleCVChannels == 1 ? 0 : ch);
                A = inputs[CHANNEL_INPUT + i].getVoltage(ch);

                c = params[OFFSET_TRIM + i].getValue();
                C = inputs[OFFSET_CV + i].getVoltage(numOffsetCVChannels == 1 ? 0 : ch);
                d = params[OFFSET_VAL + i].getValue();

                D = (b * B + a) * A + (c * C + d);

                Q = quant.quantizeEven(D, iTranspose);

                outputs[SCALED_OUTPUT + i].setVoltage(D, ch);
                outputs[QUANTIZED_OUTPUT + i].setVoltage(Q, ch);
            }
        }
    }
}

struct SetQuantizationModeMenuItem : MenuItem
{
    ComputerscareOhPeas *peas;

    bool mode = true;
    SetQuantizationModeMenuItem(bool evenMode)
    {
        mode = evenMode;
    }
    void doAction()
    {
        peas->evenQuantizeMode = mode;
    }
    void step() override
    {
        rightText = CHECKMARK(peas->evenQuantizeMode == mode);
        MenuItem::step();
    }
};
struct PeasTF2 : ComputerscareTextField
{
    ComputerscareOhPeas *module;
    int fontSize = 16;
    int rowIndex = 0;
    bool inError = false;

    PeasTF2()
    {
        ComputerscareTextField();
    };
    void draw(const DrawArgs &args) override
    {
        if (module)
        {
            if (module->manualSet) {
                text = module->currentFormula;
                module->manualSet = false;
            }
            if (text.c_str() != module->currentFormula)
            {
                module->currentFormula = text.c_str();
            }

        }
        else {
            text = "2212221";
        }
        ComputerscareTextField::draw(args);
    }

    //void draw(const DrawArgs &args) override;
    //int getTextPosition(math::Vec mousePos) override;
};
struct PeasSmallDisplay : SmallLetterDisplay
{
    ComputerscareOhPeas *module;
    int type;
    PeasSmallDisplay(int t)
    {
        type = t;
        SmallLetterDisplay();
    };
    void draw(const DrawArgs &args)
    {
        //this->setNumDivisionsString();
        if (module)
        {
            if (type == 0)
            {

                std::string transposeString =  (module->globalTranspose > 0 ? "+" : "" ) + std::to_string(module->globalTranspose);
                value = transposeString;
            }
            else
            {
                std::string numDivisionsDisplay = std::to_string(module->numDivisions);
                value = numDivisionsDisplay;
            }

        }
        else {
            value = std::to_string((random::u32() % 24) + 1);
        }
        SmallLetterDisplay::draw(args);
    }

};



void quantizationModeMenuItemAdd(ComputerscareOhPeas *peas, Menu *menu, bool evenMode, std::string label)
{
    SetQuantizationModeMenuItem *menuItem = new SetQuantizationModeMenuItem(evenMode);
    menuItem->text = label;
    menuItem->peas = peas;
    menu->addChild(menuItem);
}
//this->numDivisions,this->globalTranspose
struct ComputerscareOhPeasWidget : ModuleWidget
{
    float randAmt = 0.f;

    ComputerscareOhPeasWidget(ComputerscareOhPeas *module)
    {
        setModule(module);
        //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareOhPeasPanel.svg")));
        box.size = Vec(9 * 15, 380);
        {
            ComputerscareSVGPanel *panel = new ComputerscareSVGPanel();
            panel->box.size = box.size;
            panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareOhPeasPanel.svg")));

            //module->panelRef = panel;

            addChild(panel);

        }
        double x = 1;
        double y = 7;
        //double dy = 18.4;
        double dx = 9.95;
        double xx;
        double yy = 18;
        addParam(createParam<MediumSnapKnob>(mm2px(Vec(11, yy - 2)), module, ComputerscareOhPeas::NUM_DIVISIONS ));

        addParam(createParam<SmoothKnob>(mm2px(Vec(21, yy - 2)), module, ComputerscareOhPeas::GLOBAL_TRANSPOSE));

        textFieldTemp = createWidget<PeasTF2>(mm2px(Vec(x, y + 24)));
        textFieldTemp->module = module;
        textFieldTemp->box.size = mm2px(Vec(44, 7));
        textFieldTemp->multiline = false;
        textFieldTemp->color = nvgRGB(0xC0, 0xE7, 0xDE);
        addChild(textFieldTemp);

        ndd = new PeasSmallDisplay(1);
        ndd->module = module;
        ndd->box.pos = mm2px(Vec(2, yy));
        ndd->box.size = mm2px(Vec(9, 7));
        ndd->value = "";
        ndd->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
        addChild(ndd);

        transposeDisplay = new PeasSmallDisplay(0);
        transposeDisplay->module = module;

        transposeDisplay->box.pos = mm2px(Vec(30, yy));
        transposeDisplay->box.size = mm2px(Vec(11, 7));
        transposeDisplay->letterSpacing = 2.f;
        transposeDisplay->value = "";
        transposeDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
        addChild(transposeDisplay);

        for (int i = 0; i < numChannels; i++)
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

        }
        peas = module;
    }

    /*void fromJson(json_t *rootJ) override
    {
        std::string val;
        ModuleWidget::fromJson(rootJ);

        // legacy
        json_t *textJ = json_object_get(rootJ, "sequences");
        if (textJ) {
            peas->currentFormula = json_string_value(textJ);
            peas->manualSet = true;
        }
    }*/

    ComputerscareOhPeas *peas;
    PeasTF2 *textFieldTemp;
    SmallLetterDisplay *trimPlusMinus;
    PeasSmallDisplay *ndd;
    PeasSmallDisplay *transposeDisplay;
    void scaleItemAdd(ComputerscareOhPeas *peas, Menu *menu, std::string scale, std::string label);
    void appendContextMenu(Menu *menu) override;

};
struct SetScaleMenuItem : MenuItem
{
    ComputerscareOhPeas *peas;
    ComputerscareOhPeasWidget *peasWidget;
    std::string scale = "221222";
    SetScaleMenuItem(std::string scaleInput)
    {
        scale = scaleInput;
    }

    void onAction(const event::Action &e) override
    {
        peasWidget->textFieldTemp->text = scale;
        peas->setQuant();
    }
};
void ComputerscareOhPeasWidget::scaleItemAdd(ComputerscareOhPeas *peas, Menu *menu, std::string scale, std::string label)
{
    SetScaleMenuItem *menuItem = new SetScaleMenuItem(scale);
    menuItem->text = label;
    menuItem->peas = peas;
    menuItem->peasWidget = this;
    menu->addChild(menuItem);
}
void ComputerscareOhPeasWidget::appendContextMenu(Menu *menu)
{
    ComputerscareOhPeas *peas = dynamic_cast<ComputerscareOhPeas *>(this->module);

    MenuLabel *spacerLabel = new MenuLabel();
    menu->addChild(spacerLabel);


    MenuLabel *modeLabel = new MenuLabel();
    modeLabel->text = "Scale Presets";
    menu->addChild(modeLabel);

    scaleItemAdd(peas, menu, "221222", "Major");
    scaleItemAdd(peas, menu, "212212", "Natural Minor");
    scaleItemAdd(peas, menu, "2232", "Major Pentatonic");
    scaleItemAdd(peas, menu, "3223", "Minor Pentatonic");
    scaleItemAdd(peas, menu, "32113", "Blues");
    scaleItemAdd(peas, menu, "11111111111", "Chromatic");
    scaleItemAdd(peas, menu, "212213", "Harmonic Minor");
    scaleItemAdd(peas, menu, "22222", "Whole-Tone");
    scaleItemAdd(peas, menu, "2121212", "Whole-Half Diminished");

    scaleItemAdd(peas, menu, "43", "Major Triad");
    scaleItemAdd(peas, menu, "34", "Minor Triad");
    scaleItemAdd(peas, menu, "33", "Diminished Triad");
    scaleItemAdd(peas, menu, "434", "Major 7 Tetrachord");
    scaleItemAdd(peas, menu, "433", "Dominant 7 Tetrachord");
    scaleItemAdd(peas, menu, "343", "Minor 7 Tetrachord");
    scaleItemAdd(peas, menu, "334", "Minor 7 b5 Tetrachord");
}

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.


//Model *modelComputerscareDebug = createModel<ComputerscareDebug, ComputerscareDebugWidget>("computerscare-debug");

Model *modelComputerscareOhPeas = createModel<ComputerscareOhPeas, ComputerscareOhPeasWidget>("computerscare-ohpeas");
