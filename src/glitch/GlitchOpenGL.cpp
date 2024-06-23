#include "rack.hpp"
#include <GL/glew.h> 
#include <vector>
#include <ctime>
#include <cstdlib>

using namespace rack;
struct GlitchOpenGLWidget : OpenGlWidget {

    void step() {
    // Render every frame
        if(random::uniform() < 0.1) {
             dirty = true;
            FramebufferWidget::step(); 
        }
  
}


void drawFramebuffer() {
    math::Vec fbSize = getFramebufferSize();
    glViewport(0.0, 0.0, fbSize.x, fbSize.y);
    glClearColor(0.0, 0.0, 0.0, 1.0);
   // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, fbSize.x, 0.0, fbSize.y, -1.0, 1.0);

    glBegin(GL_TRIANGLES);
    glColor4f(random::uniform(), 0, 0,0.1);
    glVertex3f(0, 0, 0);
    glColor3f(0, random::uniform(), 0);
    glVertex3f(fbSize.x, 0, 0);
    glColor3f(0, 0, random::uniform());
    glVertex3f(0, fbSize.y, 0);
    glEnd();
}

};