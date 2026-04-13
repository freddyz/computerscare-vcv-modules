#include "../../src/Computerscare.hpp"
#include "../../src/ComputerscareResizableHandle.hpp"
#include "TiltEffect.hpp"
#include "ScaleEffect.hpp"
#include "ScanlinesEffect.hpp"
#include "ColorFloodEffect.hpp"
#include "EchoEffect.hpp"
#include "DenseEffect.hpp"
#include "OpacityEffect.hpp"
#include "StackEffect.hpp"
#include "WirePortEffect.hpp"

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscareGlolyPitch : Module {
  enum ParamId {
    // Toggles (order matches effect row order)
    TILT_TOGGLE,
    SCALE_TOGGLE,
    SCANLINES_TOGGLE,
    COLFLOOD_TOGGLE,
    ECHO_TOGGLE,
    DENSE_TOGGLE,
    OPACITY_TOGGLE,
    STACK_TOGGLE,
    // Knobs
    TILT_KNOB,
    SCALE_KNOB,
    SCANLINES_KNOB,
    COLFLOOD_KNOB,
    ECHO_KNOB,
    DENSE_KNOB,
    OPACITY_KNOB,
    STACK_KNOB,
    // Global
    GLOBAL_TOGGLE,
    NUM_PARAMS
  };

  float width = 6 * RACK_GRID_WIDTH;
  bool loadedJSON = false;

  ComputerscareGlolyPitch() {
    config(NUM_PARAMS, 0, 0, 0);
    configSwitch(TILT_TOGGLE,     0.f, 1.f, 0.f, "Tilt",        {"Off", "On"});
    configSwitch(SCALE_TOGGLE,    0.f, 1.f, 0.f, "Scale",       {"Off", "On"});
    configSwitch(SCANLINES_TOGGLE,0.f, 1.f, 0.f, "Scanlines",   {"Off", "On"});
    configSwitch(COLFLOOD_TOGGLE, 0.f, 1.f, 0.f, "Color Flood", {"Off", "On"});
    configSwitch(ECHO_TOGGLE,     0.f, 1.f, 0.f, "Echo",        {"Off", "On"});
    configSwitch(DENSE_TOGGLE,    0.f, 1.f, 0.f, "Dense",       {"Off", "On"});
    configSwitch(OPACITY_TOGGLE,  0.f, 1.f, 0.f, "Opacity",     {"Off", "On"});
    configSwitch(STACK_TOGGLE,    0.f, 1.f, 0.f, "Stack",       {"Off", "On"});

    configParam(TILT_KNOB,      -45.f,  45.f,  8.f,   "Tilt Angle",       "\u00b0");
    configParam(SCALE_KNOB,       0.5f,  2.f,  1.25f, "Scale Factor");
    configParam(SCANLINES_KNOB,   2.f,  20.f,  5.f,   "Scanline Spacing", " px");
    configParam(COLFLOOD_KNOB,    0.f,   1.f,  0.35f, "Flood Intensity");
    configParam(ECHO_KNOB,        2.f,  12.f,  5.f,   "Echo Iterations");
    configParam(DENSE_KNOB,       3.f,  20.f,  8.f,   "Dense Copies");
    configParam(OPACITY_KNOB,     0.f,   1.f,  0.5f,  "Bloom Intensity");
    configParam(STACK_KNOB,       1.f,  15.f,  5.f,   "Stack Offset",     " px");

    configSwitch(GLOBAL_TOGGLE,   0.f,   1.f,  0.f,   "Global Enable",    {"Off", "On"});
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "width", json_real(width));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) width = json_number_value(widthJ);
    loadedJSON = false;
  }
};

// ─── Widget ──────────────────────────────────────────────────────────────────

struct ComputerscareGlolyPitchWidget : ModuleWidget {
  BGPanel* bgPanel;
  ComputerscareResizeHandle* rightHandle;
  SvgWidget* topLogo;
  SvgWidget* bottomLogo;

