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

    }

};