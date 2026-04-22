#include <dirent.h>
#include <osdialog.h>

#include <atomic>

#include "../Computerscare.hpp"
#include "../ComputerscareResizableHandle.hpp"
#include "ColorTransformFBO.hpp"
#include "FlowerKaleid.hpp"
#include "MirrorKaleidoscope.hpp"
#include "MirrorRotation.hpp"
#include "ScreenCaptureEffect.hpp"

// Momentary version of SmallIsoButton for the trigger
struct PortaloofTrigButton : SmallIsoButton {
  PortaloofTrigButton() { momentary = true; }
};

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscarePortaloof : Module {
  enum ParamId {
    CONTINUOUS_TOGGLE,  // on=continuous (every frame), off=triggered
    TRIGGER_BUTTON,     // momentary: fires one draw frame
    // Effects: toggle + knob (offset) + attenuverter triples
    SCALE_TOGGLE,
    SCALE_KNOB,
    SCALE_ATTEN,
    SCALE_X_TOGGLE,
    SCALE_X_KNOB,
    SCALE_X_ATTEN,
    SCALE_Y_TOGGLE,
    SCALE_Y_KNOB,
    SCALE_Y_ATTEN,
    ROT_TOGGLE,
    ROT_KNOB,
    ROT_ATTEN,
    KALEIDO_TOGGLE,
    KALEIDO_KNOB,
    KALEIDO_ATTEN,
    TRANS_X_TOGGLE,
    TRANS_X_KNOB,
    TRANS_X_ATTEN,
    TRANS_Y_TOGGLE,
    TRANS_Y_KNOB,
    TRANS_Y_ATTEN,
    // Color transforms
    HUE_TOGGLE,
    HUE_KNOB,
    HUE_ATTEN,
    INVERT_TOGGLE,
    INVERT_KNOB,
    INVERT_ATTEN,
    CURVES_TOGGLE,
    CURVES_KNOB,
    CURVES_ATTEN,
    BACKDROP_ALPHA,
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
  bool tileEmptySpace = true;
  bool maintainAspect = true;
  bool backdropEnabled = false;
  bool emptyWindowInBgMode = true;
  bool transformPost = true;
  bool translateFirst =
      false;            // false = Kaleid > Translate, true = Translate > Kaleid
  int kaleidStyle = 0;  // 0 = classic, 1 = premium (flower)
  std::string loadedImagePath;

  // Effective row state — computed in process(), read in drawLayer()
  bool continuousMode = true;
  bool rowEnabled[10] = {};
  float rowValue[10] = {1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  std::atomic<bool> triggerFired{false};
  std::atomic<bool> backdropTriggerFired{false};
  std::atomic<bool> imageGateActive{false};
  dsp::SchmittTrigger trigInputDetector;
  dsp::SchmittTrigger trigButtonDetector;

  ComputerscarePortaloof() {
    config(NUM_PARAMS, NUM_INPUTS, 0, 0);
    configSwitch(CONTINUOUS_TOGGLE, 0.f, 1.f, 1.f, "Continuous/Triggered",
                 {"Triggered", "Continuous"});
    configParam(TRIGGER_BUTTON, 0.f, 1.f, 0.f, "Trigger");

    configSwitch(SCALE_TOGGLE, 0.f, 1.f, 1.f, "Scale", {"Off", "On"});
    configParam(SCALE_KNOB, 0.1f, 4.f, 1.f, "Scale");
    configParam(SCALE_ATTEN, -1.f, 1.f, 0.f, "Scale CV Atten");
    configSwitch(SCALE_X_TOGGLE, 0.f, 1.f, 1.f, "Scale X", {"Off", "On"});
    configParam(SCALE_X_KNOB, -5.f, 5.f, 1.f, "Scale X");
    configParam(SCALE_X_ATTEN, -1.f, 1.f, 0.f, "Scale X CV Atten");
    configSwitch(SCALE_Y_TOGGLE, 0.f, 1.f, 1.f, "Scale Y", {"Off", "On"});
    configParam(SCALE_Y_KNOB, -5.f, 5.f, 1.f, "Scale Y");
    configParam(SCALE_Y_ATTEN, -1.f, 1.f, 0.f, "Scale Y CV Atten");
    configSwitch(ROT_TOGGLE, 0.f, 1.f, 1.f, "Rotation", {"Off", "On"});
    configParam(ROT_KNOB, -180.f, 180.f, 0.f, "Rotation", "\u00b0");
    configParam(ROT_ATTEN, -1.f, 1.f, 0.f, "Rotation CV Atten");
    configSwitch(KALEIDO_TOGGLE, 0.f, 1.f, 1.f, "Kaleidoscope", {"Off", "On"});
    configSwitch(
        KALEIDO_KNOB, 0.f, 12.f, 0.f, "Kaleido Mode",
        {"Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"});
    configParam(KALEIDO_ATTEN, -1.f, 1.f, 0.f, "Kaleido CV Atten");
    configSwitch(TRANS_X_TOGGLE, 0.f, 1.f, 1.f, "Translate X", {"Off", "On"});
    configParam(TRANS_X_KNOB, -1.f, 1.f, 0.f, "Translate X");
    configParam(TRANS_X_ATTEN, -1.f, 1.f, 0.f, "Translate X CV Atten");
    configSwitch(TRANS_Y_TOGGLE, 0.f, 1.f, 1.f, "Translate Y", {"Off", "On"});
    configParam(TRANS_Y_KNOB, -1.f, 1.f, 0.f, "Translate Y");
    configParam(TRANS_Y_ATTEN, -1.f, 1.f, 0.f, "Translate Y CV Atten");

    configInput(SCALE_GATE_INPUT, "Scale Gate");
    configInput(SCALE_X_GATE_INPUT, "Scale X Gate");
    configInput(SCALE_Y_GATE_INPUT, "Scale Y Gate");
    configInput(ROT_GATE_INPUT, "Rotation Gate");
    configInput(KALEIDO_GATE_INPUT, "Kaleido Gate");
    configInput(TRANS_X_GATE_INPUT, "Translate X Gate");
    configInput(TRANS_Y_GATE_INPUT, "Translate Y Gate");
    configInput(SCALE_CV_INPUT, "Scale CV");
    configInput(SCALE_X_CV_INPUT, "Scale X CV");
    configInput(SCALE_Y_CV_INPUT, "Scale Y CV");
    configInput(ROT_CV_INPUT, "Rotation CV");
    configInput(KALEIDO_CV_INPUT, "Kaleido CV");
    configInput(TRANS_X_CV_INPUT, "Translate X CV");
    configInput(TRANS_Y_CV_INPUT, "Translate Y CV");
    configSwitch(HUE_TOGGLE, 0.f, 1.f, 1.f, "Hue", {"Off", "On"});
    configParam(HUE_KNOB, -180.f, 180.f, 0.f, "Hue", "\u00b0");
    configParam(HUE_ATTEN, -1.f, 1.f, 0.f, "Hue CV Atten");
    configSwitch(INVERT_TOGGLE, 0.f, 1.f, 1.f, "Fold", {"Off", "On"});
    configParam(INVERT_KNOB, 0.f, 1.f, 0.2f, "Fold Frequency");
    configParam(INVERT_ATTEN, -1.f, 1.f, 0.f, "Fold CV Atten");
    configSwitch(CURVES_TOGGLE, 0.f, 1.f, 1.f, "Warp", {"Off", "On"});
    configParam(CURVES_KNOB, -1.f, 1.f, 0.f, "Warp");
    configParam(CURVES_ATTEN, -1.f, 1.f, 0.f, "Warp CV Atten");
    configParam(BACKDROP_ALPHA, 0.f, 1.f, 0.85f, "Backdrop Alpha");

    configInput(HUE_GATE_INPUT, "Hue Gate");
    configInput(INVERT_GATE_INPUT, "Invert Gate");
    configInput(CURVES_GATE_INPUT, "Curves Gate");
    configInput(HUE_CV_INPUT, "Hue CV");
    configInput(INVERT_CV_INPUT, "Invert CV");
    configInput(CURVES_CV_INPUT, "Curves CV");
    configInput(CONTINUOUS_GATE_INPUT, "Continuous (gate)");
    configInput(TRIGGER_INPUT, "Trigger");
  }

  void onRandomize(const RandomizeEvent& e) override {
    for (int id = 0; id < NUM_PARAMS; id++) {
      if (id == CONTINUOUS_TOGGLE) continue;
      if (id == TRIGGER_BUTTON) continue;
      if (id == BACKDROP_ALPHA) continue;
      paramQuantities[id]->randomize();
    }
  }

  void process(const ProcessArgs& args) override {
    static const int toggleIds[10] = {
        SCALE_TOGGLE,   SCALE_X_TOGGLE, SCALE_Y_TOGGLE, ROT_TOGGLE,
        KALEIDO_TOGGLE, TRANS_X_TOGGLE, TRANS_Y_TOGGLE, HUE_TOGGLE,
        INVERT_TOGGLE,  CURVES_TOGGLE};
    static const int knobIds[10] = {
        SCALE_KNOB,   SCALE_X_KNOB, SCALE_Y_KNOB, ROT_KNOB,    KALEIDO_KNOB,
        TRANS_X_KNOB, TRANS_Y_KNOB, HUE_KNOB,     INVERT_KNOB, CURVES_KNOB};
    static const int attenIds[10] = {
        SCALE_ATTEN,   SCALE_X_ATTEN, SCALE_Y_ATTEN, ROT_ATTEN,
        KALEIDO_ATTEN, TRANS_X_ATTEN, TRANS_Y_ATTEN, HUE_ATTEN,
        INVERT_ATTEN,  CURVES_ATTEN};
    static const int gateIds[10] = {SCALE_GATE_INPUT,   SCALE_X_GATE_INPUT,
                                    SCALE_Y_GATE_INPUT, ROT_GATE_INPUT,
                                    KALEIDO_GATE_INPUT, TRANS_X_GATE_INPUT,
                                    TRANS_Y_GATE_INPUT, HUE_GATE_INPUT,
                                    INVERT_GATE_INPUT,  CURVES_GATE_INPUT};
    static const int cvIds[10] = {
        SCALE_CV_INPUT,   SCALE_X_CV_INPUT, SCALE_Y_CV_INPUT, ROT_CV_INPUT,
        KALEIDO_CV_INPUT, TRANS_X_CV_INPUT, TRANS_Y_CV_INPUT, HUE_CV_INPUT,
        INVERT_CV_INPUT,  CURVES_CV_INPUT};
    static const float mins[10] = {0.1f, -5.f, -5.f,   -360.f, 0.f,
                                   -1.f, -1.f, -360.f, 0.f,    -1.f};
    static const float maxs[10] = {4.f, 5.f, 5.f,   360.f, 12.f,
                                   1.f, 1.f, 360.f, 1.f,   1.f};
    static const float cvScale[10] = {0.3f, 0.5f, 0.5f, 36.f, 1.1f,
                                      0.1f, 0.1f, 36.f, 0.1f, 0.1f};

    // Continuous/triggered mode: gate jack overrides button when connected
    bool gateConnected = inputs[CONTINUOUS_GATE_INPUT].isConnected();
    continuousMode = gateConnected
                         ? (inputs[CONTINUOUS_GATE_INPUT].getVoltage() > 0.5f)
                         : (params[CONTINUOUS_TOGGLE].getValue() > 0.5f);

    if (!continuousMode) {
      // Triggered mode: detect rising edges to fire a screen capture
      bool fired = false;
      if (inputs[TRIGGER_INPUT].isConnected()) {
        fired |= trigInputDetector.process(inputs[TRIGGER_INPUT].getVoltage(),
                                           0.1f, 2.f);
      } else {
        trigInputDetector.reset();
      }
      fired |= trigButtonDetector.process(params[TRIGGER_BUTTON].getValue(),
                                          0.1f, 0.5f);
      if (fired) {
        triggerFired.store(true);
        backdropTriggerFired.store(true);
      }
      imageGateActive.store(false);
    } else {
      // Continuous mode: gate controls image source while held
      bool gateHigh = false;
      if (inputs[TRIGGER_INPUT].isConnected())
        gateHigh |= inputs[TRIGGER_INPUT].getVoltage() > 0.5f;
      gateHigh |= params[TRIGGER_BUTTON].getValue() > 0.5f;
      imageGateActive.store(gateHigh);
      trigInputDetector.reset();
      trigButtonDetector.reset();
    }

    for (int i = 0; i < 10; i++) {
      bool gateConn = inputs[gateIds[i]].isConnected();
      rowEnabled[i] = gateConn ? (inputs[gateIds[i]].getVoltage() > 0.5f)
                               : (params[toggleIds[i]].getValue() > 0.5f);

      float offset = params[knobIds[i]].getValue();
      float atten = params[attenIds[i]].getValue();
      float cv =
          inputs[cvIds[i]].isConnected() ? inputs[cvIds[i]].getVoltage() : 0.f;
      rowValue[i] = clamp(offset + atten * cv * cvScale[i], mins[i], maxs[i]);
    }

    // Exponential scale map for Scale X (row 1) and Scale Y (row 2).
    // Maps knob [-5, 5] → [-5, -0.1] | [0.1, 5] with two segments:
    //   [0, 1] → [0.1, 1.0] quadratic (fine control near center)
    //   [1, 5] → [1.0, 5.0] exponential (coarser at extremes)
    // Default knob=1 → scale=1.0 (identity). Negative values mirror the axis.
    for (int si = 1; si <= 2; si++) {
      float k = rowValue[si];
      float sign = (k >= 0.f) ? 1.f : -1.f;
      float a = fabsf(k);
      float result;
      if (a <= 1.f) {
        result = 0.1f + 0.9f * (a * a);
      } else {
        result = powf(5.f, (a - 1.f) / 4.f);
      }
      rowValue[si] = sign * result;
    }
  }

  void onReset(const ResetEvent& e) override {
    float contVal = params[CONTINUOUS_TOGGLE].getValue();
    Module::onReset(e);
    params[CONTINUOUS_TOGGLE].setValue(contVal);
    backdropEnabled = false;
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "width", json_real(width));
    json_object_set_new(rootJ, "tileEmptySpace", json_boolean(tileEmptySpace));
    json_object_set_new(rootJ, "maintainAspect", json_boolean(maintainAspect));
    json_object_set_new(rootJ, "backdropEnabled",
                        json_boolean(backdropEnabled));
    json_object_set_new(rootJ, "emptyWindowInBgMode",
                        json_boolean(emptyWindowInBgMode));
    json_object_set_new(rootJ, "transformPost", json_boolean(transformPost));
    json_object_set_new(rootJ, "translateFirst", json_boolean(translateFirst));
    json_object_set_new(rootJ, "kaleidStyle", json_integer(kaleidStyle));
    if (!loadedImagePath.empty())
      json_object_set_new(rootJ, "loadedImagePath",
                          json_string(loadedImagePath.c_str()));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* widthJ = json_object_get(rootJ, "width");
    if (widthJ) width = json_number_value(widthJ);
    json_t* tileJ = json_object_get(rootJ, "tileEmptySpace");
    if (tileJ) tileEmptySpace = json_boolean_value(tileJ);
    json_t* aspectJ = json_object_get(rootJ, "maintainAspect");
    if (aspectJ) maintainAspect = json_boolean_value(aspectJ);
    json_t* bdJ = json_object_get(rootJ, "backdropEnabled");
    if (bdJ) backdropEnabled = json_boolean_value(bdJ);
    json_t* ewJ = json_object_get(rootJ, "emptyWindowInBgMode");
    if (ewJ) emptyWindowInBgMode = json_boolean_value(ewJ);
    json_t* tpJ = json_object_get(rootJ, "transformPost");
    if (tpJ) transformPost = json_boolean_value(tpJ);
    json_t* tfJ = json_object_get(rootJ, "translateFirst");
    if (tfJ) translateFirst = json_boolean_value(tfJ);
    json_t* ksJ = json_object_get(rootJ, "kaleidStyle");
    if (ksJ) kaleidStyle = (int)json_integer_value(ksJ);
    json_t* lipJ = json_object_get(rootJ, "loadedImagePath");
    if (lipJ) loadedImagePath = json_string_value(lipJ);
    loadedJSON = false;
  }
};

// ─── Backdrop Widget ─────────────────────────────────────────────────────────
// Inserted as the bottom-most child of APP->scene->rack so it draws behind
// all modules.  Uses the same rendering pipeline as the main widget but maps
// the effect across the full visible viewport.

struct PortaloofBackdropWidget : widget::Widget {
  ComputerscarePortaloof* module = nullptr;
  ScreenCapture screenCap;
  ColorTransformFBO colorFBO;
  FlowerKaleidFBO flowerKaleidFBO;
  bool cachedRowEnabled[10] = {};
  float cachedRowValue[10] = {};
  bool hasValidCache = false;

  // Loaded image injection
  std::string lastImagePath;
  int loadedNvgImg = -1;
  GLuint loadedTexId = 0;
  bool pendingInject = false;

  PortaloofBackdropWidget() {
    box.pos = math::Vec(0.f, 0.f);
    box.size = math::Vec(INFINITY, INFINITY);
  }

  void draw(const DrawArgs& args) override {
    if (!module) return;

    float vpW = args.clipBox.size.x;
    float vpH = args.clipBox.size.y;
    float vpX = args.clipBox.pos.x;
    float vpY = args.clipBox.pos.y;
    if (vpW <= 1.f || vpH <= 1.f) return;

    // Load/reload image if the module's path changed
    if (module->loadedImagePath != lastImagePath) {
      if (loadedNvgImg >= 0) {
        nvgDeleteImage(args.vg, loadedNvgImg);
        loadedNvgImg = -1;
        loadedTexId = 0;
      }
      lastImagePath = module->loadedImagePath;
      if (!lastImagePath.empty()) {
        loadedNvgImg = nvgCreateImage(args.vg, lastImagePath.c_str(), 0);
        if (loadedNvgImg >= 0) {
          loadedTexId = nvglImageHandleGL2(args.vg, loadedNvgImg);
          pendingInject = true;
        }
      }
    }

    bool gateActive = module->imageGateActive.load();
    bool doCapture =
        module->continuousMode || module->backdropTriggerFired.exchange(false);

    if (gateActive && loadedNvgImg >= 0 && loadedTexId != 0)
      pendingInject = true;

    bool useInjected = pendingInject && loadedNvgImg >= 0 && loadedTexId != 0;
    if (!doCapture && !useInjected && screenCap.nvgImg < 0) return;

    if (doCapture) {
      if (useInjected) {
        if (!gateActive) pendingInject = false;
        // Use loaded image — skip screen capture this frame
      } else {
        screenCap.capture(args.vg);
      }
      memcpy(cachedRowEnabled, module->rowEnabled, sizeof(cachedRowEnabled));
      memcpy(cachedRowValue, module->rowValue, sizeof(cachedRowValue));
      hasValidCache = true;
    }

    GLuint srcTex = useInjected ? loadedTexId : screenCap.texId;
    int srcImg = useInjected ? loadedNvgImg : screenCap.nvgImg;
    if (srcImg < 0) return;

    int fbW, fbH;
    glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

    // Transform post: use live params (knobs alter frozen image).
    // Transform pre:  use cached params (knobs only affect next snapshot).
    bool useLive = module->continuousMode || module->transformPost;
    bool* en =
        (useLive || !hasValidCache) ? module->rowEnabled : cachedRowEnabled;
    float* rv = (useLive || !hasValidCache) ? module->rowValue : cachedRowValue;

    bool scaleOn = en[0], scaleXOn = en[1], scaleYOn = en[2];
    bool rotOn = en[3], kaliOn = en[4];
    bool txOn = en[5], tyOn = en[6];
    bool hueOn = en[7], invertOn = en[8], curvesOn = en[9];

    float scaleV = rv[0], scaleXV = rv[1], scaleYV = rv[2];
    float rotV = rv[3], kaliV = rv[4];
    float txV = rv[5], tyV = rv[6];
    float hueV = hueOn ? rv[7] : 0.f;
    float foldFreqV = invertOn ? (1.0f + rv[8] * 3.0f) : 1.0f;
    float warpV = curvesOn ? rv[9] : 0.f;

    float alpha =
        module->params[ComputerscarePortaloof::BACKDROP_ALPHA].getValue();
    float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
    float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
    float imgW = vpW;
    float imgHW = imgW * 0.5f;
    float hh = vpH * 0.5f;
    float txLocal = txOn ? txV * imgW : 0.f;
    float tyLocal = tyOn ? tyV * vpH : 0.f;

    int img = colorFBO.apply(args.vg, srcTex, srcImg, fbW, fbH, hueV, warpV,
                             foldFreqV, useInjected);
    GLuint effectTex =
        (img == colorFBO.nvgImg && colorFBO.outTex != 0) ? colorFBO.outTex
                                                         : srcTex;

    nvgSave(args.vg);
    nvgScissor(args.vg, vpX, vpY, vpW, vpH);
    nvgTranslate(args.vg, vpX + imgHW, vpY + hh);
    if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

    bool tileOn = module->tileEmptySpace;
    int kaliMode = kaliOn ? (int)kaliV : 0;

    float absSx = std::max(fabsf(sx), 0.01f);
    float absSy = std::max(fabsf(sy), 0.01f);
    float renderScale =
        std::max(APP->window->windowRatio * getAbsoluteZoom(), 1.f);

    if (kaliMode > 0 && module->kaleidStyle == 1) {
      float kaliTxOff = 0.f, kaliTyOff = 0.f;
      float nvgTx = 0.f, nvgTy = 0.f;
      if (txOn || tyOn) {
        if (module->translateFirst) {
          kaliTxOff = txLocal;
          kaliTyOff = tyLocal;
        } else {
          nvgTx = txLocal / absSx;
          nvgTy = tyLocal / absSy;
          nvgTranslate(args.vg, nvgTx, nvgTy);
        }
      }
      int flowerImg = flowerKaleidFBO.apply(
          args.vg, effectTex, (int)std::lround(imgW * renderScale),
          (int)std::lround(vpH * renderScale), kaliMode, rotOn ? rotV : 0.f,
          kaliTxOff * renderScale, kaliTyOff * renderScale, useInjected);
      if (flowerImg >= 0) {
        if (tileOn) {
          float pcx = -(txOn && !module->translateFirst ? nvgTx : 0.f);
          float pcy = -(tyOn && !module->translateFirst ? nvgTy : 0.f);
          float dispHW = imgHW / absSx;
          float dispHH = hh / absSy;
          int iMin = (int)ceilf((pcx - dispHW - imgHW) / imgW) - 1;
          int iMax = (int)floorf((pcx + dispHW + imgHW) / imgW) + 1;
          int jMin = (int)ceilf((pcy - dispHH - hh) / vpH) - 1;
          int jMax = (int)floorf((pcy + dispHH + hh) / vpH) + 1;
          iMin = std::max(iMin, -20);
          iMax = std::min(iMax, 20);
          jMin = std::max(jMin, -20);
          jMax = std::min(jMax, 20);
          for (int j = jMin; j <= jMax; j++) {
            for (int i = iMin; i <= iMax; i++) {
              float ox = -imgHW + i * imgW;
              float oy = -hh + j * vpH;
              NVGpaint p = nvgImagePattern(args.vg, ox, oy, imgW, vpH, 0.f,
                                           flowerImg, alpha);
              nvgBeginPath(args.vg);
              nvgRect(args.vg, ox, oy, imgW, vpH);
              nvgFillPaint(args.vg, p);
              nvgFill(args.vg);
            }
          }
        } else {
          NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH, 0.f,
                                       flowerImg, alpha);
          nvgBeginPath(args.vg);
          nvgRect(args.vg, -imgHW, -hh, imgW, vpH);
          nvgFillPaint(args.vg, p);
          nvgFill(args.vg);
        }
      }
    } else if (kaliMode > 0) {
      float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
      float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;

      float kaliTxOff = 0.f, kaliTyOff = 0.f;
      float nvgTx = 0.f, nvgTy = 0.f;
      if (txOn || tyOn) {
        if (module->translateFirst) {
          kaliTxOff = txLocal;
          kaliTyOff = tyLocal;
        } else {
          nvgTx = txLocal / absSx;
          nvgTy = tyLocal / absSy;
          nvgTranslate(args.vg, nvgTx, nvgTy);
        }
      }

      float effTxAbs = fabsf(nvgTx);
      float effTyAbs = fabsf(nvgTy);
      float rHW =
          ((imgW + 2.f * effTxAbs) * cosA + (vpH + 2.f * effTyAbs) * sinA) /
              (2.f * absSx) +
          4.f;
      float rHH =
          ((imgW + 2.f * effTxAbs) * sinA + (vpH + 2.f * effTyAbs) * cosA) /
              (2.f * absSy) +
          4.f;
      float pcx = -(txOn && !module->translateFirst ? nvgTx : 0.f);
      float pcy = -(tyOn && !module->translateFirst ? nvgTy : 0.f);
      float dispHW = imgHW / absSx;
      float dispHH = hh / absSy;

      if (tileOn) {
        int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW);
        int iMax = (int)floorf((pcx + rHW + imgHW) / imgW);
        int jMin = (int)ceilf((pcy - rHH - hh) / vpH);
        int jMax = (int)floorf((pcy + rHH + hh) / vpH);
        iMin = std::max(iMin, -20);
        iMax = std::min(iMax, 20);
        jMin = std::max(jMin, -20);
        jMax = std::min(jMax, 20);
        for (int j = jMin; j <= jMax; j++) {
          for (int i = iMin; i <= iMax; i++) {
            nvgSave(args.vg);
            nvgTranslate(args.vg, i * imgW, j * vpH);
            float dX = pcx - dispHW - (float)i * imgW;
            float dY = pcy - dispHH - (float)j * vpH;
            drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH,
                             kaliMode, alpha, rotOn, rotV, false, 0.f, false,
                             dX, dY, 2.f * dispHW, 2.f * dispHH, kaliTxOff,
                             kaliTyOff);
            nvgRestore(args.vg);
          }
        }
      } else {
        drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH, kaliMode,
                         alpha, rotOn, rotV, false, 0.f, false, pcx - dispHW,
                         pcy - dispHH, 2.f * dispHW, 2.f * dispHH, kaliTxOff,
                         kaliTyOff);
      }
    } else {
      float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
      float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
      if (rotOn) applyRotation(args.vg, rotV);
      float normTx = txOn ? txLocal / absSx : 0.f;
      float normTy = tyOn ? tyLocal / absSy : 0.f;
      if (txOn || tyOn) nvgTranslate(args.vg, normTx, normTy);
      float normTxAbs = fabsf(normTx);
      float normTyAbs = fabsf(normTy);
      float rHW =
          ((imgW + 2.f * normTxAbs) * cosA + (vpH + 2.f * normTyAbs) * sinA) /
              (2.f * absSx) +
          4.f;
      float rHH =
          ((imgW + 2.f * normTxAbs) * sinA + (vpH + 2.f * normTyAbs) * cosA) /
              (2.f * absSy) +
          4.f;

      if (tileOn) {
        float pcx = -normTx;
        float pcy = -normTy;
        int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW);
        int iMax = (int)floorf((pcx + rHW + imgHW) / imgW);
        int jMin = (int)ceilf((pcy - rHH - hh) / vpH);
        int jMax = (int)floorf((pcy + rHH + hh) / vpH);
        iMin = std::max(iMin, -20);
        iMax = std::min(iMax, 20);
        jMin = std::max(jMin, -20);
        jMax = std::min(jMax, 20);
        for (int j = jMin; j <= jMax; j++) {
          for (int i = iMin; i <= iMax; i++) {
            float ox = -imgHW + i * imgW;
            float oy = -hh + j * vpH;
            NVGpaint p =
                nvgImagePattern(args.vg, ox, oy, imgW, vpH, 0.f, img, alpha);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, ox, oy, imgW, vpH);
            nvgFillPaint(args.vg, p);
            nvgFill(args.vg);
          }
        }
      } else {
        // Exact rect — no oversized fill so NVG won't tile/smear the edges.
        NVGpaint p =
            nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH, 0.f, img, alpha);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, -imgHW, -hh, imgW, vpH);
        nvgFillPaint(args.vg, p);
        nvgFill(args.vg);
      }
    }

    nvgRestore(args.vg);
  }
};

