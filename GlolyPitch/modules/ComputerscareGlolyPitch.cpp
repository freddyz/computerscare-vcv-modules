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
    // Global gate (overrides global toggle when connected)
    GLOBAL_GATE_INPUT,
    NUM_INPUTS
  };

  float width = 20 * RACK_GRID_WIDTH;
  bool loadedJSON = false;
  bool tileEmptySpace = false;

  // Effective row state — computed in process(), read in drawLayer()
  bool  globalEnabled = true;
  bool  rowEnabled[7] = {};
  float rowValue[7]   = {1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f};

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
    configSwitch(KALEIDO_KNOB,   0.f, 11.f, 0.f, "Kaleido Mode",
      {"1","2","3","4","5","6","7","8","9","10","11","12"});
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
    configInput(GLOBAL_GATE_INPUT,  "On/Off");
  }

  void process(const ProcessArgs& args) override {
    static const int toggleIds[7] = {
        SCALE_TOGGLE, SCALE_X_TOGGLE, SCALE_Y_TOGGLE,
        ROT_TOGGLE, KALEIDO_TOGGLE, TRANS_X_TOGGLE, TRANS_Y_TOGGLE};
    static const int knobIds[7] = {
        SCALE_KNOB, SCALE_X_KNOB, SCALE_Y_KNOB,
        ROT_KNOB, KALEIDO_KNOB, TRANS_X_KNOB, TRANS_Y_KNOB};
    static const int attenIds[7] = {
        SCALE_ATTEN, SCALE_X_ATTEN, SCALE_Y_ATTEN,
        ROT_ATTEN, KALEIDO_ATTEN, TRANS_X_ATTEN, TRANS_Y_ATTEN};
    static const int gateIds[7] = {
        SCALE_GATE_INPUT, SCALE_X_GATE_INPUT, SCALE_Y_GATE_INPUT,
        ROT_GATE_INPUT, KALEIDO_GATE_INPUT, TRANS_X_GATE_INPUT, TRANS_Y_GATE_INPUT};
    static const int cvIds[7] = {
        SCALE_CV_INPUT, SCALE_X_CV_INPUT, SCALE_Y_CV_INPUT,
        ROT_CV_INPUT, KALEIDO_CV_INPUT, TRANS_X_CV_INPUT, TRANS_Y_CV_INPUT};
    static const float mins[7]    = {0.1f, 0.1f, 0.1f, -180.f,  0.f, -1.f, -1.f};
    static const float maxs[7]    = {4.f,  4.f,  4.f,   180.f, 11.f,  1.f,  1.f};
    // CV scale: attenuverter=1, +10V maps to the full positive range of each param.
    // Rotation: +10V = +180° (full rotation).  Trans: +10V = +1.0 (full screen unit).
    static const float cvScale[7] = {0.3f, 0.3f, 0.3f, 18.f, 1.1f, 0.1f, 0.1f};

    bool globalGate = inputs[GLOBAL_GATE_INPUT].isConnected();
    globalEnabled = globalGate
        ? (inputs[GLOBAL_GATE_INPUT].getVoltage() > 0.5f)
        : (params[GLOBAL_TOGGLE].getValue() > 0.5f);

    for (int i = 0; i < 7; i++) {
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

    // ── Controls: 7HP left strip ──────────────────────────────────────────────
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

    // ── 7 effect rows ─────────────────────────────────────────────────────────
    // Layout per row (top-left positions, row center at y):
    //   x=2   Toggle (SmallIsoButton 10x10)
    //   x=30  Gate jack (OutPort 32x34, flipped)  — overrides toggle when patched
    //   x=68  CV jack (InPort 27x29)              — modulates offset knob via attenuverter
    //   x=100 Attenuverter (SmallKnob 18x18)
    //   x=122 Offset knob (SmoothKnob 25x25)
    const int N = 7;
    float rowY[N] = {46.f, 84.f, 122.f, 160.f, 198.f, 236.f, 274.f};
    const char* rowLabels[N] = {"SCALE", "SCL X", "SCL Y",
                                 "ROT", "KALI", "TRN X", "TRN Y"};
    int toggleIds[N] = {
        ComputerscareGlolyPitch::SCALE_TOGGLE,
        ComputerscareGlolyPitch::SCALE_X_TOGGLE,
        ComputerscareGlolyPitch::SCALE_Y_TOGGLE,
        ComputerscareGlolyPitch::ROT_TOGGLE,
        ComputerscareGlolyPitch::KALEIDO_TOGGLE,
        ComputerscareGlolyPitch::TRANS_X_TOGGLE,
        ComputerscareGlolyPitch::TRANS_Y_TOGGLE,
    };
    int knobIds[N] = {
        ComputerscareGlolyPitch::SCALE_KNOB,
        ComputerscareGlolyPitch::SCALE_X_KNOB,
        ComputerscareGlolyPitch::SCALE_Y_KNOB,
        ComputerscareGlolyPitch::ROT_KNOB,
        ComputerscareGlolyPitch::KALEIDO_KNOB,
        ComputerscareGlolyPitch::TRANS_X_KNOB,
        ComputerscareGlolyPitch::TRANS_Y_KNOB,
    };
    int attenIds[N] = {
        ComputerscareGlolyPitch::SCALE_ATTEN,
        ComputerscareGlolyPitch::SCALE_X_ATTEN,
        ComputerscareGlolyPitch::SCALE_Y_ATTEN,
        ComputerscareGlolyPitch::ROT_ATTEN,
        ComputerscareGlolyPitch::KALEIDO_ATTEN,
        ComputerscareGlolyPitch::TRANS_X_ATTEN,
        ComputerscareGlolyPitch::TRANS_Y_ATTEN,
    };
    int gateInputIds[N] = {
        ComputerscareGlolyPitch::SCALE_GATE_INPUT,
        ComputerscareGlolyPitch::SCALE_X_GATE_INPUT,
        ComputerscareGlolyPitch::SCALE_Y_GATE_INPUT,
        ComputerscareGlolyPitch::ROT_GATE_INPUT,
        ComputerscareGlolyPitch::KALEIDO_GATE_INPUT,
        ComputerscareGlolyPitch::TRANS_X_GATE_INPUT,
        ComputerscareGlolyPitch::TRANS_Y_GATE_INPUT,
    };
    int cvInputIds[N] = {
        ComputerscareGlolyPitch::SCALE_CV_INPUT,
        ComputerscareGlolyPitch::SCALE_X_CV_INPUT,
        ComputerscareGlolyPitch::SCALE_Y_CV_INPUT,
        ComputerscareGlolyPitch::ROT_CV_INPUT,
        ComputerscareGlolyPitch::KALEIDO_CV_INPUT,
        ComputerscareGlolyPitch::TRANS_X_CV_INPUT,
        ComputerscareGlolyPitch::TRANS_Y_CV_INPUT,
    };

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

    // Global on/off — to the right of the logo
    addParam(createParam<GlolyPitchOnOffButton>(
        Vec(34.f, RACK_GRID_HEIGHT - 30.f), module,
        ComputerscareGlolyPitch::GLOBAL_TOGGLE));
    // Global gate — overrides global toggle when patched
    addInput(createInput<OutPort>(
        Vec(62.f, RACK_GRID_HEIGHT - 30.f), module,
        ComputerscareGlolyPitch::GLOBAL_GATE_INPUT));
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

        float scaleV   = m->rowValue[0];
        float scaleXV  = m->rowValue[1];
        float scaleYV  = m->rowValue[2];
        float rotV     = m->rowValue[3];
        float kaliV    = m->rowValue[4];
        float txV      = m->rowValue[5];
        float tyV      = m->rowValue[6];

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
        // Translation: scaled by sx/sy so one full CV sweep = one image tile width,
        // keeping the scroll seamless regardless of zoom level.
        float tx = txOn ? txV * mirrorW * sx : 0.f;
        float ty = tyOn ? tyV * mirrorH * sy : 0.f;

        nvgSave(args.vg);

        nvgScissor(args.vg, CONTROLS_WIDTH, 0.f, mirrorW, box.size.y);
        nvgTranslate(args.vg, CONTROLS_WIDTH + hw + tx, hh + ty);

        if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

        bool tileOn = m->tileEmptySpace;
        int img = screenCap.nvgImg;

        // Extra extent needed so the fill rect covers the full panel when translated
        float txAbs = txOn ? fabsf(tx) : 0.f;
        float tyAbs = tyOn ? fabsf(ty) : 0.f;

        if (kaliOn) {
          int mode = (int)kaliV + 1;
          float cosA = rotOn ? cosf(fabsf(rotV) * (float)M_PI / 180.f) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = ((mirrorW + 2.f * txAbs) * cosA + (box.size.y + 2.f * tyAbs) * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = ((mirrorW + 2.f * txAbs) * sinA + (box.size.y + 2.f * tyAbs) * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;
          drawKaleidoscope(args.vg, img, hw, hh, mirrorW, mirrorH, rHW, rHH, mode, alpha,
                           rotOn, rotV, false, 0.f);
        } else {
          if (rotOn) applyRotation(args.vg, rotV);
          float cosA = rotOn ? cosf(fabsf(rotV) * (float)M_PI / 180.f) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = ((mirrorW + 2.f * txAbs) * cosA + (box.size.y + 2.f * tyAbs) * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = ((mirrorW + 2.f * txAbs) * sinA + (box.size.y + 2.f * tyAbs) * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;

          if (tileOn) {
            float pcx = -(txOn ? tx / std::max(sx, 0.01f) : 0.f);
            float pcy = -(tyOn ? ty / std::max(sy, 0.01f) : 0.f);
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
