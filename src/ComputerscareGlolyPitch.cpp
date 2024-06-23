/*
    GlolyPitch

*/

#include "Computerscare.hpp"

#include "glitch/GlitchNanoVG.cpp"
#include "glitch/GlitchOpenGL.cpp"
//#include "glitch/CustomNanoVGRackWidget.cpp"
//#include "glitch/CustomGLRackWidget.cpp"


using namespace rack;

struct ComputerscareGlolyPitch : Module {
    enum ParamIds {
        MUTE_ENABLED,
        INPUT_MUTE_ENABLED,
        AMOUNT_KNOB,
        AMOUNT_CV_TRIM_KNOB,
        LOCATION_KNOB_A,
        LOCATION_KNOB_B,
        LOCATION_CV_A_TRIM,
        LOCATION_CV_B_TRIM,
        NUM_PARAMS
    };
    enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        RHYTHM_INPUT,
        CV_INPUT_A,
        CV_INPUT_B,
        NUM_INPUTS
    };
    enum OutputIds {
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    ComputerscareGlolyPitch() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    }

    void process(const ProcessArgs& args) override {
        // Module processing code here
    }
};

struct ComputerscareGlolyPitchWidget : ModuleWidget {
    ComputerscareGlolyPitchWidget(ComputerscareGlolyPitch* module) {
        setModule(module);
        box.size = Vec(4 * 15, 380);


       /* glitchNanoVG = new GlitchNanoVGWidget();
       // glitchNanoVG->module = module;
        glitchNanoVG->box.pos = Vec(0, 0);
        glitchNanoVG->box.size = Vec(box.size.x, box.size.y);
         addChild(glitchNanoVG);

         glitchOpenGL = new GlitchOpenGLWidget();
       // glitchNanoVG->module = module;
        glitchOpenGL->box.pos = Vec(0, 0);
        glitchOpenGL->box.size = Vec(box.size.x, box.size.y);
         addChild(glitchOpenGL);*/

         if (module) {
            glitchNanoVG = new GlitchNanoVGWidget();
            APP->scene->addChild(glitchNanoVG);

            glitchOpenGL= new GlitchOpenGLWidget();
            glitchOpenGL->box.size = APP->window->getSize();
            APP->scene->addChild(glitchOpenGL);

            APP->window->screenshot(system::join("/Users/adammalone/Desktop", "screenshot.png"));

        }
       /* addChild(display);

        glitchNanoVG = new GlitchNanoVGWidget();
        glitchNanoVG.box.size(4*15,380);
        addChild(glitchNanoVG);*/

        // Module widget setup here

        //APP->scene->rack = new CustomGLRackWidget();
    }
    ~ComputerscareGlolyPitchWidget() {
        if (module) {
            APP->scene->removeChild(glitchNanoVG);
            APP->scene->removeChild(glitchOpenGL);
            delete glitchNanoVG;
            delete glitchOpenGL;
        }
    }
    void draw(const DrawArgs &ctx) override    {
        float brd=12.f;

        // Background
        NVGcolor backgroundColor = nvgRGB(0x10, 0x00, 0x10);
        NVGcolor StrokeColor = nvgRGB(0xC0, 0xC7, 0xDE);
        
        nvgBeginPath(ctx.vg);
        nvgRoundedRect(ctx.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
        nvgFillColor(ctx.vg, backgroundColor);
        nvgFill(ctx.vg);

        nvgBeginPath(ctx.vg);
        nvgRoundedRect(ctx.vg, +brd, +brd, box.size.x - 2*brd, box.size.y -2*brd, 4.0);
        nvgFillColor(ctx.vg, StrokeColor);
        nvgFill(ctx.vg);

        //glitchNanoVG->draw(ctx);
      //  glitchOpenGL->draw(ctx);

    }
    // To use this CustomGLRackWidget instead of the default RackWidget
   /* void init() {
        // Assuming APP->scene is a Scene object that holds the RackWidget
        g = new CustomGLRackWidget();
    }*/

    GlitchNanoVGWidget* glitchNanoVG;
    GlitchOpenGLWidget* glitchOpenGL;
};
Model *modelComputerscareGlolyPitch = createModel<ComputerscareGlolyPitch, ComputerscareGlolyPitchWidget>("computerscare-glolypitch");