// ─── Widget ──────────────────────────────────────────────────────────────────

static const float CONTROLS_WIDTH = 11 * RACK_GRID_WIDTH;  // 165 px / 11 HP
static const float DISPLAY_X = CONTROLS_WIDTH - RACK_GRID_WIDTH;  // 10 HP

struct ScaledSvgWidget : widget::Widget {
  std::shared_ptr<window::Svg> svg;
  void draw(const DrawArgs& args) override {
    if (!svg || !svg->handle) return;
    float svgW = svg->handle->width;
    float svgH = svg->handle->height;
    if (svgW <= 0 || svgH <= 0) return;
    nvgSave(args.vg);
    nvgScale(args.vg, box.size.x / svgW, box.size.y / svgH);
    window::svgDraw(args.vg, svg->handle);
    nvgRestore(args.vg);
  }
};

static std::string pickRandomDocImage() {
  std::string dir = asset::plugin(pluginInstance, "doc");
  DIR* dp = opendir(dir.c_str());
  if (!dp) return "";
  std::vector<std::string> paths;
  struct dirent* entry;
  while ((entry = readdir(dp)) != nullptr) {
    std::string name = entry->d_name;
    auto endsWith = [&](const std::string& ext) {
      return name.size() >= ext.size() &&
             name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
    };
    if (endsWith(".png") || endsWith(".PNG") || endsWith(".jpg") ||
        endsWith(".JPG") || endsWith(".jpeg") || endsWith(".JPEG"))
      paths.push_back(dir + "/" + name);
  }
  closedir(dp);
  if (paths.empty()) return "";
  return paths[random::u32() % paths.size()];
}

