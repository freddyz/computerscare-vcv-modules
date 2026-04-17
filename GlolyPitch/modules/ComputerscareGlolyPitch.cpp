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
#include "ColorTransformFBO.hpp"

// ─── Local button: corrected frame order (up=off=0, down=on=1) ───────────────

struct GlolyPitchOnOffButton : SvgSwitch {
  GlolyPitchOnOffButton() {
    addFrame(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-iso-button-up.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-iso-button-down.svg")));
    shadow->opacity = 0.f;
  }
};

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscareGlolyPitch : Module {
  enum ParamId {
    GLOBAL_TOGGLE,
    // Effects: toggle + knob (offset) + attenuverter triples
    SCALE_TOGGLE,    SCALE_KNOB,    SCALE_ATTEN,
    SCALE_X_TOGGLE,  SCALE_X_KNOB,  SCALE_X_ATTEN,
    SCALE_Y_TOGGLE,  SCALE_Y_KNOB,  SCALE_Y_ATTEN,
    ROT_TOGGLE,      ROT_KNOB,      ROT_ATTEN,
    KALEIDO_TOGGLE,  KALEIDO_KNOB,  KALEIDO_ATTEN,
    TRANS_X_TOGGLE,  TRANS_X_KNOB,  TRANS_X_ATTEN,
    TRANS_Y_TOGGLE,  TRANS_Y_KNOB,  TRANS_Y_ATTEN,
    // Color transforms
    HUE_TOGGLE,      HUE_KNOB,      HUE_ATTEN,
    INVERT_TOGGLE,   INVERT_KNOB,   INVERT_ATTEN,
    CURVES_TOGGLE,   CURVES_KNOB,   CURVES_ATTEN,
    NUM_PARAMS
  };

  enum InputId {
    // Gate inputs (override toggle when connected)
    SCALE_GATE_INPUT,
    SCALE_X_GATE_INPUT,
    SCALE_Y_GATE_INPUT,
    ROT_GATE_INPUT,
    KALEIDO_GATE_INPUT,
    TRANS_X_GATE_INPUT,
    TRANS_Y_GATE_INPUT,
    // CV inputs (modulate knob value via attenuverter)
    SCALE_CV_INPUT,
    SCALE_X_CV_INPUT,
    SCALE_Y_CV_INPUT,
    ROT_CV_INPUT,
    KALEIDO_CV_INPUT,
    TRANS_X_CV_INPUT,
    TRANS_Y_CV_INPUT,
    HUE_GATE_INPUT,
    INVERT_GATE_INPUT,
    CURVES_GATE_INPUT,
    HUE_CV_INPUT,
    INVERT_CV_INPUT,
    CURVES_CV_INPUT,
    // Global gate (overrides global toggle when connected)
    GLOBAL_GATE_INPUT,
    NUM_INPUTS
  };

  float width = 20 * RACK_GRID_WIDTH;
  bool loadedJSON = false;
  bool tileEmptySpace = false;

  // Effective row state — computed in process(), read in drawLayer()
  bool  globalEnabled = true;
  bool  rowEnabled[10] = {};
  float rowValue[10]   = {1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  ComputerscareGlolyPitch() {
    config(NUM_PARAMS, NUM_INPUTS, 0, 0);
    configSwitch(GLOBAL_TOGGLE,  0.f, 1.f, 1.f, "On/Off", {"Off", "On"});

    configSwitch(SCALE_TOGGLE,   0.f, 1.f, 1.f, "Scale",       {"Off", "On"});
    configParam (SCALE_KNOB,     0.1f, 4.f, 1.f, "Scale");
    configParam (SCALE_ATTEN,   -1.f,  1.f, 0.f, "Scale CV Atten");
    configSwitch(SCALE_X_TOGGLE, 0.f, 1.f, 1.f, "Scale X",     {"Off", "On"});
    configParam (SCALE_X_KNOB,   0.1f, 4.f, 1.f, "Scale X");
    configParam (SCALE_X_ATTEN, -1.f,  1.f, 0.f, "Scale X CV Atten");
    configSwitch(SCALE_Y_TOGGLE, 0.f, 1.f, 1.f, "Scale Y",     {"Off", "On"});
    configParam (SCALE_Y_KNOB,   0.1f, 4.f, 1.f, "Scale Y");
    configParam (SCALE_Y_ATTEN, -1.f,  1.f, 0.f, "Scale Y CV Atten");
    configSwitch(ROT_TOGGLE,     0.f, 1.f, 1.f, "Rotation",    {"Off", "On"});
    configParam (ROT_KNOB,     -180.f, 180.f, 0.f, "Rotation", "\u00b0");
    configParam (ROT_ATTEN,     -1.f,  1.f, 0.f, "Rotation CV Atten");
    configSwitch(KALEIDO_TOGGLE, 0.f, 1.f, 1.f, "Kaleidoscope", {"Off", "On"});
    configSwitch(KALEIDO_KNOB,   0.f, 12.f, 0.f, "Kaleido Mode",
      {"Off","1","2","3","4","5","6","7","8","9","10","11","12"});
    configParam (KALEIDO_ATTEN, -1.f,  1.f, 0.f, "Kaleido CV Atten");
    configSwitch(TRANS_X_TOGGLE, 0.f, 1.f, 1.f, "Translate X", {"Off", "On"});
    configParam (TRANS_X_KNOB, -1.f, 1.f, 0.f, "Translate X");
    configParam (TRANS_X_ATTEN, -1.f,  1.f, 0.f, "Translate X CV Atten");
    configSwitch(TRANS_Y_TOGGLE, 0.f, 1.f, 1.f, "Translate Y", {"Off", "On"});
    configParam (TRANS_Y_KNOB, -1.f, 1.f, 0.f, "Translate Y");
    configParam (TRANS_Y_ATTEN, -1.f,  1.f, 0.f, "Translate Y CV Atten");

    configInput(SCALE_GATE_INPUT,   "Scale Gate");
    configInput(SCALE_X_GATE_INPUT, "Scale X Gate");
    configInput(SCALE_Y_GATE_INPUT, "Scale Y Gate");
    configInput(ROT_GATE_INPUT,     "Rotation Gate");
    configInput(KALEIDO_GATE_INPUT, "Kaleido Gate");
    configInput(TRANS_X_GATE_INPUT, "Translate X Gate");
    configInput(TRANS_Y_GATE_INPUT, "Translate Y Gate");
    configInput(SCALE_CV_INPUT,     "Scale CV");
    configInput(SCALE_X_CV_INPUT,   "Scale X CV");
    configInput(SCALE_Y_CV_INPUT,   "Scale Y CV");
    configInput(ROT_CV_INPUT,       "Rotation CV");
    configInput(KALEIDO_CV_INPUT,   "Kaleido CV");
    configInput(TRANS_X_CV_INPUT,   "Translate X CV");
    configInput(TRANS_Y_CV_INPUT,   "Translate Y CV");
    configSwitch(HUE_TOGGLE,    0.f, 1.f, 1.f, "Hue",    {"Off", "On"});
    configParam (HUE_KNOB,    -180.f, 180.f, 0.f, "Hue", "\u00b0");
    configParam (HUE_ATTEN,    -1.f,  1.f, 0.f, "Hue CV Atten");
    configSwitch(INVERT_TOGGLE, 0.f, 1.f, 1.f, "Invert", {"Off", "On"});
    configParam (INVERT_KNOB,   0.f,  1.f, 0.f, "Invert");
    configParam (INVERT_ATTEN, -1.f,  1.f, 0.f, "Invert CV Atten");
    configSwitch(CURVES_TOGGLE, 0.f, 1.f, 1.f, "Curves", {"Off", "On"});
    configParam (CURVES_KNOB,  -1.f,  1.f, 0.f, "Curves");
    configParam (CURVES_ATTEN, -1.f,  1.f, 0.f, "Curves CV Atten");

    configInput(HUE_GATE_INPUT,    "Hue Gate");
    configInput(INVERT_GATE_INPUT,  "Invert Gate");
    configInput(CURVES_GATE_INPUT,  "Curves Gate");
    configInput(HUE_CV_INPUT,      "Hue CV");
    configInput(INVERT_CV_INPUT,    "Invert CV");
    configInput(CURVES_CV_INPUT,    "Curves CV");
    configInput(GLOBAL_GATE_INPUT,  "On/Off");
  }

  void onRandomize(const RandomizeEvent& e) override {
    // Randomize everything except the global on/off toggle
    for (int id = 0; id < NUM_PARAMS; id++) {
      if (id == GLOBAL_TOGGLE) continue;
      paramQuantities[id]->randomize();
    }
  }

  void process(const ProcessArgs& args) override {
    static const int toggleIds[10] = {
        SCALE_TOGGLE, SCALE_X_TOGGLE, SCALE_Y_TOGGLE,
        ROT_TOGGLE, KALEIDO_TOGGLE, TRANS_X_TOGGLE, TRANS_Y_TOGGLE,
        HUE_TOGGLE, INVERT_TOGGLE, CURVES_TOGGLE};
    static const int knobIds[10] = {
        SCALE_KNOB, SCALE_X_KNOB, SCALE_Y_KNOB,
        ROT_KNOB, KALEIDO_KNOB, TRANS_X_KNOB, TRANS_Y_KNOB,
        HUE_KNOB, INVERT_KNOB, CURVES_KNOB};
    static const int attenIds[10] = {
        SCALE_ATTEN, SCALE_X_ATTEN, SCALE_Y_ATTEN,
        ROT_ATTEN, KALEIDO_ATTEN, TRANS_X_ATTEN, TRANS_Y_ATTEN,
        HUE_ATTEN, INVERT_ATTEN, CURVES_ATTEN};
    static const int gateIds[10] = {
        SCALE_GATE_INPUT, SCALE_X_GATE_INPUT, SCALE_Y_GATE_INPUT,
        ROT_GATE_INPUT, KALEIDO_GATE_INPUT, TRANS_X_GATE_INPUT, TRANS_Y_GATE_INPUT,
        HUE_GATE_INPUT, INVERT_GATE_INPUT, CURVES_GATE_INPUT};
    static const int cvIds[10] = {
        SCALE_CV_INPUT, SCALE_X_CV_INPUT, SCALE_Y_CV_INPUT,
        ROT_CV_INPUT, KALEIDO_CV_INPUT, TRANS_X_CV_INPUT, TRANS_Y_CV_INPUT,
        HUE_CV_INPUT, INVERT_CV_INPUT, CURVES_CV_INPUT};
    // mins/maxs/cvScale indexed by row (0-9)
    static const float mins[10]    = {0.1f, 0.1f, 0.1f, -360.f,  0.f, -1.f, -1.f, -360.f, 0.f, -1.f};
    static const float maxs[10]    = {4.f,  4.f,  4.f,   360.f, 12.f,  1.f,  1.f,  360.f, 1.f,  1.f};
    // CV scale: +10V maps to full positive range. Hue: 360°. Invert/Curves: 1.0.
    static const float cvScale[10] = {0.3f, 0.3f, 0.3f, 36.f, 1.1f, 0.1f, 0.1f, 36.f, 0.1f, 0.1f};

    bool globalGate = inputs[GLOBAL_GATE_INPUT].isConnected();
    globalEnabled = globalGate
        ? (inputs[GLOBAL_GATE_INPUT].getVoltage() > 0.5f)
        : (params[GLOBAL_TOGGLE].getValue() > 0.5f);

    for (int i = 0; i < 10; i++) {
      bool gateConnected = inputs[gateIds[i]].isConnected();
      rowEnabled[i] = gateConnected
          ? (inputs[gateIds[i]].getVoltage() > 0.5f)
          : (params[toggleIds[i]].getValue() > 0.5f);

      float offset = params[knobIds[i]].getValue();
      float atten  = params[attenIds[i]].getValue();
      float cv     = inputs[cvIds[i]].isConnected() ? inputs[cvIds[i]].getVoltage() : 0.f;
      rowValue[i]  = clamp(offset + atten * cv * cvScale[i], mins[i], maxs[i]);
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "width", json_real(width));
    json_object_set_new(rootJ, "tileEmptySpace", json_boolean(tileEmptySpace));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) width = json_number_value(widthJ);
    json_t* tileJ = json_object_get(rootJ, "tileEmptySpace");
    if (tileJ) tileEmptySpace = json_boolean_value(tileJ);
    loadedJSON = false;
  }
};

// ─── Widget ──────────────────────────────────────────────────────────────────

static const float CONTROLS_WIDTH = 11 * RACK_GRID_WIDTH;  // 165 px / 11 HP

struct ComputerscareGlolyPitchWidget : ModuleWidget {
  BGPanel*                   bgPanel;
  ComputerscareResizeHandle* rightHandle;
  SvgWidget*                 bottomLogo;
  ScreenCapture              screenCap;
  ColorTransformFBO          colorFBO;

