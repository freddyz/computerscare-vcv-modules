#include "rack.hpp"
#include <GL/glew.h> 
#include <vector>
#include <ctime>
#include <cstdlib>

using namespace rack;
struct GlitchOpenGLWidget : OpenGlWidget {
    bool shouldClear = false;

    void step() {
    // Render every frame
        if(random::uniform() < 0.02) {
            shouldClear=true;
        }
        if(random::uniform() < 0.1) {
            dirty = true;
            FramebufferWidget::step(); 
        }
    }

    void clearRandomRect() {
        math::Vec fbSize = getFramebufferSize();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, fbSize.x, 0.0, fbSize.y, -1.0, 1.0);

        float x0 = random::uniform()*fbSize.x;
        float y0 = random::uniform()*fbSize.y;
        float x1 = random::uniform()*fbSize.x;
        float y1 = random::uniform()*fbSize.y;

        glClearColor(0.0, 0.0, 0.0, 0.0);
        glScissor(x0,y0,x1,y1);
        glEnable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
    }



    void drawPixels(int width,int height) {
        math::Vec fbSize = getFramebufferSize();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, fbSize.x, 0.0, fbSize.y, -1.0, 1.0);

       /* void Window::screenshot(const std::string& screenshotPath) {
            // Get window framebuffer size
            int width, height;
            glfwGetFramebufferSize(APP->window->win, &width, &height);

            // Allocate pixel color buffer
            uint8_t* pixels = new uint8_t[height * width * 4];

            // glReadPixels defaults to GL_BACK, but the back-buffer is unstable, so use the front buffer (what the user sees)
            glReadBuffer(GL_FRONT);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

            // Write pixels to PNG
            flipBitmap(pixels, width, height, 4);
            stbi_write_png(screenshotPath.c_str(), width, height, 4, pixels, width * 4);

            delete[] pixels;
        }*/

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(APP->window->win, &fbWidth, &fbHeight);

        uint8_t* fbPixels = new uint8_t[width * height * 4];


        // glReadPixels defaults to GL_BACK, but the back-buffer is unstable, so use the front buffer (what the user sees)
        

        glReadBuffer(GL_FRONT);
        glReadPixels(300, 200, width, height, GL_RGBA, GL_UNSIGNED_BYTE, fbPixels);

        //unsigned char pixels[width * height * 3];
        // memset(pixels, 0, sizeof(pixels)); // Clear to black
        uint8_t* pixels = new uint8_t[height * width * 4];
        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {
                pixels[(i * height + j) * 4 + 0] = 255*random::uniform(); // Red component
                pixels[(i * height + j) * 4 + 1] = 127*random::uniform();   // Green component
                pixels[(i * height + j) * 4 + 2] = 0;   // Blue component
                pixels[(i * height + j) * 4 + 3] = 255*random::uniform();   // Alpha component
            }
        }
        glRasterPos2i(fbWidth*random::uniform(), fbWidth*random::uniform()); // Start drawing at position (0, 0)
        if(random::uniform() < 0.5) {
            glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        } else {
            glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, fbPixels);
        }

        delete[] pixels;
        delete [] fbPixels;
    }

    void drawFramebuffer() {
        math::Vec fbSize = getFramebufferSize();
        glViewport(0.0, 0.0, fbSize.x, fbSize.y);
        
        if(shouldClear) {
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            shouldClear=false;
        }
       

       glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, fbSize.x, 0.0, fbSize.y, -1.0, 1.0);

        glBegin(GL_TRIANGLES);
        glColor4f(random::uniform(), random::uniform(), random::uniform(),0.1);
        glVertex3f(fbSize.x*random::uniform(), random::uniform()*fbSize.y, 0);
        glColor3f(random::uniform(), random::uniform(), random::uniform());
        glVertex3f(fbSize.x*random::uniform(), 0, 0);
        glColor3f(0, 0, random::uniform());
        glVertex3f(0, fbSize.y*random::uniform(), 0);
        glEnd();
         if(random::uniform() < 0.8) {
            clearRandomRect();
            }
            if(random::uniform() < 0.9) {
                drawPixels((int) 1000*random::uniform(), (int) 1000*random::uniform());
            }

    }

};