struct ComputerscarePortaloofWidget : ModuleWidget {
  BGPanel* bgPanel;
  ComputerscareResizeHandle* rightHandle;
  ScreenCapture screenCap;
  ColorTransformFBO colorFBO;
  FlowerKaleidFBO flowerKaleidFBO;
  PortaloofBackdropWidget* backdropWidget = nullptr;
  std::shared_ptr<window::Svg> panelSvg;
  std::shared_ptr<window::Svg> headerSvg;

  // Cache for triggered mode — holds the last rendered frame's params
  bool cachedRowEnabled[10] = {};
  float cachedRowValue[10] = {};
  bool hasValidCache = false;

  // Loaded image injection
  std::string lastImagePath;
  int loadedNvgImg = -1;
  GLuint loadedTexId = 0;
  bool pendingInject = false;

  // Browser preview fake module
  ComputerscarePortaloof* browserModule = nullptr;

  ~ComputerscarePortaloofWidget() {
    if (backdropWidget) {
      if (APP->scene && APP->scene->rack)
        APP->scene->rack->removeChild(backdropWidget);
      delete backdropWidget;
    }
    delete browserModule;
  }

  ComputerscarePortaloofWidget(ComputerscarePortaloof* module) {
    setModule(module);
    float initialWidth = module ? module->width : 20 * RACK_GRID_WIDTH;
    box.size = Vec(initialWidth, RACK_GRID_HEIGHT);

    bgPanel = new BGPanel(nvgRGB(0x14, 0x14, 0x14));
    bgPanel->box.pos = Vec(DISPLAY_X, 0);
    bgPanel->box.size = Vec(box.size.x - DISPLAY_X, box.size.y);
    addChild(bgPanel);

    panelSvg = APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/panels/portaloof-panel.svg"));
    {
      auto* svgBg = new ScaledSvgWidget();
      svgBg->svg = panelSvg;
      svgBg->box.pos = Vec(0, 0);
      svgBg->box.size = Vec(CONTROLS_WIDTH, RACK_GRID_HEIGHT);
      addChild(svgBg);
    }

    // ── Resize handles — added early so they are lower z-order than params
    // ────
    rightHandle = new ComputerscareResizeHandle();
    rightHandle->right = true;
    rightHandle->minWidth = CONTROLS_WIDTH;
    rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
    addChild(rightHandle);

    // ── Global mode controls
    // ────────────────────────────────────────────────── Both jacks share the
    // same Y (aligned). Button is positioned relative to its jack.
    const float HDR_JACK_Y = 44.f;
    const float HDR_BTN_DX = -24.f;  // button x = jack x + this
    const float HDR_BTN_DY = -2.f;   // button y = jack y + this

    const float CONT_JACK_X = 68.f;
    const float TRIG_JACK_X = 122.f;

    auto addHdrLabel = [&](float x, const char* text) {
      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos = Vec(x, HDR_JACK_Y - 8.f);  // above jack
      lbl->box.size = Vec(50.f, 14.f);
      lbl->fontSize = 12;
      lbl->value = text;
      lbl->textColor = nvgRGBf(0.1f, 0.1f, 0.1f);
      lbl->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 50.f;
      addChild(lbl);
    };

    addHdrLabel(CONT_JACK_X + HDR_BTN_DX, "CONT");
    addParam(createParam<SmallIsoButton>(
        Vec(CONT_JACK_X + HDR_BTN_DX, HDR_JACK_Y + HDR_BTN_DY), module,
        ComputerscarePortaloof::CONTINUOUS_TOGGLE));
    addInput(
        createInput<InPort>(Vec(CONT_JACK_X, HDR_JACK_Y), module,
                            ComputerscarePortaloof::CONTINUOUS_GATE_INPUT));

    addHdrLabel(TRIG_JACK_X + HDR_BTN_DX, "TRIG");
    addParam(createParam<PortaloofTrigButton>(
        Vec(TRIG_JACK_X + HDR_BTN_DX, HDR_JACK_Y + HDR_BTN_DY), module,
        ComputerscarePortaloof::TRIGGER_BUTTON));
    addInput(createInput<InPort>(Vec(TRIG_JACK_X, HDR_JACK_Y), module,
                                 ComputerscarePortaloof::TRIGGER_INPUT));

    // ── 10 effect rows (7 geometry + 3 color) ────────────────────────────────
    // Shifted down 16px from original to accommodate header controls.
    const int N = 10;
    float rowY[N] = {90.f,  123.f, 156.f, 189.f, 222.f,
                     255.f, 288.f, 321.f, 354.f, 354.f};
    const char* rowLabels[N] = {"SCALE", "SCL X", "SCL Y", "ROT",  "KALI",
                                "TRN X", "TRN Y", "HUE",   "FOLD", "WARP"};
    int toggleIds[N] = {
        ComputerscarePortaloof::SCALE_TOGGLE,
        ComputerscarePortaloof::SCALE_X_TOGGLE,
        ComputerscarePortaloof::SCALE_Y_TOGGLE,
        ComputerscarePortaloof::ROT_TOGGLE,
        ComputerscarePortaloof::KALEIDO_TOGGLE,
        ComputerscarePortaloof::TRANS_X_TOGGLE,
        ComputerscarePortaloof::TRANS_Y_TOGGLE,
        ComputerscarePortaloof::HUE_TOGGLE,
        ComputerscarePortaloof::INVERT_TOGGLE,
        ComputerscarePortaloof::CURVES_TOGGLE,
    };
    int knobIds[N] = {
        ComputerscarePortaloof::SCALE_KNOB,
        ComputerscarePortaloof::SCALE_X_KNOB,
        ComputerscarePortaloof::SCALE_Y_KNOB,
        ComputerscarePortaloof::ROT_KNOB,
        ComputerscarePortaloof::KALEIDO_KNOB,
        ComputerscarePortaloof::TRANS_X_KNOB,
        ComputerscarePortaloof::TRANS_Y_KNOB,
        ComputerscarePortaloof::HUE_KNOB,
        ComputerscarePortaloof::INVERT_KNOB,
        ComputerscarePortaloof::CURVES_KNOB,
    };
    int attenIds[N] = {
        ComputerscarePortaloof::SCALE_ATTEN,
        ComputerscarePortaloof::SCALE_X_ATTEN,
        ComputerscarePortaloof::SCALE_Y_ATTEN,
        ComputerscarePortaloof::ROT_ATTEN,
        ComputerscarePortaloof::KALEIDO_ATTEN,
        ComputerscarePortaloof::TRANS_X_ATTEN,
        ComputerscarePortaloof::TRANS_Y_ATTEN,
        ComputerscarePortaloof::HUE_ATTEN,
        ComputerscarePortaloof::INVERT_ATTEN,
        ComputerscarePortaloof::CURVES_ATTEN,
    };
    int gateInputIds[N] = {
        ComputerscarePortaloof::SCALE_GATE_INPUT,
        ComputerscarePortaloof::SCALE_X_GATE_INPUT,
        ComputerscarePortaloof::SCALE_Y_GATE_INPUT,
        ComputerscarePortaloof::ROT_GATE_INPUT,
        ComputerscarePortaloof::KALEIDO_GATE_INPUT,
        ComputerscarePortaloof::TRANS_X_GATE_INPUT,
        ComputerscarePortaloof::TRANS_Y_GATE_INPUT,
        ComputerscarePortaloof::HUE_GATE_INPUT,
        ComputerscarePortaloof::INVERT_GATE_INPUT,
        ComputerscarePortaloof::CURVES_GATE_INPUT,
    };
    int cvInputIds[N] = {
        ComputerscarePortaloof::SCALE_CV_INPUT,
        ComputerscarePortaloof::SCALE_X_CV_INPUT,
        ComputerscarePortaloof::SCALE_Y_CV_INPUT,
        ComputerscarePortaloof::ROT_CV_INPUT,
        ComputerscarePortaloof::KALEIDO_CV_INPUT,
        ComputerscarePortaloof::TRANS_X_CV_INPUT,
        ComputerscarePortaloof::TRANS_Y_CV_INPUT,
        ComputerscarePortaloof::HUE_CV_INPUT,
        ComputerscarePortaloof::INVERT_CV_INPUT,
        ComputerscarePortaloof::CURVES_CV_INPUT,
    };

    for (int i = 0; i < N; i++) {
      if (i == 8) continue;  // FOLD row moved to right-click menu

      float y = rowY[i];

      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos = Vec(0.f, y - 22.f);
      lbl->box.size = Vec(165.f, 14.f);
      lbl->fontSize = 12;
      lbl->value = rowLabels[i];
      lbl->textColor = nvgRGBf(0.1f, 0.1f, 0.1f);
      lbl->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 165.f;
      addChild(lbl);

      addParam(createParam<SmallIsoButton>(Vec(2.f, y - 14.f), module,
                                           toggleIds[i]));
      addInput(
          createInput<OutPort>(Vec(30.f, y - 17.f), module, gateInputIds[i]));
      addInput(createInput<InPort>(Vec(68.f, y - 14.f), module, cvInputIds[i]));
      addParam(
          createParam<SmallKnob>(Vec(100.f, y - 9.f), module, attenIds[i]));
      addParam(
          createParam<SmoothKnob>(Vec(122.f, y - 13.f), module, knobIds[i]));
    }

    headerSvg = APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/panels/portaloof-header.svg"));
  }