  ComputerscareGlolyPitchWidget(ComputerscareGlolyPitch* module) {
    setModule(module);
    float initialWidth = module ? module->width : 20 * RACK_GRID_WIDTH;
    box.size = Vec(initialWidth, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0x14, 0x14, 0x14));
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    // ── Resize handles — added early so they are lower z-order than params ────
    ComputerscareResizeHandle* leftHandle = new ComputerscareResizeHandle();
    leftHandle->minWidth = CONTROLS_WIDTH;
    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->minWidth = CONTROLS_WIDTH;
    rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    addChild(leftHandle);
    addChild(rightHandle);

    bottomLogo = new SvgWidget();
    bottomLogo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-light.svg")));
    bottomLogo->box.pos = Vec(0.f, RACK_GRID_HEIGHT - 30.f);
    addChild(bottomLogo);

    // ── Controls: 11HP left strip ─────────────────────────────────────────────
    // Column headers
    SmallLetterDisplay* hdr = new SmallLetterDisplay();
    hdr->box.pos  = Vec(3.f, 5.f);
    hdr->box.size = Vec(100.f, 10.f);
    hdr->fontSize = 8;
    hdr->value    = "SCRN";
    hdr->textColor  = nvgRGBf(0.7f, 0.9f, 0.85f);
    hdr->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
    hdr->breakRowWidth = 100.f;
    addChild(hdr);

