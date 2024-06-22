#include "rack.hpp"
//#include <GL/gl.h>

using namespace rack;

struct GlitchOpenGLModuleWidget : ModuleWidget {
    GlitchOpenGLModuleWidget() {
        // Constructor code here
    }

    void draw(const DrawArgs &args) override {
        // Call the parent class's draw method
        ModuleWidget::draw(args);

        // Get the framebuffer
        GLuint fbo;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&fbo);

        // Read the current framebuffer
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int width = viewport[2];
        int height = viewport[3];

        std::vector<GLubyte> pixels(width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // Apply a simple glitch effect by altering pixel data
        for (int i = 0; i < pixels.size(); i += 4) {
            // Introduce some random noise
            pixels[i] = (pixels[i] + rand() % 50) % 256;      // Red
            pixels[i + 1] = (pixels[i + 1] + rand() % 50) % 256;  // Green
            pixels[i + 2] = (pixels[i + 2] + rand() % 50) % 256;  // Blue
        }

        // Write the altered pixels back to the framebuffer
        glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // Reset the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }
};

//Model *modelGlitchModule = createModel<Module, GlitchModuleWidget>("GlitchModule");
