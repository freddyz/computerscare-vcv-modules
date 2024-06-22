#include "rack.hpp"

using namespace rack;

struct GlitchModuleWidget : ModuleWidget {
    GlitchModuleWidget() {
        // Constructor code here
    }

    void draw(const DrawArgs &args) override {
        // Call the parent class's draw method
        ModuleWidget::draw(args);

        // Start NanoVG drawing context
        nvgBeginPath(args.vg);

        // Set random seed for consistent randomness
        srand((unsigned)time(0));
        
        // Draw multiple random rectangles with varying positions and colors
        for (int i = 0; i < 10; i++) {
            float x = (rand() % 128);
            float y = (rand() % 380);
            float w = (rand() % 20) + 10;
            float h = (rand() % 20) + 10;

            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, y, w, h);

            // Set random color
            nvgFillColor(args.vg, nvgRGBA(rand() % 256, rand() % 256, rand() % 256, 255));
            nvgFill(args.vg);
        }
    }
};

//Model *modelGlitchModule = createModel<Module, GlitchModuleWidget>("GlitchModule");