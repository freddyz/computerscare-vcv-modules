#pragma once
#include "Computerscare.hpp"

#include "glitch/GlitchNanoVG.cpp"
#include "glitch/GlitchOpenGL.cpp"


using namespace rack;

struct ComputerscareGlolyPitch : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
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
        //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareGlolyPitchPanel.svg")));

        // Module widget setup here
    }
    void draw(const DrawArgs &ctx) override
    {
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
    }
};
Model *modelComputerscareGlolyPitch = createModel<ComputerscareGlolyPitch, ComputerscareGlolyPitchWidget>("computerscare-glolypitch");