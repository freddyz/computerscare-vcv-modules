#include "rack.hpp"

using namespace rack;


struct RandomSquaresWidget : TransparentWidget {
    RandomSquaresWidget() {
        // Constructor code here
    }

    void draw(const DrawArgs &args) override {
        // Call the parent class's draw method
        TransparentWidget::draw(args);

        // Start NanoVG drawing context
        nvgBeginPath(args.vg);

        // Set random seed for consistent randomness
        srand((unsigned)time(0));
        
        // Draw multiple random rectangles with varying positions and colors
        for (int i = 0; i < 4; i++) {
            float x = (rand() % 1238);
            float y = (rand() % 1380);
            float w = (rand() % 200) + 10;
            float h = (rand() % 200) + 10;

            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, y, w, h);

            // Set random color
            nvgFillColor(args.vg, nvgRGBA(rand() % 256, rand() % 256, rand() % 256, 255));
            nvgFill(args.vg);
        }
    }
};

struct GlitchNanoVGWidget : TransparentWidget {
  
    GlitchNanoVGWidget() {
        // Constructor code here
        RandomSquaresWidget* rsq = new RandomSquaresWidget();
        addChild(rsq);
    }

  /*  void draw(const DrawArgs &args) override {
        DEBUG("wid %i",box.size.x);
        // Call the parent class's draw method
        TransparentWidget::draw(args);

        // Start NanoVG drawing context
        nvgBeginPath(args.vg);

        // Set random seed for consistent randomness
        srand((unsigned)time(0));
        
        // Draw multiple random rectangles with varying positions and colors
        for (int i = 0; i < 10; i++) {
            float x = (rand() % 1238);
            float y = (rand() % 1380);
            float w = (rand() % 200) + 10;
            float h = (rand() % 200) + 10;

            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, y, w, h);

            // Set random color
            nvgFillColor(args.vg, nvgRGBA(rand() % 256, rand() % 256, rand() % 256, 255));
            nvgFill(args.vg);
        }
    }*/
};
