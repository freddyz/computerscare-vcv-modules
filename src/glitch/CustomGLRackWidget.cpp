#include "rack.hpp"

using namespace rack;

struct CustomGLRackWidget : RackWidget {
    void draw(const DrawArgs& args) override {
        // Apply OpenGL transformations
        glPushMatrix();

        // Example: Apply a global scale and rotation
        float scaleFactor = 1.2f; // Scale by 20%
        float rotationAngle = 15.0f; // Rotate by 15 degrees
        glScalef(scaleFactor, scaleFactor, 1.0f);
        glRotatef(rotationAngle, 0.0f, 0.0f, 1.0f);

        // Draw the rest of the rack (calling the base class draw method)
        RackWidget::draw(args);

        // Reset transformations
        glPopMatrix();
    }
};


