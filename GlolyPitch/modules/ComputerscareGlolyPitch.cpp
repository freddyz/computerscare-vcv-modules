#include "../../src/Computerscare.hpp"
#include "../../src/ComputerscareResizableHandle.hpp"
#include "ScreenCaptureEffect.hpp"

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscareGlolyPitch : Module {
  enum ParamId {
    OPACITY_KNOB,   // mirror overlay alpha
    GLOBAL_TOGGLE,  // on/off
    NUM_PARAMS
  };

  // Default 12 HP — 6 HP for controls + 6 HP for mirror
  float width = 12 * RACK_GRID_WIDTH;
  bool loadedJSON = false;

  ComputerscareGlolyPitch() {
    config(NUM_PARAMS, 0, 0, 0);
    configParam(OPACITY_KNOB, 0.f, 1.f, 0.85f, "Mirror Opacity");
    configSwitch(GLOBAL_TOGGLE, 0.f, 1.f, 0.f, "Mirror Enable", {"Off", "On"});
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

// Width of the left control strip (in px). Mirror fills everything to the right.
static const float CONTROLS_WIDTH = 4 * RACK_GRID_WIDTH;  // 60 px / 4 HP

struct ComputerscareGlolyPitchWidget : ModuleWidget {
  BGPanel*                  bgPanel;
  ComputerscareResizeHandle* rightHandle;
  SvgWidget*                topLogo;
  SvgWidget*                bottomLogo;

  ScreenCapture screenCap;

  ComputerscareGlolyPitchWidget(ComputerscareGlolyPitch* module) {
    setModule(module);
    float initialWidth = module ? module->width : 12 * RACK_GRID_WIDTH;
    box.size = Vec(initialWidth, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0x14, 0x14, 0x14));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    // Bottom-left logo (light version for dark bg) — no top logo
    topLogo = nullptr;
    bottomLogo = new SvgWidget();
    bottomLogo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-light.svg")));
    bottomLogo->box.pos = Vec(0.f, RACK_GRID_HEIGHT - 30.f);
    addChild(bottomLogo);

    // ── Controls (left strip, 4 HP) ───────────────────────────────────────────
    // "SCRN" label at top
    SmallLetterDisplay* lbl = new SmallLetterDisplay();
    lbl->box.pos  = Vec(4.f, 8.f);
    lbl->box.size = Vec(52.f, 11.f);
    lbl->fontSize = 8;
    lbl->value    = "SCRN";
    lbl->textColor  = nvgRGBf(0.7f, 0.9f, 0.85f);
    lbl->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
    lbl->breakRowWidth = 52.f;
    addChild(lbl);

    // Opacity knob (centered in 4HP strip)
    addParam(createParam<SmallKnob>(Vec(19.f, 22.f), module,
                                    ComputerscareGlolyPitch::OPACITY_KNOB));

    // Global on/off — left-aligned, just above the logo
    addParam(createParam<IsoButton>(Vec(2.f, RACK_GRID_HEIGHT - 68.f), module,
                                    ComputerscareGlolyPitch::GLOBAL_TOGGLE));

    // ── Resize handles ────────────────────────────────────────────────────────
    ComputerscareResizeHandle* leftHandle = new ComputerscareResizeHandle();
    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    addChild(leftHandle);
    addChild(rightHandle);
  }

  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && module && APP->scene && APP->scene->rack) {
      ComputerscareGlolyPitch* m =
          dynamic_cast<ComputerscareGlolyPitch*>(module);

      if (m && m->params[ComputerscareGlolyPitch::GLOBAL_TOGGLE].getValue() > 0.5f) {
        float alpha = m->params[ComputerscareGlolyPitch::OPACITY_KNOB].getValue();

        // Grab framebuffer every frame regardless of size (keeps texture warm)
        screenCap.capture(args.vg);

        // Mirror area: right side of the panel, after the control strip.
        float mirrorX = box.pos.x + CONTROLS_WIDTH;
        float mirrorW = box.size.x - CONTROLS_WIDTH;

        if (mirrorW > 2.f) {
          nvgSave(args.vg);
          // Local widget space → moduleContainer space
          nvgTranslate(args.vg, -box.pos.x, -box.pos.y);

          // Clip to the mirror area so the image doesn't bleed left over controls
          nvgScissor(args.vg, mirrorX, box.pos.y, mirrorW, box.size.y);
          screenCap.drawInPanel(args.vg,
                                Vec(mirrorX, box.pos.y),
                                Vec(mirrorW, box.size.y),
                                alpha);
          nvgResetScissor(args.vg);

          nvgRestore(args.vg);
        }
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
        rightHandle->box.pos.x = m->width - rightHandle->box.size.x;
        m->loadedJSON = true;
      } else if (box.size.x != m->width) {
        m->width = box.size.x;
        bgPanel->box.size.x = box.size.x;
        rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
      }
    }
    ModuleWidget::step();
  }
};

Model* modelComputerscareGlolyPitch =
    createModel<ComputerscareGlolyPitch, ComputerscareGlolyPitchWidget>(
        "computerscare-gloly-pitch");