  ComputerscareGlolyPitchWidget(ComputerscareGlolyPitch* module) {
    setModule(module);
    float initialWidth = module ? module->width : 6 * RACK_GRID_WIDTH;
    box.size = Vec(initialWidth, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0x14, 0x14, 0x14));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    // Top-right logo
    topLogo = new SvgWidget();
    topLogo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-normal.svg")));
    topLogo->box.pos = Vec(box.size.x - 30, 0);
    addChild(topLogo);

    // Bottom-left logo
    bottomLogo = new SvgWidget();
    bottomLogo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-normal.svg")));
    bottomLogo->box.pos = Vec(0, RACK_GRID_HEIGHT - 30);
    addChild(bottomLogo);

    // ── Effect rows (8 total, densely packed) ─────────────────────────────────
    const int NUM_EFFECTS = 8;
    // 36px row spacing, first row center at y=48
    float rowCenterY[NUM_EFFECTS] = {48.f, 84.f, 120.f, 156.f,
                                     192.f, 228.f, 264.f, 300.f};
    const char* labels[NUM_EFFECTS] = {
        "TILT", "SCALE", "LINES", "FLOOD",
        "ECHO", "DENSE", "OPAC",  "STACK"};

    int toggleParams[NUM_EFFECTS] = {
        ComputerscareGlolyPitch::TILT_TOGGLE,
        ComputerscareGlolyPitch::SCALE_TOGGLE,
        ComputerscareGlolyPitch::SCANLINES_TOGGLE,
        ComputerscareGlolyPitch::COLFLOOD_TOGGLE,
        ComputerscareGlolyPitch::ECHO_TOGGLE,
        ComputerscareGlolyPitch::DENSE_TOGGLE,
        ComputerscareGlolyPitch::OPACITY_TOGGLE,
        ComputerscareGlolyPitch::STACK_TOGGLE,
    };
    int knobParams[NUM_EFFECTS] = {
        ComputerscareGlolyPitch::TILT_KNOB,
        ComputerscareGlolyPitch::SCALE_KNOB,
        ComputerscareGlolyPitch::SCANLINES_KNOB,
        ComputerscareGlolyPitch::COLFLOOD_KNOB,
        ComputerscareGlolyPitch::ECHO_KNOB,
        ComputerscareGlolyPitch::DENSE_KNOB,
        ComputerscareGlolyPitch::OPACITY_KNOB,
        ComputerscareGlolyPitch::STACK_KNOB,
    };

    for (int i = 0; i < NUM_EFFECTS; i++) {
      float y = rowCenterY[i];

      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos = Vec(8.f, y - 14.f);
      lbl->box.size = Vec(74.f, 12.f);
      lbl->fontSize = 8;
      lbl->value = labels[i];
      lbl->textColor = nvgRGBf(0.7f, 0.9f, 0.85f);
      lbl->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 74.f;
      addChild(lbl);

      addParam(createParam<SmallIsoButton>(Vec(8.f, y - 6.f), module,
                                          toggleParams[i]));
      addParam(createParam<SmallKnob>(Vec(52.f, y - 11.f), module,
                                     knobParams[i]));
    }

    // ── Global on/off ─────────────────────────────────────────────────────────
    addParam(createParam<IsoButton>(Vec(28.f, 342.f), module,
                                   ComputerscareGlolyPitch::GLOBAL_TOGGLE));

