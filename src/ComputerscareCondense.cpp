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
Vec* widgetPosition = nullptr; // Pointer to the widget's position
Vec* widgetSize = nullptr; // Pointer to the widget's position

    std::unordered_map<int, bool> visibilityMap; // Tracks visibility state by module ID
    std::unordered_map<int, Vec> originalPositions;
    std::unordered_map<int, float> originalPositionsX;
    std::unordered_map<int, float> originalPositionsY;

    CondenseModule() {

        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    }

   
    void toggleCollapse() {
            bool allAligned = true;
            float leftmostX = 0.f;
            float upmostY = 0.f;
             // Check if the trigger button is pressed
        if (params[TOGGLE_HIDE].getValue() > 0.f) { 

              if (!widgetPosition || !widgetSize) {
                DEBUG("Widget position not set");
                return;
            } 

            float condenseX = widgetPosition->x + widgetSize->x; // Right edge of the CondenseModule
            float condenseY = widgetPosition->y; // Y position of the CondenseModule

                    //gather info
            for (auto widget : APP->scene->rack->getSelected()) {
                if (widget) {
                    int moduleId = widget->module->id;
                             // Toggle visibility state
                    bool currentState = visibilityMap[moduleId];


                    if (originalPositions.find(moduleId) == originalPositions.end()) {
                        originalPositions[moduleId] = widget->box.pos;
                    }
                     allAligned = allAligned && (leftmostX == widget->box.pos.x) && (upmostY == widget->box.pos.y);

                    leftmostX = std::max(leftmostX, widget->box.pos.x);
                    upmostY =  std::max(upmostY, widget->box.pos.y);


                   // widget->setVisible(!currentState);
                    visibilityMap[moduleId] = !currentState;


                    

                }
            }

                // Toggle positions
            DEBUG("allAligned: %i",allAligned);
            for (auto widget : APP->scene->rack->getSelected()) {
                if (widget && widget->module) {
                    int moduleId = widget->module->id;
                    widget->box.pos.x = condenseX;
                    widget->box.pos.y = condenseY;
                    widget->box.size.x = 2.f;
                }
            }

            // Reset trigger button state
    params[1].setValue(0.f);

}
}


void process(const ProcessArgs& args) override {
        // Check if the "Condense" mode is active


    if(counter%maxCounter==0) {

        logSelected();
        toggleCollapse();

    }
    counter++;
}   

 void logSelected() {
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
        }
};

// Custom widget to add buttons
struct CondenseModuleWidget : ModuleWidget {
    CondenseModuleWidget(CondenseModule* module) {
        setModule(module);
          box.size = Vec(3 * 15, 380);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/ComputerscareCondensePanel.svg")));

        // Add "Condense" toggle button
        addParam(createParam<CKSS>(mm2px(Vec(5, 20)), module, CondenseModule::ENABLED));


        addParam(createParam<CKSS>(mm2px(Vec(10, 20)), module, 1));
        addParam(createParam<CKSS>(mm2px(Vec(15, 20)), module, 2));

          // Add trigger button
        //addParam(createParam<LEDButton>(mm2px(Vec(5, 40)), module, 0));
        // Optional: Add another toggle for future use if needed
    }
    void step() override {
        ModuleWidget::step(); // Call parent step to ensure proper functionality

        // Ensure that the module has access to the widget's position
        if (module) {
            // Update the module's widget position
            CondenseModule* condenseModule = dynamic_cast<CondenseModule*>(module);
            if (condenseModule) {
                condenseModule->widgetPosition = &box.pos; // Update the widget position
                 condenseModule->widgetSize = &box.size; // Update the widget position
                
            }
        }
    }
};

// Register the module
Model* modelComputerscareCondense = createModel<CondenseModule, CondenseModuleWidget>("computerscare-condense");