  void drawBrowserPreview(const DrawArgs& args) {
    // Create the fake module once with random params + a random doc image
    if (!browserModule) {
      browserModule = new ComputerscarePortaloof();
      browserModule->loadedImagePath = pickRandomDocImage();

      // kali always on with mode >= 2
      browserModule->rowEnabled[4] = true;
      browserModule->rowValue[4] = 2.f + (int)(random::uniform() * 3);

      // rotation always on with a meaningful angle
      browserModule->rowEnabled[3] = true;
      browserModule->rowValue[3] = 0.1f + random::uniform() * 0.9f;

      // hue or warp always on (pick one or both)
      browserModule->rowEnabled[7] = random::uniform() > 0.3f;  // hue
      browserModule->rowValue[7] = random::uniform();
      browserModule->rowEnabled[9] = random::uniform() > 0.3f;  // warp/curves
      browserModule->rowValue[9] = random::uniform();

      // other rows: random on/off and values
      for (int i = 0; i < 10; i++) {
        if (i == 3 || i == 4 || i == 7 || i == 9) continue;
        browserModule->rowEnabled[i] = random::uniform() > 0.5f;
        browserModule->rowValue[i] = random::uniform();
      }

      browserModule->continuousMode = false;
      browserModule->maintainAspect = true;
      browserModule->tileEmptySpace = true;
    }

    // Swap in the fake module, run the normal drawLayer pipeline, swap back
    module = browserModule;
    drawLayer(args, 1);
    module = nullptr;
  }

