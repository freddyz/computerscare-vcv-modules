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
#include <atomic>

// Momentary version of SmallIsoButton for the trigger
struct GlolyPitchTrigButton : SmallIsoButton {
  GlolyPitchTrigButton() { momentary = true; }
};

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscareGlolyPitch : Module {
  enum ParamId {
    CONTINUOUS_TOGGLE,  // on=continuous (every frame), off=triggered
    TRIGGER_BUTTON,     // momentary: fires one draw frame
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
    // Global mode inputs
    CONTINUOUS_GATE_INPUT,  // gate: high=continuous, low=triggered
    TRIGGER_INPUT,          // trigger: fires one draw in triggered mode
    NUM_INPUTS
  };

  float width = 20 * RACK_GRID_WIDTH;
  bool loadedJSON = false;
  bool tileEmptySpace = false;
  bool maintainAspect = false;

  // Effective row state — computed in process(), read in drawLayer()
  bool  continuousMode = true;
  bool  rowEnabled[10] = {};
  float rowValue[10]   = {1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  std::atomic<bool> triggerFired{false};
  dsp::SchmittTrigger trigInputDetector;
  dsp::SchmittTrigger trigButtonDetector;

  ComputerscareGlolyPitch() {
    config(NUM_PARAMS, NUM_INPUTS, 0, 0);
    configSwitch(CONTINUOUS_TOGGLE, 0.f, 1.f, 1.f, "Continuous/Triggered", {"Triggered", "Continuous"});
    configParam (TRIGGER_BUTTON,    0.f, 1.f, 0.f, "Trigger");

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
    configSwitch(INVERT_TOGGLE, 0.f, 1.f, 1.f, "Fold", {"Off", "On"});
    configParam (INVERT_KNOB,   0.f,  1.f, 0.f, "Fold Frequency");
    configParam (INVERT_ATTEN, -1.f,  1.f, 0.f, "Fold CV Atten");
    configSwitch(CURVES_TOGGLE, 0.f, 1.f, 1.f, "Warp", {"Off", "On"});
    configParam (CURVES_KNOB,  -1.f,  1.f, 0.f, "Warp");
    configParam (CURVES_ATTEN, -1.f,  1.f, 0.f, "Warp CV Atten");

    configInput(HUE_GATE_INPUT,          "Hue Gate");
    configInput(INVERT_GATE_INPUT,       "Invert Gate");
    configInput(CURVES_GATE_INPUT,       "Curves Gate");
    configInput(HUE_CV_INPUT,            "Hue CV");
    configInput(INVERT_CV_INPUT,         "Invert CV");
    configInput(CURVES_CV_INPUT,         "Curves CV");
    configInput(CONTINUOUS_GATE_INPUT,   "Continuous (gate)");
    configInput(TRIGGER_INPUT,           "Trigger");
  }

  void onRandomize(const RandomizeEvent& e) override {
    for (int id = 0; id < NUM_PARAMS; id++) {
      if (id == CONTINUOUS_TOGGLE) continue;
      if (id == TRIGGER_BUTTON)    continue;
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
    static const float mins[10]    = {0.1f, 0.1f, 0.1f, -360.f,  0.f, -1.f, -1.f, -360.f, 0.f, -1.f};
    static const float maxs[10]    = {4.f,  4.f,  4.f,   360.f, 12.f,  1.f,  1.f,  360.f, 1.f,  1.f};
    static const float cvScale[10] = {0.3f, 0.3f, 0.3f, 36.f, 1.1f, 0.1f, 0.1f, 36.f, 0.1f, 0.1f};

    // Continuous/triggered mode: gate jack overrides button when connected
    bool gateConnected = inputs[CONTINUOUS_GATE_INPUT].isConnected();
    continuousMode = gateConnected
        ? (inputs[CONTINUOUS_GATE_INPUT].getVoltage() > 0.5f)
        : (params[CONTINUOUS_TOGGLE].getValue() > 0.5f);

    if (!continuousMode) {
      // In triggered mode, detect rising edges on trigger jack or trigger button
      bool fired = false;
      if (inputs[TRIGGER_INPUT].isConnected()) {
        fired |= trigInputDetector.process(inputs[TRIGGER_INPUT].getVoltage(), 0.1f, 2.f);
      }
      fired |= trigButtonDetector.process(params[TRIGGER_BUTTON].getValue(), 0.1f, 0.5f);
      if (fired) triggerFired.store(true);
    } else {
      trigInputDetector.reset();
      trigButtonDetector.reset();
    }

    for (int i = 0; i < 10; i++) {
      bool gateConn = inputs[gateIds[i]].isConnected();
      rowEnabled[i] = gateConn
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
    json_object_set_new(rootJ, "maintainAspect", json_boolean(maintainAspect));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) width = json_number_value(widthJ);
    json_t* tileJ = json_object_get(rootJ, "tileEmptySpace");
    if (tileJ) tileEmptySpace = json_boolean_value(tileJ);
    json_t* aspectJ = json_object_get(rootJ, "maintainAspect");
    if (aspectJ) maintainAspect = json_boolean_value(aspectJ);
    loadedJSON = false;
  }
};

// ─── Backdrop Widget ─────────────────────────────────────────────────────────
// Inserted as the bottom-most child of APP->scene->rack so it draws behind
// all modules.  Uses the same rendering pipeline as the main widget but maps
// the effect across the full visible viewport.

struct GlolyPitchBackdropWidget : widget::Widget {
  ComputerscareGlolyPitch* module = nullptr;
  ScreenCapture             screenCap;
  ColorTransformFBO         colorFBO;

  GlolyPitchBackdropWidget() {
    box.pos  = math::Vec(0.f, 0.f);
    box.size = math::Vec(INFINITY, INFINITY);
  }

  void draw(const DrawArgs& args) override {
    if (!module) return;

    float vpW = args.clipBox.size.x;
    float vpH = args.clipBox.size.y;
    float vpX = args.clipBox.pos.x;
    float vpY = args.clipBox.pos.y;
    if (vpW <= 1.f || vpH <= 1.f) return;

    bool doCapture = module->continuousMode || module->triggerFired.load();
    if (!doCapture && screenCap.nvgImg < 0) return;

    if (doCapture) screenCap.capture(args.vg);
    if (screenCap.nvgImg < 0) return;

    int fbW, fbH;
    glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

    bool*  en = module->rowEnabled;
    float* rv = module->rowValue;

    bool  scaleOn  = en[0], scaleXOn = en[1], scaleYOn = en[2];
    bool  rotOn    = en[3], kaliOn   = en[4];
    bool  txOn     = en[5], tyOn     = en[6];
    bool  hueOn    = en[7], invertOn = en[8], curvesOn = en[9];

    float scaleV   = rv[0], scaleXV  = rv[1], scaleYV  = rv[2];
    float rotV     = rv[3], kaliV    = rv[4];
    float txV      = rv[5], tyV      = rv[6];
    float hueV     = hueOn    ? rv[7] : 0.f;
    float foldFreqV = invertOn ? (1.0f + rv[8] * 3.0f) : 1.0f;
    float warpV    = curvesOn ? rv[9] : 0.f;

    float alpha   = 0.85f;
    float sx      = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
    float sy      = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
    float imgW    = vpW;
    float imgHW   = imgW * 0.5f;
    float hh      = vpH  * 0.5f;
    float txLocal = txOn ? txV * imgW : 0.f;
    float tyLocal = tyOn ? tyV * vpH  : 0.f;
    float txAbs   = fabsf(txLocal);
    float tyAbs   = fabsf(tyLocal);

    int img = colorFBO.apply(args.vg, screenCap.texId, screenCap.nvgImg,
                              fbW, fbH, hueV, warpV, foldFreqV);

    nvgSave(args.vg);
    nvgScissor(args.vg, vpX, vpY, vpW, vpH);
    nvgTranslate(args.vg, vpX + imgHW, vpY + hh);
    if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

    int kaliMode = kaliOn ? (int)kaliV : 0;

    if (kaliMode > 0) {
      if (txOn || tyOn) nvgTranslate(args.vg, txLocal, tyLocal);
      float cosA  = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
      float sinA  = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
      float rHW   = ((imgW + 2.f*txAbs)*cosA + (vpH + 2.f*tyAbs)*sinA) / (2.f*std::max(sx, 0.01f)) + 4.f;
      float rHH   = ((imgW + 2.f*txAbs)*sinA + (vpH + 2.f*tyAbs)*cosA) / (2.f*std::max(sy, 0.01f)) + 4.f;
      float pcx   = -(txOn ? txLocal : 0.f);
      float pcy   = -(tyOn ? tyLocal : 0.f);
      float dispHW = imgHW / std::max(sx, 0.01f);
      float dispHH = hh    / std::max(sy, 0.01f);
      drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH, kaliMode, alpha,
                       rotOn, rotV, false, 0.f, false,
                       pcx - dispHW, pcy - dispHH, 2.f * dispHW, 2.f * dispHH);
    } else {
      if (rotOn) applyRotation(args.vg, rotV);
      if (txOn || tyOn) nvgTranslate(args.vg, txLocal, tyLocal);
      float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
      float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
      float rHW  = ((imgW + 2.f*txAbs)*cosA + (vpH + 2.f*tyAbs)*sinA) / (2.f*std::max(sx, 0.01f)) + 4.f;
      float rHH  = ((imgW + 2.f*txAbs)*sinA + (vpH + 2.f*tyAbs)*cosA) / (2.f*std::max(sy, 0.01f)) + 4.f;
      NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH, 0.f, img, alpha);
      nvgBeginPath(args.vg);
      nvgRect(args.vg, -rHW, -rHH, 2.f * rHW, 2.f * rHH);
      nvgFillPaint(args.vg, p);
      nvgFill(args.vg);
    }

    nvgRestore(args.vg);
  }
};

