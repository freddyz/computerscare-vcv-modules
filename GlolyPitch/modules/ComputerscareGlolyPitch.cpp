#include "../../src/Computerscare.hpp"
#include "../../src/ComputerscareResizableHandle.hpp"
#include "ScreenCaptureEffect.hpp"
#include "MirrorScale.hpp"
#include "MirrorScaleX.hpp"
#include "MirrorScaleY.hpp"
#include "MirrorRotation.hpp"
#include "MirrorFlip.hpp"
#include "MirrorTranslation.hpp"
#include "MirrorKaleidoscope.hpp"

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscareGlolyPitch : Module {
  enum ParamId {
    OPACITY_KNOB,
    GLOBAL_TOGGLE,
    // Effects (toggle + knob pairs)
    SCALE_TOGGLE,    SCALE_KNOB,     // uniform scale
    SCALE_X_TOGGLE,  SCALE_X_KNOB,   // horizontal stretch
    SCALE_Y_TOGGLE,  SCALE_Y_KNOB,   // vertical stretch
    ROT_TOGGLE,      ROT_KNOB,       // rotation degrees
    MIRROR_TOGGLE,   MIRROR_KNOB,    // reflection axis angle (0=H, 90=V, etc.)
    KALEIDO_TOGGLE,  KALEIDO_KNOB,   // mode 1-4
    TRANS_X_TOGGLE,  TRANS_X_KNOB,   // horizontal pan
    TRANS_Y_TOGGLE,  TRANS_Y_KNOB,   // vertical pan
    NUM_PARAMS
  };

  float width = 12 * RACK_GRID_WIDTH;
  bool loadedJSON = false;

  ComputerscareGlolyPitch() {
    config(NUM_PARAMS, 0, 0, 0);
    configParam(OPACITY_KNOB,   0.f,   1.f,   0.85f, "Mirror Opacity");
    configSwitch(GLOBAL_TOGGLE, 0.f,   1.f,   0.f,   "Mirror Enable",  {"Off", "On"});

    configSwitch(SCALE_TOGGLE,   0.f, 1.f, 0.f, "Scale",      {"Off", "On"});
    configParam (SCALE_KNOB,     0.1f, 4.f, 1.f, "Scale");
    configSwitch(SCALE_X_TOGGLE, 0.f, 1.f, 0.f, "Scale X",    {"Off", "On"});
    configParam (SCALE_X_KNOB,   0.1f, 4.f, 1.f, "Scale X");
    configSwitch(SCALE_Y_TOGGLE, 0.f, 1.f, 0.f, "Scale Y",    {"Off", "On"});
    configParam (SCALE_Y_KNOB,   0.1f, 4.f, 1.f, "Scale Y");
    configSwitch(ROT_TOGGLE,     0.f, 1.f, 0.f, "Rotation",   {"Off", "On"});
    configParam (ROT_KNOB,     -180.f, 180.f, 0.f, "Rotation", "\u00b0");
    configSwitch(MIRROR_TOGGLE,  0.f, 1.f, 0.f, "Mirror",     {"Off", "On"});
    configParam (MIRROR_KNOB,    0.f, 180.f, 0.f, "Mirror Axis", "\u00b0");
    configSwitch(KALEIDO_TOGGLE, 0.f, 1.f, 0.f, "Kaleidoscope", {"Off", "On"});
    configSwitch(KALEIDO_KNOB,   0.f, 11.f, 0.f, "Kaleido Mode",
      {"1","2","3","4","5","6","7","8","9","10","11","12"});
    configSwitch(TRANS_X_TOGGLE, 0.f, 1.f, 0.f, "Translate X", {"Off", "On"});
    configParam (TRANS_X_KNOB, -300.f, 300.f, 0.f, "Translate X", " px");
    configSwitch(TRANS_Y_TOGGLE, 0.f, 1.f, 0.f, "Translate Y", {"Off", "On"});
    configParam (TRANS_Y_KNOB, -300.f, 300.f, 0.f, "Translate Y", " px");
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

static const float CONTROLS_WIDTH = 4 * RACK_GRID_WIDTH;  // 60 px / 4 HP

struct ComputerscareGlolyPitchWidget : ModuleWidget {
  BGPanel*                   bgPanel;
  ComputerscareResizeHandle* rightHandle;
  SvgWidget*                 bottomLogo;
  ScreenCapture              screenCap;

  ComputerscareGlolyPitchWidget(ComputerscareGlolyPitch* module) {
    setModule(module);
    float initialWidth = module ? module->width : 12 * RACK_GRID_WIDTH;
    box.size = Vec(initialWidth, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0x14, 0x14, 0x14));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    bottomLogo = new SvgWidget();
    bottomLogo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-light.svg")));
    bottomLogo->box.pos = Vec(0.f, RACK_GRID_HEIGHT - 30.f);
    addChild(bottomLogo);

    // ── Controls: 4HP left strip ──────────────────────────────────────────────
    // "SCRN" header
    SmallLetterDisplay* hdr = new SmallLetterDisplay();
    hdr->box.pos  = Vec(3.f, 5.f);
    hdr->box.size = Vec(54.f, 10.f);
    hdr->fontSize = 8;
    hdr->value    = "SCRN";
    hdr->textColor  = nvgRGBf(0.7f, 0.9f, 0.85f);
    hdr->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
    hdr->breakRowWidth = 54.f;
    addChild(hdr);

    // Opacity knob (no toggle — always active)
    SmallLetterDisplay* opLbl = new SmallLetterDisplay();
    opLbl->box.pos  = Vec(3.f, 16.f);
    opLbl->box.size = Vec(54.f, 9.f);
    opLbl->fontSize = 7;
    opLbl->value    = "OPAC";
    opLbl->textColor  = nvgRGBf(0.55f, 0.75f, 0.7f);
    opLbl->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
    opLbl->breakRowWidth = 54.f;
    addChild(opLbl);
    addParam(createParam<SmallKnob>(Vec(19.f, 26.f), module,
                                    ComputerscareGlolyPitch::OPACITY_KNOB));

    // ── 8 effect rows ─────────────────────────────────────────────────────────
    const int N = 8;
    // Row center Y values — start at 62, 30px spacing
    float rowY[N] = {62.f, 92.f, 122.f, 152.f, 182.f, 212.f, 242.f, 272.f};
    const char* rowLabels[N] = {"SCALE", "SCL X", "SCL Y",
                                 "ROT", "MIRR", "KALI", "TRN X", "TRN Y"};
    int toggleIds[N] = {
        ComputerscareGlolyPitch::SCALE_TOGGLE,
        ComputerscareGlolyPitch::SCALE_X_TOGGLE,
        ComputerscareGlolyPitch::SCALE_Y_TOGGLE,
        ComputerscareGlolyPitch::ROT_TOGGLE,
        ComputerscareGlolyPitch::MIRROR_TOGGLE,
        ComputerscareGlolyPitch::KALEIDO_TOGGLE,
        ComputerscareGlolyPitch::TRANS_X_TOGGLE,
        ComputerscareGlolyPitch::TRANS_Y_TOGGLE,
    };
    int knobIds[N] = {
        ComputerscareGlolyPitch::SCALE_KNOB,
        ComputerscareGlolyPitch::SCALE_X_KNOB,
        ComputerscareGlolyPitch::SCALE_Y_KNOB,
        ComputerscareGlolyPitch::ROT_KNOB,
        ComputerscareGlolyPitch::MIRROR_KNOB,
        ComputerscareGlolyPitch::KALEIDO_KNOB,
        ComputerscareGlolyPitch::TRANS_X_KNOB,
        ComputerscareGlolyPitch::TRANS_Y_KNOB,
    };

    for (int i = 0; i < N; i++) {
      float y = rowY[i];

      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos  = Vec(2.f, y - 11.f);
      lbl->box.size = Vec(56.f, 9.f);
      lbl->fontSize = 7;
      lbl->value    = rowLabels[i];
      lbl->textColor  = nvgRGBf(0.6f, 0.82f, 0.78f);
      lbl->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 56.f;
      addChild(lbl);

      addParam(createParam<SmallIsoButton>(Vec(2.f, y - 6.f), module, toggleIds[i]));
      addParam(createParam<SmallKnob>(Vec(32.f, y - 8.f), module, knobIds[i]));
    }

    // Global on/off — left-aligned, above logo
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
      ComputerscareGlolyPitch* m = dynamic_cast<ComputerscareGlolyPitch*>(module);

      if (m && m->params[ComputerscareGlolyPitch::GLOBAL_TOGGLE].getValue() > 0.5f) {
        float alpha   = m->params[ComputerscareGlolyPitch::OPACITY_KNOB].getValue();

        bool  scaleOn  = m->params[ComputerscareGlolyPitch::SCALE_TOGGLE].getValue()   > 0.5f;
        bool  scaleXOn = m->params[ComputerscareGlolyPitch::SCALE_X_TOGGLE].getValue() > 0.5f;
        bool  scaleYOn = m->params[ComputerscareGlolyPitch::SCALE_Y_TOGGLE].getValue() > 0.5f;
        bool  rotOn    = m->params[ComputerscareGlolyPitch::ROT_TOGGLE].getValue()      > 0.5f;
        bool  mirrOn   = m->params[ComputerscareGlolyPitch::MIRROR_TOGGLE].getValue()  > 0.5f;
        bool  kaliOn   = m->params[ComputerscareGlolyPitch::KALEIDO_TOGGLE].getValue() > 0.5f;
        bool  txOn     = m->params[ComputerscareGlolyPitch::TRANS_X_TOGGLE].getValue() > 0.5f;
        bool  tyOn     = m->params[ComputerscareGlolyPitch::TRANS_Y_TOGGLE].getValue() > 0.5f;

        float scaleV   = m->params[ComputerscareGlolyPitch::SCALE_KNOB].getValue();
        float scaleXV  = m->params[ComputerscareGlolyPitch::SCALE_X_KNOB].getValue();
        float scaleYV  = m->params[ComputerscareGlolyPitch::SCALE_Y_KNOB].getValue();
        float rotV     = m->params[ComputerscareGlolyPitch::ROT_KNOB].getValue();
        float mirrV    = m->params[ComputerscareGlolyPitch::MIRROR_KNOB].getValue();
        float kaliV    = m->params[ComputerscareGlolyPitch::KALEIDO_KNOB].getValue();
        float txV      = m->params[ComputerscareGlolyPitch::TRANS_X_KNOB].getValue();
        float tyV      = m->params[ComputerscareGlolyPitch::TRANS_Y_KNOB].getValue();

        float mirrorW = box.size.x - CONTROLS_WIDTH;
        float mirrorH = box.size.y;

        if (mirrorW <= 2.f) {
          ModuleWidget::drawLayer(args, layer);
          return;
        }

        screenCap.capture(args.vg);
        if (screenCap.nvgImg < 0) {
          ModuleWidget::drawLayer(args, layer);
          return;
        }

        float hw = mirrorW * 0.5f;
        float hh = mirrorH * 0.5f;

        // Scale values (compose uniform + per-axis)
        float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
        float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
        float tx = txOn ? txV : 0.f;
        float ty = tyOn ? tyV : 0.f;

        nvgSave(args.vg);

        // ── Scissor in MODULE-LOCAL space (origin = top-left of our panel) ──
        // drawLayer is called with NVG already translated to our box.pos,
        // so (CONTROLS_WIDTH, 0) is exactly the mirror panel top-left in local coords.
        nvgScissor(args.vg, CONTROLS_WIDTH, 0.f, mirrorW, box.size.y);

        // Translate to mirror panel center + pan offset (still in local space)
        nvgTranslate(args.vg, CONTROLS_WIDTH + hw + tx, hh + ty);

        // Scale only here — rotation/flip are applied differently for kaleido vs. plain.
        if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

        int img = screenCap.nvgImg;

        if (kaliOn) {
          int mode = (int)kaliV + 1;
          // Rotation/flip are NOT applied to the outer NVG transform here.
          // They are applied inside each kSector() call, AFTER nvgIntersectScissor.
          // This avoids NanoVG's broken scissor-intersection behaviour under rotation,
          // which was causing the rendered image to bleed outside the module frame.
          float cosA = rotOn ? cosf(fabsf(rotV) * (float)M_PI / 180.f) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = (mirrorW * cosA + box.size.y * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = (mirrorW * sinA + box.size.y * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;
          drawKaleidoscope(args.vg, img, hw, hh, mirrorW, mirrorH, rHW, rHH, mode, alpha,
                           rotOn, rotV, mirrOn, mirrV);
        } else {
          // Non-kaleido: apply rotation/flip to the outer transform as before
          if (rotOn)  applyRotation(args.vg, rotV);
          if (mirrOn) applyFlip(args.vg, mirrV);
          // Compute the half-extents of the rect that, in the ROTATED+SCALED
          // coordinate space, exactly covers the screen-aligned panel rectangle.
          // The scissor clips everything back to the panel bounds.
          float cosA = rotOn ? cosf(fabsf(rotV) * (float)M_PI / 180.f) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = (mirrorW * cosA + box.size.y * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = (mirrorW * sinA + box.size.y * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;

          NVGpaint p = nvgImagePattern(args.vg, -hw, -hh, mirrorW, mirrorH,
                                       0.f, img, alpha);
          nvgBeginPath(args.vg);
          nvgRect(args.vg, -rHW, -rHH, 2.f * rHW, 2.f * rHH);
          nvgFillPaint(args.vg, p);
          nvgFill(args.vg);
        }

        nvgRestore(args.vg);
      }
    }
    ModuleWidget::drawLayer(args, layer);
  }

  void step() override {
    if (module) {
      ComputerscareGlolyPitch* m = dynamic_cast<ComputerscareGlolyPitch*>(module);
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