    // ── Resize handles ────────────────────────────────────────────────────────
    ComputerscareResizeHandle* leftHandle = new ComputerscareResizeHandle();
    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    addChild(leftHandle);
    addChild(rightHandle);
  }

  // Effects draw in layer 1 — after all module panels, before cables/plugs.
  // No nvgScissor active here, so we can paint anywhere in the rack.
  // Effects are applied sequentially (each draws on top of the previous),
  // creating a visual chain.
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && module && APP->scene && APP->scene->rack) {
      ComputerscareGlolyPitch* m =
          dynamic_cast<ComputerscareGlolyPitch*>(module);

      if (m && m->params[ComputerscareGlolyPitch::GLOBAL_TOGGLE].getValue() >
                   0.5f) {
        bool tiltOn =
            m->params[ComputerscareGlolyPitch::TILT_TOGGLE].getValue() > 0.5f;
        bool scaleOn =
            m->params[ComputerscareGlolyPitch::SCALE_TOGGLE].getValue() > 0.5f;
        bool scanOn =
            m->params[ComputerscareGlolyPitch::SCANLINES_TOGGLE].getValue() >
            0.5f;
        bool floodOn =
            m->params[ComputerscareGlolyPitch::COLFLOOD_TOGGLE].getValue() >
            0.5f;
        bool echoOn =
            m->params[ComputerscareGlolyPitch::ECHO_TOGGLE].getValue() > 0.5f;
        bool denseOn =
            m->params[ComputerscareGlolyPitch::DENSE_TOGGLE].getValue() > 0.5f;
        bool opacOn =
            m->params[ComputerscareGlolyPitch::OPACITY_TOGGLE].getValue() >
            0.5f;
        bool stackOn =
            m->params[ComputerscareGlolyPitch::STACK_TOGGLE].getValue() > 0.5f;

        float tiltAngle =
            m->params[ComputerscareGlolyPitch::TILT_KNOB].getValue();
        float scale =
            m->params[ComputerscareGlolyPitch::SCALE_KNOB].getValue();
        float scanSpacing =
            m->params[ComputerscareGlolyPitch::SCANLINES_KNOB].getValue();
        float floodIntensity =
            m->params[ComputerscareGlolyPitch::COLFLOOD_KNOB].getValue();
        float echoIter =
            m->params[ComputerscareGlolyPitch::ECHO_KNOB].getValue();
        float denseCopies =
            m->params[ComputerscareGlolyPitch::DENSE_KNOB].getValue();
        float opacIntensity =
            m->params[ComputerscareGlolyPitch::OPACITY_KNOB].getValue();
        float stackOffset =
            m->params[ComputerscareGlolyPitch::STACK_KNOB].getValue();

        nvgSave(args.vg);
        // Translate from local widget space → moduleContainer coordinate space
        nvgTranslate(args.vg, -box.pos.x, -box.pos.y);

        auto mods = APP->scene->rack->getModules();

        // ── Per-module effects, chained in order ──────────────────────────
        for (app::ModuleWidget* mw : mods) {
          if (mw == this) continue;

          // Each effect draws on top of the previous, creating a visual chain.
          if (tiltOn)  drawTiltEffect(args.vg, mw, tiltAngle);
          if (scaleOn) drawScaleEffect(args.vg, mw, scale);
          if (scanOn)  drawScanlinesEffect(args.vg, mw, scanSpacing);
          if (floodOn) drawColorFloodEffect(args.vg, mw, floodIntensity);
          if (echoOn)  drawEchoEffect(args.vg, mw, echoIter);
          if (denseOn) drawDenseEffect(args.vg, mw, denseCopies);
          if (opacOn)  drawOpacityEffect(args.vg, mw, opacIntensity);
          if (stackOn) drawStackEffect(args.vg, mw, stackOffset);
        }

        // ── Wire glow + port halos (global when active) ───────────────────
        drawWireGlowEffect(args.vg, 0.6f);
        drawPortHaloEffect(args.vg, mods, 0.5f);

        nvgRestore(args.vg);
      }
    }
    ModuleWidget::drawLayer(args, layer);
  }

  void step() override {
    if (module) {
      ComputerscareGlolyPitch* m =
          dynamic_cast<ComputerscareGlolyPitch*>(module);
      if (!m->loadedJSON) {
        box.size.x = m->width;
        bgPanel->box.size.x = m->width;
        topLogo->box.pos.x = m->width - 30.f;
        rightHandle->box.pos.x = m->width - rightHandle->box.size.x;
        m->loadedJSON = true;
      } else if (box.size.x != m->width) {
        m->width = box.size.x;
        bgPanel->box.size.x = box.size.x;
        topLogo->box.pos.x = box.size.x - 30.f;
        rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
      }
    }
    ModuleWidget::step();
  }
};

Model* modelComputerscareGlolyPitch =
    createModel<ComputerscareGlolyPitch, ComputerscareGlolyPitchWidget>(
        "computerscare-gloly-pitch");
