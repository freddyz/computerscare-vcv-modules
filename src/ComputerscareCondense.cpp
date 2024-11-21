#include "Computerscare.hpp"


struct CondenseModule : ComputerscareMenuParamModule {
   enum ParamIds {
    ENABLED,
    TOGGLE_HIDE,

    NUM_PARAMS = TOGGLE_HIDE+20
};
enum InputIds {
    VAL_INPUT,
    TRG_INPUT,
    CLR_INPUT,
    NUM_INPUTS
};
enum OutputIds {
    POLY_OUTPUT,
    NUM_OUTPUTS
};
enum LightIds {
    BLINK_LIGHT,
    NUM_LIGHTS
};

int counter = 0;
int maxCounter = 2000;

    std::unordered_map<int, bool> visibilityMap; // Tracks visibility state by module ID

CondenseModule() {

    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
}


void process(const ProcessArgs& args) override {
    // Check if the "Condense" mode is active

    if(counter%maxCounter==0) {
       bool condenseEnabled =params[ENABLED].getValue();
       if (condenseEnabled) {
                condenseEnabled = false; // Prevent constant printing
                // Access selected modules
                for (auto widget : APP->scene->rack->getSelected()) {
                    if (widget) {
                                                    // Log module IDs
                        DEBUG("Selected Module ID: %d", widget->module->id);
                    }
                }
                counter = 0;
            }

            // Check if the trigger button is pressed
        if (params[TOGGLE_HIDE].getValue() > 0.f) { // Button is pressed
            for (auto widget : APP->scene->rack->getSelected()) {
                if (widget) {
                    int moduleId = widget->module->id;
                     // Toggle visibility state
                    bool currentState = visibilityMap[moduleId];
                    widget->setVisible(!currentState);
                    visibilityMap[moduleId] = !currentState;

                    DEBUG("Toggled visibility for Module ID: %d, New State: %s", 
                          moduleId, !currentState ? "Hidden" : "Visible");
                }
            }
            // Reset trigger button state
            params[1].setValue(0.f);
        }
    }
    counter++;
}   
};

// Custom widget to add buttons
struct CondenseModuleWidget : ModuleWidget {
    CondenseModuleWidget(CondenseModule* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/ComputerscareCondensePanel.svg")));

        // Add "Condense" toggle button
        addParam(createParam<CKSS>(mm2px(Vec(5, 20)), module, CondenseModule::ENABLED));


        addParam(createParam<CKSS>(mm2px(Vec(25, 20)), module, 1));
        addParam(createParam<CKSS>(mm2px(Vec(45, 20)), module, 2));

          // Add trigger button
        //addParam(createParam<LEDButton>(mm2px(Vec(5, 40)), module, 0));
        // Optional: Add another toggle for future use if needed
    }
};

// Register the module
Model* modelComputerscareCondense = createModel<CondenseModule, CondenseModuleWidget>("computerscare-condense");