// ─── Widget ──────────────────────────────────────────────────────────────────

static const float CONTROLS_WIDTH = 11 * RACK_GRID_WIDTH;  // 165 px / 11 HP

struct ComputerscareGlolyPitchWidget : ModuleWidget {
  BGPanel*                   bgPanel;
  ComputerscareResizeHandle* rightHandle;
  SvgWidget*                 topLogo;
  ScreenCapture              screenCap;
  ColorTransformFBO          colorFBO;
  GlolyPitchBackdropWidget*  backdropWidget = nullptr;

  // Cache for triggered mode — holds the last rendered frame's params
  bool  cachedRowEnabled[10] = {};
  float cachedRowValue[10]   = {};
  bool  hasValidCache        = false;

  ~ComputerscareGlolyPitchWidget() {
    if (backdropWidget) {
      if (APP->scene && APP->scene->rack)
        APP->scene->rack->removeChild(backdropWidget);
      delete backdropWidget;
    }
  }

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

    // ── Logo — top left ───────────────────────────────────────────────────────
    topLogo = new SvgWidget();
    topLogo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-light.svg")));
    topLogo->box.pos = Vec(2.f, 3.f);
    addChild(topLogo);

    // ── Global mode controls ──────────────────────────────────────────────────
    // Both jacks share the same Y (aligned). Button is positioned relative to its jack.
    const float HDR_JACK_Y = 9.f;
    const float HDR_BTN_DX = -24.f;  // button x = jack x + this
    const float HDR_BTN_DY = -2.f;   // button y = jack y + this

