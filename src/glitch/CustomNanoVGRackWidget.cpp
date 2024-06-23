#include "rack.hpp"

using namespace rack;

struct CustomNanoVGRackWidget : RackWidget {
    void draw(const DrawArgs& args) override {
        // Begin NanoVG drawing context
        nvgBeginPath(args.vg);

        // Example: Apply a global scale and rotation
        float scaleFactor = 1.2f; // Scale by 20%
        float rotationAngle = NVG_PI / 12; // Rotate by 15 degrees (in radians)
        nvgScale(args.vg, scaleFactor, scaleFactor);
        nvgRotate(args.vg, rotationAngle);

        // Draw the rest of the rack (calling the base class draw method)
        RackWidget::draw(args);

        // Reset transformations
        nvgResetTransform(args.vg);
    }
};