  void draw(const DrawArgs& args) override {
    if (!module) {
      // Narrow bgPanel to controls only so the display area stays transparent,
      // letting the kaleidoscope show through when we redraw the panel on top.
      bgPanel->box.size.x = 0;
      drawBrowserPreview(args);  // kaleidoscope goes down first
      ModuleWidget::draw(args);  // controls + narrow bg drawn on top
    } else {
      ModuleWidget::draw(args);
    }
  }

  void drawLayer(const DrawArgs& args, int layer) override {
    ComputerscarePortaloof* m = dynamic_cast<ComputerscarePortaloof*>(module);
    bool bgActive = backdropWidget != nullptr;
    bool renderInWindow = !bgActive || (m && !m->emptyWindowInBgMode);
    if (layer == 1 && module && APP->scene && APP->scene->rack &&
        renderInWindow) {
      // Load/reload image if the module's path changed
      if (m->loadedImagePath != lastImagePath) {
        if (loadedNvgImg >= 0) {
          nvgDeleteImage(args.vg, loadedNvgImg);
          loadedNvgImg = -1;
          loadedTexId = 0;
        }
        lastImagePath = m->loadedImagePath;
        if (!lastImagePath.empty()) {
          loadedNvgImg = nvgCreateImage(args.vg, lastImagePath.c_str(), 0);
          if (loadedNvgImg >= 0) {
            loadedTexId = nvglImageHandleGL2(args.vg, loadedNvgImg);
            pendingInject = true;
          }
        }
      }

      // In triggered mode, only re-capture when a trigger fired; otherwise hold
      // last image
      bool trigFired = m->triggerFired.exchange(false);
      bool gateActive = m->imageGateActive.load();
      bool doCapture = m->continuousMode || trigFired;

      // Continuous mode + gate high + loaded image → keep using the image as
      // source
      if (gateActive && loadedNvgImg >= 0 && loadedTexId != 0)
        pendingInject = true;

      bool useInjected = pendingInject && loadedNvgImg >= 0 && loadedTexId != 0;

      if (m && (doCapture || useInjected || hasValidCache)) {
        float alpha = 0.85f;

        // Choose live params on capture, or based on transform pre/post setting
        bool useLive = doCapture || m->continuousMode || m->transformPost;
        bool* en =
            (useLive || !hasValidCache) ? m->rowEnabled : cachedRowEnabled;
        float* rv = (useLive || !hasValidCache) ? m->rowValue : cachedRowValue;

        bool scaleOn = en[0];
        bool scaleXOn = en[1];
        bool scaleYOn = en[2];
        bool rotOn = en[3];
        bool kaliOn = en[4];
        bool txOn = en[5];
        bool tyOn = en[6];
        bool hueOn = en[7];
        bool invertOn = en[8];
        bool curvesOn = en[9];

        float scaleV = rv[0];
        float scaleXV = rv[1];
        float scaleYV = rv[2];
        float rotV = rv[3];
        float kaliV = rv[4];
        float txV = rv[5];
        float tyV = rv[6];
        float hueV = hueOn ? rv[7] : 0.f;
        // Fold Freq (0..1) maps to foldFreq (1..4); 1.0 = no chromatic
        // divergence
        float foldFreqV = invertOn ? (1.0f + rv[8] * 3.0f) : 1.0f;
        float warpV = curvesOn ? rv[9] : 0.f;

        const float displayX = DISPLAY_X;
        float mirrorW = box.size.x - displayX;
        float mirrorH = box.size.y;

        if (mirrorW <= 2.f) {
          ModuleWidget::drawLayer(args, layer);
          return;
        }

        if (doCapture) {
          if (useInjected) {
            if (!gateActive) pendingInject = false;
            // Use loaded image — skip screen capture this frame
          } else {
            screenCap.capture(args.vg);
          }
          // Update cache with current params
          memcpy(cachedRowEnabled, m->rowEnabled, sizeof(cachedRowEnabled));
          memcpy(cachedRowValue, m->rowValue, sizeof(cachedRowValue));
          hasValidCache = true;
        }

        GLuint srcTex = useInjected ? loadedTexId : screenCap.texId;
        int srcImg = useInjected ? loadedNvgImg : screenCap.nvgImg;

        if (srcImg < 0) {
          ModuleWidget::drawLayer(args, layer);
          return;
        }

        int fbW, fbH;
        glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

        float hw =
            mirrorW * 0.5f;  // display half-width (for scissor/translate)
        float hh = mirrorH * 0.5f;

        // In aspect-ratio mode, size the image to the framebuffer's natural
        // aspect. The scissor clips pillarboxes; scaleX/Y are independent NVG
        // transforms.
        float imgW =
            m->maintainAspect ? (mirrorH * (float)fbW / (float)fbH) : mirrorW;
        float imgHW = imgW * 0.5f;

        float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
        float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
        // Translation is relative to one image width/height
        float txLocal = txOn ? txV * imgW : 0.f;
        float tyLocal = tyOn ? tyV * mirrorH : 0.f;

        // Grey stage background — drawn in raw display coords, isolated from
        // any scale/rotation transforms applied to the image rendering below.
        {
          nvgSave(args.vg);
          nvgBeginPath(args.vg);
          nvgRect(args.vg, displayX, 0.f, mirrorW, box.size.y);
          nvgFillColor(args.vg, nvgRGB(0x23, 0x21, 0x29));
          nvgFill(args.vg);
          nvgRestore(args.vg);
        }

        nvgSave(args.vg);

        nvgScissor(args.vg, displayX, 0.f, mirrorW, box.size.y);
        nvgTranslate(args.vg, displayX + hw,
                     hh);  // center of display area

        if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

        bool tileOn = m->tileEmptySpace;
        int img = colorFBO.apply(args.vg, srcTex, srcImg, fbW, fbH, hueV, warpV,
                                 foldFreqV, useInjected);
        GLuint effectTex =
            (img == colorFBO.nvgImg && colorFBO.outTex != 0) ? colorFBO.outTex
                                                             : srcTex;

        int kaliMode = kaliOn ? (int)kaliV : 0;

        float absSx = std::max(fabsf(sx), 0.01f);
        float absSy = std::max(fabsf(sy), 0.01f);
        float renderScale =
            std::max(APP->window->windowRatio * getAbsoluteZoom(), 1.f);

        if (kaliMode > 0 && m->kaleidStyle == 1) {
          float kaliTxOff = 0.f, kaliTyOff = 0.f;
          float nvgTx = 0.f, nvgTy = 0.f;
          if (txOn || tyOn) {
            if (m->translateFirst) {
              kaliTxOff = txLocal;
              kaliTyOff = tyLocal;
            } else {
              nvgTx = txLocal / absSx;
              nvgTy = tyLocal / absSy;
              nvgTranslate(args.vg, nvgTx, nvgTy);
            }
          }
          int flowerImg = flowerKaleidFBO.apply(
              args.vg, effectTex, (int)std::lround(imgW * renderScale),
              (int)std::lround(mirrorH * renderScale), kaliMode,
              rotOn ? rotV : 0.f, kaliTxOff * renderScale,
              kaliTyOff * renderScale, useInjected);
          if (flowerImg >= 0) {
            if (tileOn) {
              float pcx = -(txOn && !m->translateFirst ? nvgTx : 0.f);
              float pcy = -(tyOn && !m->translateFirst ? nvgTy : 0.f);
              float dispHW = hw / absSx;
              float dispHH = hh / absSy;
              int iMin = (int)ceilf((pcx - dispHW - imgHW) / imgW) - 1;
              int iMax = (int)floorf((pcx + dispHW + imgHW) / imgW) + 1;
              int jMin = (int)ceilf((pcy - dispHH - hh) / mirrorH) - 1;
              int jMax = (int)floorf((pcy + dispHH + hh) / mirrorH) + 1;
              iMin = std::max(iMin, -20);
              iMax = std::min(iMax, 20);
              jMin = std::max(jMin, -20);
              jMax = std::min(jMax, 20);
              for (int j = jMin; j <= jMax; j++) {
                for (int i = iMin; i <= iMax; i++) {
                  float ox = -imgHW + i * imgW;
                  float oy = -hh + j * mirrorH;
                  NVGpaint p = nvgImagePattern(args.vg, ox, oy, imgW, mirrorH,
                                               0.f, flowerImg, alpha);
                  nvgBeginPath(args.vg);
                  nvgRect(args.vg, ox, oy, imgW, mirrorH);
                  nvgFillPaint(args.vg, p);
                  nvgFill(args.vg);
                }
              }
            } else {
              NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, mirrorH,
                                           0.f, flowerImg, alpha);
              nvgBeginPath(args.vg);
              nvgRect(args.vg, -imgHW, -hh, imgW, mirrorH);
              nvgFillPaint(args.vg, p);
              nvgFill(args.vg);
            }
          }
        } else if (kaliMode > 0) {
          // No global rotation here — each sector applies its own rotation with
          // sign correction for flipped sectors (see kSector in
          // MirrorKaleidoscope.hpp).
          float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;

          // Algorithm mode determines whether translate shifts the kali center
          // ("Kaleid > Translate", the default) or scrolls the image content
          // through the fixed kali geometry ("Translate > Kaleid").
          float kaliTxOff = 0.f, kaliTyOff = 0.f;
          float nvgTx = 0.f, nvgTy = 0.f;
          if (txOn || tyOn) {
            if (m->translateFirst) {
              kaliTxOff = txLocal;
              kaliTyOff = tyLocal;
            } else {
              nvgTx = txLocal / absSx;
              nvgTy = tyLocal / absSy;
              nvgTranslate(args.vg, nvgTx, nvgTy);
            }
          }

          float effTxAbs = fabsf(nvgTx);
          float effTyAbs = fabsf(nvgTy);
          float rHW = ((imgW + 2.f * effTxAbs) * cosA +
                       (box.size.y + 2.f * effTyAbs) * sinA) /
                          (2.f * absSx) +
                      4.f;
          float rHH = ((imgW + 2.f * effTxAbs) * sinA +
                       (box.size.y + 2.f * effTyAbs) * cosA) /
                          (2.f * absSy) +
                      4.f;
          float pcx = -(txOn && !m->translateFirst ? nvgTx : 0.f);
          float pcy = -(tyOn && !m->translateFirst ? nvgTy : 0.f);
          float dispHW = hw / absSx;
          float dispHH = hh / absSy;

          if (tileOn) {
            int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW) - 1;
            int iMax = (int)floorf((pcx + rHW + imgHW) / imgW) + 1;
            int jMin = (int)ceilf((pcy - rHH - hh) / mirrorH) - 1;
            int jMax = (int)floorf((pcy + rHH + hh) / mirrorH) + 1;
            iMin = std::max(iMin, -20);
            iMax = std::min(iMax, 20);
            jMin = std::max(jMin, -20);
            jMax = std::min(jMax, 20);
            for (int j = jMin; j <= jMax; j++) {
              for (int i = iMin; i <= iMax; i++) {
                nvgSave(args.vg);
                nvgTranslate(args.vg, i * imgW, j * mirrorH);
                float dX = pcx - dispHW - (float)i * imgW;
                float dY = pcy - dispHH - (float)j * mirrorH;
                drawKaleidoscope(args.vg, img, imgHW, hh, imgW, mirrorH, rHW,
                                 rHH, kaliMode, alpha, rotOn, rotV, false, 0.f,
                                 false, dX, dY, 2.f * dispHW, 2.f * dispHH,
                                 kaliTxOff, kaliTyOff);
                nvgRestore(args.vg);
              }
            }
          } else {
            drawKaleidoscope(args.vg, img, imgHW, hh, imgW, mirrorH, rHW, rHH,
                             kaliMode, alpha, rotOn, rotV, false, 0.f, false,
                             pcx - dispHW, pcy - dispHH, 2.f * dispHW,
                             2.f * dispHH, kaliTxOff, kaliTyOff);

            // Overlay opaque grey strips covering display area outside the
            // image bounds. Drawing grey ON TOP means its soft NVG edges blend
            // grey-into-grey (invisible) rather than grey-into-image (smear).
            // A 2px overlap into the image eats any residual tiling bleed at
            // the image boundary.
            if (dispHW > imgHW || dispHH > hh) {
              static const float OVR = 2.f;
              nvgFillColor(args.vg, nvgRGB(0x23, 0x21, 0x29));
              float imgL = pcx - imgHW, imgR = pcx + imgHW;
              float imgT = pcy - hh, imgB = pcy + hh;
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
              fill(dspL, dspT, imgL - dspL + OVR, dspB - dspT);
              fill(imgR - OVR, dspT, dspR - imgR + OVR, dspB - dspT);
              // top/bottom strips (span only the non-left/right region)
              fill(imgL + OVR, dspT, imgR - imgL - 2.f * OVR,
                   imgT - dspT + OVR);
              fill(imgL + OVR, imgB - OVR, imgR - imgL - 2.f * OVR,
                   dspB - imgB + OVR);
            }
          }
        } else {
          // No kaleidoscope — apply rotation then a scale-normalized translate
          // so 10V always pans by imgW screen pixels regardless of scale.
          float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
          float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
          if (rotOn) applyRotation(args.vg, rotV);
          float normTx = txOn ? txLocal / absSx : 0.f;
          float normTy = tyOn ? tyLocal / absSy : 0.f;
          if (txOn || tyOn) nvgTranslate(args.vg, normTx, normTy);
          float normTxAbs = fabsf(normTx);
          float normTyAbs = fabsf(normTy);
          float rHW = ((imgW + 2.f * normTxAbs) * cosA +
                       (box.size.y + 2.f * normTyAbs) * sinA) /
                          (2.f * absSx) +
                      4.f;
          float rHH = ((imgW + 2.f * normTxAbs) * sinA +
                       (box.size.y + 2.f * normTyAbs) * cosA) /
                          (2.f * absSy) +
                      4.f;

          if (tileOn) {
            float pcx = -normTx;
            float pcy = -normTy;
            int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW) - 1;
            int iMax = (int)floorf((pcx + rHW + imgHW) / imgW) + 1;
            int jMin = (int)ceilf((pcy - rHH - hh) / mirrorH) - 1;
            int jMax = (int)floorf((pcy + rHH + hh) / mirrorH) + 1;
            iMin = std::max(iMin, -20);
            iMax = std::min(iMax, 20);
            jMin = std::max(jMin, -20);
            jMax = std::min(jMax, 20);
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

        // Redraw SVG panel clipped to the overlap strip
        // (DISPLAY_X–CONTROLS_WIDTH) so the display peeks under the panel
        // without covering the controls.
        if (panelSvg && panelSvg->handle) {
          float svgW = panelSvg->handle->width;
          float svgH = panelSvg->handle->height;
          if (svgW > 0.f && svgH > 0.f) {
            nvgSave(args.vg);
            nvgScissor(args.vg, DISPLAY_X - 1.f, 0.f,
                       CONTROLS_WIDTH - DISPLAY_X + 1.f, RACK_GRID_HEIGHT);
            nvgGlobalAlpha(args.vg, rack::settings::rackBrightness);
            nvgScale(args.vg, CONTROLS_WIDTH / svgW, RACK_GRID_HEIGHT / svgH);
            window::svgDraw(args.vg, panelSvg->handle);
            nvgRestore(args.vg);
          }
        }
      }
    }