    const float CONT_JACK_X = 68.f;
    const float TRIG_JACK_X = 122.f;

    auto addHdrLabel = [&](float x, const char* text) {
      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos       = Vec(x, HDR_JACK_Y - 8.f);  // above jack
      lbl->box.size      = Vec(50.f, 14.f);
      lbl->fontSize      = 12;
      lbl->value         = text;
      lbl->textColor     = nvgRGBf(0.9f, 1.0f, 0.95f);
      lbl->baseColor     = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 50.f;
      addChild(lbl);
    };

    addHdrLabel(CONT_JACK_X + HDR_BTN_DX, "CONT");
    addParam(createParam<SmallIsoButton>(
        Vec(CONT_JACK_X + HDR_BTN_DX, HDR_JACK_Y + HDR_BTN_DY),
        module, ComputerscareGlolyPitch::CONTINUOUS_TOGGLE));
    addInput(createInput<InPort>(
        Vec(CONT_JACK_X, HDR_JACK_Y),
        module, ComputerscareGlolyPitch::CONTINUOUS_GATE_INPUT));

    addHdrLabel(TRIG_JACK_X + HDR_BTN_DX, "TRIG");
    addParam(createParam<GlolyPitchTrigButton>(
        Vec(TRIG_JACK_X + HDR_BTN_DX, HDR_JACK_Y + HDR_BTN_DY),
        module, ComputerscareGlolyPitch::TRIGGER_BUTTON));
    addInput(createInput<InPort>(
        Vec(TRIG_JACK_X, HDR_JACK_Y),
        module, ComputerscareGlolyPitch::TRIGGER_INPUT));