    // Global on/off moved to header row (right side)
    addParam(createParam<GlolyPitchOnOffButton>(
        Vec(120.f, 3.f), module, ComputerscareGlolyPitch::GLOBAL_TOGGLE));
    addInput(createInput<OutPort>(
        Vec(140.f, 0.f), module, ComputerscareGlolyPitch::GLOBAL_GATE_INPUT));

    // ── 10 effect rows (7 geometry + 3 color) ────────────────────────────────
    // 35px row pitch fits all 10 rows within module height.
    // Layout per row (top-left positions, row center at y):
    //   x=2   Toggle   x=30 Gate jack   x=68 CV jack   x=100 Atten   x=122 Knob
    const int N = 10;
    float rowY[N] = {46.f, 81.f, 116.f, 151.f, 186.f, 221.f, 256.f, 291.f, 326.f, 361.f};
    const char* rowLabels[N] = {
        "SCALE", "SCL X", "SCL Y", "ROT", "KALI", "TRN X", "TRN Y",
        "HUE", "INVERT", "CURVES"
    };
    int toggleIds[N] = {
        ComputerscareGlolyPitch::SCALE_TOGGLE,
        ComputerscareGlolyPitch::SCALE_X_TOGGLE,
        ComputerscareGlolyPitch::SCALE_Y_TOGGLE,
        ComputerscareGlolyPitch::ROT_TOGGLE,
        ComputerscareGlolyPitch::KALEIDO_TOGGLE,
        ComputerscareGlolyPitch::TRANS_X_TOGGLE,
        ComputerscareGlolyPitch::TRANS_Y_TOGGLE,
        ComputerscareGlolyPitch::HUE_TOGGLE,
        ComputerscareGlolyPitch::INVERT_TOGGLE,
        ComputerscareGlolyPitch::CURVES_TOGGLE,
    };
    int knobIds[N] = {
        ComputerscareGlolyPitch::SCALE_KNOB,
        ComputerscareGlolyPitch::SCALE_X_KNOB,
        ComputerscareGlolyPitch::SCALE_Y_KNOB,
        ComputerscareGlolyPitch::ROT_KNOB,
        ComputerscareGlolyPitch::KALEIDO_KNOB,
        ComputerscareGlolyPitch::TRANS_X_KNOB,
        ComputerscareGlolyPitch::TRANS_Y_KNOB,
        ComputerscareGlolyPitch::HUE_KNOB,
        ComputerscareGlolyPitch::INVERT_KNOB,
        ComputerscareGlolyPitch::CURVES_KNOB,
    };
    int attenIds[N] = {
        ComputerscareGlolyPitch::SCALE_ATTEN,
        ComputerscareGlolyPitch::SCALE_X_ATTEN,
        ComputerscareGlolyPitch::SCALE_Y_ATTEN,
        ComputerscareGlolyPitch::ROT_ATTEN,
        ComputerscareGlolyPitch::KALEIDO_ATTEN,
        ComputerscareGlolyPitch::TRANS_X_ATTEN,
        ComputerscareGlolyPitch::TRANS_Y_ATTEN,
        ComputerscareGlolyPitch::HUE_ATTEN,
        ComputerscareGlolyPitch::INVERT_ATTEN,
        ComputerscareGlolyPitch::CURVES_ATTEN,
    };
    int gateInputIds[N] = {
        ComputerscareGlolyPitch::SCALE_GATE_INPUT,
        ComputerscareGlolyPitch::SCALE_X_GATE_INPUT,
        ComputerscareGlolyPitch::SCALE_Y_GATE_INPUT,
        ComputerscareGlolyPitch::ROT_GATE_INPUT,
        ComputerscareGlolyPitch::KALEIDO_GATE_INPUT,
        ComputerscareGlolyPitch::TRANS_X_GATE_INPUT,
        ComputerscareGlolyPitch::TRANS_Y_GATE_INPUT,
        ComputerscareGlolyPitch::HUE_GATE_INPUT,
        ComputerscareGlolyPitch::INVERT_GATE_INPUT,
        ComputerscareGlolyPitch::CURVES_GATE_INPUT,
    };
    int cvInputIds[N] = {
        ComputerscareGlolyPitch::SCALE_CV_INPUT,
        ComputerscareGlolyPitch::SCALE_X_CV_INPUT,
        ComputerscareGlolyPitch::SCALE_Y_CV_INPUT,
        ComputerscareGlolyPitch::ROT_CV_INPUT,
        ComputerscareGlolyPitch::KALEIDO_CV_INPUT,
        ComputerscareGlolyPitch::TRANS_X_CV_INPUT,
        ComputerscareGlolyPitch::TRANS_Y_CV_INPUT,
        ComputerscareGlolyPitch::HUE_CV_INPUT,
        ComputerscareGlolyPitch::INVERT_CV_INPUT,
        ComputerscareGlolyPitch::CURVES_CV_INPUT,
    };