    ModuleWidget::drawLayer(args, layer);

    // Header drawn last — on top of everything including kaleidoscope
    if (layer == 1 && headerSvg && headerSvg->handle) {
      float svgW = headerSvg->handle->width;
      float svgH = headerSvg->handle->height;
      const float drawW = 170.f;
      const float drawH = drawW * (svgH / svgW);
      nvgSave(args.vg);
      nvgTranslate(args.vg, 3.f, 3.f);
      nvgScale(args.vg, drawW / svgW, drawH / svgH);
      window::svgDraw(args.vg, headerSvg->handle);
      nvgRestore(args.vg);
    }
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscarePortaloof* m = dynamic_cast<ComputerscarePortaloof*>(module);
    if (!m) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuItem("Load image...", "", [=]() {
      char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
      if (pathC) {
        m->loadedImagePath = pathC;
        std::free(pathC);
      }
    }));
    if (!m->loadedImagePath.empty()) {
      menu->addChild(createMenuItem("Clear image", "",
                                    [=]() { m->loadedImagePath.clear(); }));
    }
    menu->addChild(new MenuSeparator());

    // Fold Frequency slider (replaces panel FOLD controls)
    struct WideParamSlider : MenuEntry {
      WideParamSlider(ParamQuantity* q) {
        box.size.x = 280.f;
        box.size.y = 32.f;
        auto* lbl = new widget::Widget;
        lbl->box.size = Vec(0.f, 0.f);
        // Use a label via the slider's quantity name
        auto* s = new ui::Slider;
        s->box.size.x = 260.f;
        s->quantity = q;
        s->box.pos = Vec(6.f, 0.f);
        addChild(s);
      }
    };
    menu->addChild(createMenuLabel("Fold Frequency"));
    menu->addChild(new WideParamSlider(
        m->paramQuantities[ComputerscarePortaloof::INVERT_KNOB]));

    menu->addChild(new MenuSeparator());
    menu->addChild(createBoolPtrMenuItem("Render as rack background", "",
                                         &m->backdropEnabled));
    menu->addChild(
        createBoolPtrMenuItem("Transform post", "", &m->transformPost));
    menu->addChild(createSubmenuItem("Full Rack BG", "", [=](Menu* menu) {
      menu->addChild(createBoolPtrMenuItem("Empty module window", "",
                                           &m->emptyWindowInBgMode));
      struct WideSlider : MenuEntry {
        WideSlider(ParamQuantity* q) {
          box.size.x = 280.f;
          box.size.y = 32.f;
          auto* s = new ui::Slider;
          s->box.size.x = 260.f;
          s->quantity = q;
          s->box.pos = Vec(6.f, 0.f);
          addChild(s);
        }
      };
      menu->addChild(new WideSlider(
          m->paramQuantities[ComputerscarePortaloof::BACKDROP_ALPHA]));
    }));
    menu->addChild(createSubmenuItem("Visual", "", [=](Menu* menu) {
      menu->addChild(
          createBoolPtrMenuItem("Tile empty space", "", &m->tileEmptySpace));
      menu->addChild(createBoolPtrMenuItem("Maintain aspect ratio", "",
                                           &m->maintainAspect));
    }));
    menu->addChild(createSubmenuItem("Algorithm", "", [=](Menu* menu) {
      menu->addChild(createCheckMenuItem(
          "Kaleid > Translate", "", [=]() { return !m->translateFirst; },
          [=]() { m->translateFirst = false; }));
      menu->addChild(createCheckMenuItem(
          "Translate > Kaleid", "", [=]() { return m->translateFirst; },
          [=]() { m->translateFirst = true; }));
    }));
    menu->addChild(createSubmenuItem("Kaleid style", "", [=](Menu* menu) {
      menu->addChild(createCheckMenuItem(
          "Classic", "", [=]() { return m->kaleidStyle == 0; },
          [=]() { m->kaleidStyle = 0; }));
      menu->addChild(createCheckMenuItem(
          "Premium", "", [=]() { return m->kaleidStyle == 1; },
          [=]() { m->kaleidStyle = 1; }));
    }));
  }

  void onHover(const HoverEvent& e) override {
    ModuleWidget::onHover(e);
    float hw = RACK_GRID_WIDTH;
    bool overRightHandle = (e.pos.x >= box.size.x - hw);
    static GLFWcursor* resizeCursor = nullptr;
    if (!resizeCursor)
      resizeCursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    glfwSetCursor(APP->window->win, overRightHandle ? resizeCursor : nullptr);
  }

  void onLeave(const LeaveEvent& e) override {
    ModuleWidget::onLeave(e);
    glfwSetCursor(APP->window->win, nullptr);
  }

  void onPathDrop(const PathDropEvent& e) override {
    if (!module || e.paths.empty()) return;
    std::string path = e.paths[0];
    std::string ext = system::getExtension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
      auto* m = dynamic_cast<ComputerscarePortaloof*>(module);
      if (m) m->loadedImagePath = path;
      e.consume(this);
    }
  }

  void step() override {
    if (module) {
      ComputerscarePortaloof* m = dynamic_cast<ComputerscarePortaloof*>(module);

      // Sync backdrop widget with module flag (handles reset / patch load)
      if (m->backdropEnabled && !backdropWidget) {
        auto* w = new PortaloofBackdropWidget();
        w->module = m;
        backdropWidget = w;
        if (APP->scene && APP->scene->rack) {
          auto& rc = APP->scene->rack->children;
          if (!rc.empty())
            APP->scene->rack->addChildAbove(w, rc.front());
          else
            APP->scene->rack->addChild(w);
        }
      } else if (!m->backdropEnabled && backdropWidget) {
        if (APP->scene && APP->scene->rack)
          APP->scene->rack->removeChild(backdropWidget);
        delete backdropWidget;
        backdropWidget = nullptr;
      }

      bool windowEmpty = backdropWidget && m->emptyWindowInBgMode;

      if (!m->loadedJSON) {
        box.size.x = m->width;
        if (!windowEmpty)
          bgPanel->box.size.x = m->width - DISPLAY_X;
        else
          bgPanel->box.size.x = 0;
        rightHandle->box.pos.x = m->width - rightHandle->box.size.x;
        m->loadedJSON = true;
      } else if (box.size.x != m->width) {
        m->width = box.size.x;
        if (!windowEmpty) bgPanel->box.size.x = box.size.x - DISPLAY_X;
        rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
      } else {
        // Keep panel width in sync when emptyWindowInBgMode toggles
        float desiredPanelW = windowEmpty ? 0 : (box.size.x - DISPLAY_X);
        if (bgPanel->box.size.x != desiredPanelW)
          bgPanel->box.size.x = desiredPanelW;
      }
    }
    ModuleWidget::step();
  }
};

Model* modelComputerscarePortaloof =
    createModel<ComputerscarePortaloof, ComputerscarePortaloofWidget>(
        "computerscare-portaloof");