    // ── 10 effect rows (7 geometry + 3 color) ────────────────────────────────
    // Shifted down 16px from original to accommodate header controls.
    const int N = 10;
    float rowY[N] = {55.f, 90.f, 125.f, 160.f, 195.f, 230.f, 265.f, 300.f, 335.f, 364.f};
    const char* rowLabels[N] = {
        "SCALE", "SCL X", "SCL Y", "ROT", "KALI", "TRN X", "TRN Y",
        "HUE", "FOLD", "WARP"
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

      addParam(createParam<SmallIsoButton>(Vec(2.f,   y - 14.f), module, toggleIds[i]));
      addInput(createInput<OutPort>(       Vec(30.f,  y - 17.f), module, gateInputIds[i]));
      addInput(createInput<InPort>(        Vec(68.f,  y - 14.f), module, cvInputIds[i]));
      addParam(createParam<SmallKnob>(     Vec(100.f, y - 9.f),  module, attenIds[i]));
      addParam(createParam<SmoothKnob>(    Vec(122.f, y - 13.f), module, knobIds[i]));
    }

  }

  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && module && APP->scene && APP->scene->rack && !backdropWidget) {
      ComputerscareGlolyPitch* m = dynamic_cast<ComputerscareGlolyPitch*>(module);

      // In triggered mode, only re-capture when a trigger fired; otherwise hold last image
      bool doCapture = m->continuousMode || m->triggerFired.exchange(false);

      if (m && (doCapture || hasValidCache)) {
        float alpha = 0.85f;

        // Choose live params on capture, cached params when holding
        bool*  en = doCapture ? m->rowEnabled : cachedRowEnabled;
        float* rv = doCapture ? m->rowValue   : cachedRowValue;

        bool  scaleOn  = en[0];
        bool  scaleXOn = en[1];
        bool  scaleYOn = en[2];
        bool  rotOn    = en[3];
        bool  kaliOn   = en[4];
        bool  txOn     = en[5];
        bool  tyOn     = en[6];
        bool  hueOn    = en[7];
        bool  invertOn = en[8];
        bool  curvesOn = en[9];

        float scaleV   = rv[0];
        float scaleXV  = rv[1];
        float scaleYV  = rv[2];
        float rotV     = rv[3];
        float kaliV    = rv[4];
        float txV      = rv[5];
        float tyV      = rv[6];
        float hueV      = hueOn    ? rv[7] : 0.f;
        // FOLD knob (0..1) maps to foldFreq (1..4); 1.0 = no chromatic divergence
        float foldFreqV = invertOn ? (1.0f + rv[8] * 3.0f) : 1.0f;
        float warpV     = curvesOn ? rv[9] : 0.f;

        float mirrorW = box.size.x - CONTROLS_WIDTH;
        float mirrorH = box.size.y;

        if (mirrorW <= 2.f) {
          ModuleWidget::drawLayer(args, layer);
          return;
        }

        if (doCapture) {
          screenCap.capture(args.vg);
          // Update cache with current params
          memcpy(cachedRowEnabled, m->rowEnabled, sizeof(cachedRowEnabled));
          memcpy(cachedRowValue,   m->rowValue,   sizeof(cachedRowValue));
          hasValidCache = true;
        }

        if (screenCap.nvgImg < 0) {
          ModuleWidget::drawLayer(args, layer);
          return;
        }

        int fbW, fbH;
        glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

        float hw = mirrorW * 0.5f;   // display half-width (for scissor/translate)
        float hh = mirrorH * 0.5f;

        // In aspect-ratio mode, size the image to the framebuffer's natural aspect.
        // The scissor clips pillarboxes; scaleX/Y are independent NVG transforms.
        float imgW  = m->maintainAspect
                        ? (mirrorH * (float)fbW / (float)fbH)
                        : mirrorW;
        float imgHW = imgW * 0.5f;

        float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
        float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
        // Translation is relative to one image width/height
        float txLocal = txOn ? txV * imgW  : 0.f;
        float tyLocal = tyOn ? tyV * mirrorH : 0.f;
        float txAbs   = fabsf(txLocal);
        float tyAbs   = fabsf(tyLocal);

        nvgSave(args.vg);

        nvgScissor(args.vg, CONTROLS_WIDTH, 0.f, mirrorW, box.size.y);
        nvgTranslate(args.vg, CONTROLS_WIDTH + hw, hh);  // center of display area

        if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

        bool tileOn = m->tileEmptySpace;
        int img = colorFBO.apply(args.vg, screenCap.texId, screenCap.nvgImg,
                                  fbW, fbH, hueV, warpV, foldFreqV);

        int kaliMode = kaliOn ? (int)kaliV : 0;

        if (kaliMode > 0) {
          // No global rotation here — each sector applies its own rotation with
          // sign correction for flipped sectors (see kSector in MirrorKaleidoscope.hpp).
          if (txOn || tyOn) nvgTranslate(args.vg, txLocal, tyLocal);
          float cosA  = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
          float sinA  = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW   = ((imgW + 2.f * txAbs) * cosA + (box.size.y + 2.f * tyAbs) * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH   = ((imgW + 2.f * txAbs) * sinA + (box.size.y + 2.f * tyAbs) * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;
          // Display bounds in the current (post-txLocal) coordinate space.
          // kSector uses these to guarantee a non-degenerate scissor intersection.
          float pcx    = -(txOn ? txLocal : 0.f);
          float pcy    = -(tyOn ? tyLocal : 0.f);
          float dispHW = hw / std::max(sx, 0.01f);
          float dispHH = hh / std::max(sy, 0.01f);

          if (tileOn) {
            int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW);
            int iMax = (int)floorf((pcx + rHW + imgHW) / imgW);
            int jMin = (int)ceilf((pcy - rHH - hh) / mirrorH);
            int jMax = (int)floorf((pcy + rHH + hh) / mirrorH);
            iMin = std::max(iMin, -20); iMax = std::min(iMax, 20);
            jMin = std::max(jMin, -20); jMax = std::min(jMax, 20);
            for (int j = jMin; j <= jMax; j++) {
              for (int i = iMin; i <= iMax; i++) {
                nvgSave(args.vg);
                nvgTranslate(args.vg, i * imgW, j * mirrorH);
                // Display bounds shifted into this tile's coordinate space.
                float dX = pcx - dispHW - (float)i * imgW;
                float dY = pcy - dispHH - (float)j * mirrorH;
                drawKaleidoscope(args.vg, img, imgHW, hh, imgW, mirrorH, rHW, rHH, kaliMode, alpha,
                                 rotOn, rotV, false, 0.f, false,
                                 dX, dY, 2.f * dispHW, 2.f * dispHH);
                nvgRestore(args.vg);
              }
            }
          } else {
            // Draw kaleidoscope first (large coverHW rect fills each sector fully).
            drawKaleidoscope(args.vg, img, imgHW, hh, imgW, mirrorH, rHW, rHH, kaliMode, alpha,
                             rotOn, rotV, false, 0.f, false,
                             pcx - dispHW, pcy - dispHH, 2.f * dispHW, 2.f * dispHH);

            // Overlay opaque grey strips covering display area outside the image bounds.
            // Drawing grey ON TOP means its soft NVG edges blend grey-into-grey (invisible)
            // rather than grey-into-image (smear).  A 2px overlap into the image eats any
            // residual tiling bleed at the image boundary.
            if (dispHW > imgHW || dispHH > hh) {
              static const float OVR = 2.f;
              nvgFillColor(args.vg, nvgRGB(0x23, 0x21, 0x29));
              float imgL = pcx - imgHW, imgR = pcx + imgHW;
              float imgT = pcy - hh,    imgB = pcy + hh;
              float dspL = pcx - dispHW, dspR = pcx + dispHW;
              float dspT = pcy - dispHH, dspB = pcy + dispHH;
              auto fill = [&](float x, float y, float w, float h) {
                if (w > 0.f && h > 0.f) {
                  nvgBeginPath(args.vg);
                  nvgRect(args.vg, x, y, w, h);
                  nvgFill(args.vg);
                }
              };
              // left/right full-height strips
              fill(dspL,        dspT, imgL - dspL + OVR, dspB - dspT);
              fill(imgR - OVR,  dspT, dspR - imgR + OVR, dspB - dspT);
              // top/bottom strips (span only the non-left/right region)
              fill(imgL + OVR, dspT, imgR - imgL - 2.f*OVR, imgT - dspT + OVR);
              fill(imgL + OVR, imgB - OVR, imgR - imgL - 2.f*OVR, dspB - imgB + OVR);
            }
          }
        } else {
          if (rotOn) applyRotation(args.vg, rotV);
          if (txOn || tyOn) nvgTranslate(args.vg, txLocal, tyLocal);
          float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          float rHW  = ((imgW + 2.f * txAbs) * cosA + (box.size.y + 2.f * tyAbs) * sinA) / (2.f * std::max(sx, 0.01f)) + 4.f;
          float rHH  = ((imgW + 2.f * txAbs) * sinA + (box.size.y + 2.f * tyAbs) * cosA) / (2.f * std::max(sy, 0.01f)) + 4.f;

          if (tileOn) {
            float pcx = -(txOn ? txLocal : 0.f);
            float pcy = -(tyOn ? tyLocal : 0.f);
            int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW);
            int iMax = (int)floorf((pcx + rHW + imgHW) / imgW);
            int jMin = (int)ceilf((pcy - rHH - hh) / mirrorH);
            int jMax = (int)floorf((pcy + rHH + hh) / mirrorH);
            iMin = std::max(iMin, -20); iMax = std::min(iMax, 20);
            jMin = std::max(jMin, -20); jMax = std::min(jMax, 20);
            for (int j = jMin; j <= jMax; j++) {
              for (int i = iMin; i <= iMax; i++) {
                float ox = -imgHW + i * imgW;
                float oy = -hh + j * mirrorH;
                NVGpaint p = nvgImagePattern(args.vg, ox, oy, imgW, mirrorH,
                                             0.f, img, alpha);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, ox, oy, imgW, mirrorH);
                nvgFillPaint(args.vg, p);
                nvgFill(args.vg);
              }
            }
          } else {
            // Grey background first — large rect clips to outer display scissor.
            nvgBeginPath(args.vg);
            nvgRect(args.vg, -rHW, -rHH, 2.f * rHW, 2.f * rHH);
            nvgFillColor(args.vg, nvgRGB(0x23, 0x21, 0x29));
            nvgFill(args.vg);
            // Image drawn with exact rect — no tiling, no smear at edges.
            NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, mirrorH,
                                         0.f, img, alpha);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, -imgHW, -hh, imgW, mirrorH);
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
    menu->addChild(createMenuItem(
      backdropWidget ? "Disable rack background" : "Render as rack background",
      "", [=]() {
        if (backdropWidget) {
          if (APP->scene && APP->scene->rack)
            APP->scene->rack->removeChild(backdropWidget);
          delete backdropWidget;
          backdropWidget = nullptr;
          bgPanel->box.size.x = box.size.x;
        } else {
          auto* w = new GlolyPitchBackdropWidget();
          w->module = m;
          backdropWidget = w;
          bgPanel->box.size.x = CONTROLS_WIDTH;
          if (APP->scene && APP->scene->rack) {
            auto& rc = APP->scene->rack->children;
            if (!rc.empty())
              // Insert after the first child (RailWidget) so the effect
              // draws on top of the rail background but beneath modules.
              APP->scene->rack->addChildAbove(w, rc.front());
            else
              APP->scene->rack->addChild(w);
          }
        }
      }
    ));
    menu->addChild(createSubmenuItem("Visual", "", [=](Menu* menu) {
      menu->addChild(createBoolPtrMenuItem("Tile empty space",    "", &m->tileEmptySpace));
      menu->addChild(createBoolPtrMenuItem("Maintain aspect ratio", "", &m->maintainAspect));
    }));
  }

  void step() override {
    if (module) {
      ComputerscareGlolyPitch* m = dynamic_cast<ComputerscareGlolyPitch*>(module);
      if (!m->loadedJSON) {
        box.size.x = m->width;
        if (!backdropWidget) bgPanel->box.size.x = m->width;
        rightHandle->box.pos.x = m->width - rightHandle->box.size.x;
        m->loadedJSON = true;
      } else if (box.size.x != m->width) {
        m->width = box.size.x;
        if (!backdropWidget) bgPanel->box.size.x = box.size.x;
        rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
      }
    }
    ModuleWidget::step();
  }
};

Model* modelComputerscareGlolyPitch =
    createModel<ComputerscareGlolyPitch, ComputerscareGlolyPitchWidget>(
        "computerscare-gloly-pitch");