    // Small "COLOR" divider label between geometry and color rows
    if (module) {
      SmallLetterDisplay* clrHdr = new SmallLetterDisplay();
      clrHdr->box.pos  = Vec(0.f, 272.f);
      clrHdr->box.size = Vec(165.f, 9.f);
      clrHdr->fontSize = 7;
      clrHdr->value    = "──── COLOR ────";
      clrHdr->textColor  = nvgRGBf(0.6f, 0.8f, 0.95f);
      clrHdr->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
      clrHdr->breakRowWidth = 165.f;
      addChild(clrHdr);
    }

    for (int i = 0; i < N; i++) {
      float y = rowY[i];

      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos  = Vec(0.f, y - 22.f);
      lbl->box.size = Vec(165.f, 14.f);
      lbl->fontSize = 12;
      lbl->value    = rowLabels[i];
      lbl->textColor  = nvgRGBf(0.9f, 1.0f, 0.95f);
      lbl->baseColor  = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 165.f;
      addChild(lbl);

      // Toggle (manual on/off; bypassed when gate jack is patched)
      addParam(createParam<SmallIsoButton>(Vec(2.f,   y - 14.f), module, toggleIds[i]));
      // Gate jack (flipped OutPort) — overrides toggle when connected (high = on)
      addInput(createInput<OutPort>(       Vec(30.f,  y - 17.f), module, gateInputIds[i]));
      // CV jack — modulates offset knob value via attenuverter
      addInput(createInput<InPort>(        Vec(68.f,  y - 14.f), module, cvInputIds[i]));
      // Attenuverter for CV input (-1 to +1)
      addParam(createParam<SmallKnob>(     Vec(100.f, y - 9.f),  module, attenIds[i]));
      // Offset knob (base value)
      addParam(createParam<SmoothKnob>(    Vec(122.f, y - 13.f), module, knobIds[i]));
    }

  }

  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && module && APP->scene && APP->scene->rack) {
      ComputerscareGlolyPitch* m = dynamic_cast<ComputerscareGlolyPitch*>(module);

      if (m && m->globalEnabled) {
        float alpha = 0.85f;

        bool  scaleOn  = m->rowEnabled[0];
        bool  scaleXOn = m->rowEnabled[1];
        bool  scaleYOn = m->rowEnabled[2];
        bool  rotOn    = m->rowEnabled[3];
        bool  kaliOn   = m->rowEnabled[4];
        bool  txOn     = m->rowEnabled[5];
        bool  tyOn     = m->rowEnabled[6];
        bool  hueOn    = m->rowEnabled[7];
        bool  invertOn = m->rowEnabled[8];
        bool  curvesOn = m->rowEnabled[9];

        float scaleV   = m->rowValue[0];
        float scaleXV  = m->rowValue[1];
        float scaleYV  = m->rowValue[2];
        float rotV     = m->rowValue[3];
        float kaliV    = m->rowValue[4];
        float txV      = m->rowValue[5];
        float tyV      = m->rowValue[6];
        float hueV     = hueOn    ? m->rowValue[7] : 0.f;
        float invertV  = invertOn ? m->rowValue[8] : 0.f;
        float curvesV  = curvesOn ? m->rowValue[9] : 0.f;

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

        // Apply color transforms via GPU shader (skipped when all neutral)
        int fbW, fbH;
        glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

        float hw = mirrorW * 0.5f;
        float hh = mirrorH * 0.5f;

        // Scale values (compose uniform + per-axis)
        float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
        float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
        // Translation in image/local space: one full 0-10V sweep = one tile width,
        // applied AFTER rotation so scrolling is seamless at any scale or rotation angle.
        float txLocal = txOn ? txV * mirrorW : 0.f;
        float tyLocal = tyOn ? tyV * mirrorH : 0.f;
        float txAbs   = fabsf(txLocal);
        float tyAbs   = fabsf(tyLocal);

        nvgSave(args.vg);

        nvgScissor(args.vg, CONTROLS_WIDTH, 0.f, mirrorW, box.size.y);
        // Translate to panel center only — tx/ty applied later in image space
        nvgTranslate(args.vg, CONTROLS_WIDTH + hw, hh);

        if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

        bool tileOn = m->tileEmptySpace;
        int img = colorFBO.apply(args.vg, screenCap.texId, screenCap.nvgImg,
                                  fbW, fbH, hueV, invertV, curvesV);

        int kaliMode = kaliOn ? (int)kaliV : 0;

        if (kaliMode > 0) {
          // Apply local translation before kali's internal rotation (handles scale independence)
          if (txOn || tyOn) nvgTranslate(args.vg, txLocal, tyLocal);
          float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = ((mirrorW + 2.f * txAbs) * cosA + (box.size.y + 2.f * tyAbs) * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = ((mirrorW + 2.f * txAbs) * sinA + (box.size.y + 2.f * tyAbs) * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;
          drawKaleidoscope(args.vg, img, hw, hh, mirrorW, mirrorH, rHW, rHH, kaliMode, alpha,
                           rotOn, rotV, false, 0.f);
        } else {
          if (rotOn) applyRotation(args.vg, rotV);
          // Apply translation after rotation so scroll direction is in image space —
          // this ensures one full CV sweep = one tile at any rotation angle.
          if (txOn || tyOn) nvgTranslate(args.vg, txLocal, tyLocal);
          float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = ((mirrorW + 2.f * txAbs) * cosA + (box.size.y + 2.f * tyAbs) * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = ((mirrorW + 2.f * txAbs) * sinA + (box.size.y + 2.f * tyAbs) * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;

          if (tileOn) {
            float pcx = -(txOn ? txLocal : 0.f);
            float pcy = -(tyOn ? tyLocal : 0.f);
            int iMin = (int)ceilf((pcx - rHW - hw) / mirrorW);
            int iMax = (int)floorf((pcx + rHW + hw) / mirrorW);
            int jMin = (int)ceilf((pcy - rHH - hh) / mirrorH);
            int jMax = (int)floorf((pcy + rHH + hh) / mirrorH);
            iMin = std::max(iMin, -20); iMax = std::min(iMax, 20);
            jMin = std::max(jMin, -20); jMax = std::min(jMax, 20);
            for (int j = jMin; j <= jMax; j++) {
              for (int i = iMin; i <= iMax; i++) {
                float ox = -hw + i * mirrorW;
                float oy = -hh + j * mirrorH;
                NVGpaint p = nvgImagePattern(args.vg, ox, oy, mirrorW, mirrorH,
                                             0.f, img, alpha);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, ox, oy, mirrorW, mirrorH);
                nvgFillPaint(args.vg, p);
                nvgFill(args.vg);
              }
            }
          } else {
            NVGpaint p = nvgImagePattern(args.vg, -hw, -hh, mirrorW, mirrorH,
                                         0.f, img, alpha);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, -rHW, -rHH, 2.f * rHW, 2.f * rHH);
            nvgFillPaint(args.vg, p);
            nvgFill(args.vg);
          }
        }

        nvgRestore(args.vg);
      }
    }
    ModuleWidget::drawLayer(args, layer);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareGlolyPitch* m = dynamic_cast<ComputerscareGlolyPitch*>(module);
    if (!m) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createSubmenuItem("Visual", "", [=](Menu* menu) {
      menu->addChild(createBoolPtrMenuItem("Tile empty space", "", &m->tileEmptySpace));
    }));
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
