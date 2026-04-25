#pragma once

#include <dirent.h>
#include <osdialog.h>

#include <atomic>

#include "../Computerscare.hpp"
#include "../ComputerscareResizableHandle.hpp"
#include "ClassicKaleid.hpp"
#include "ColorTransformFBO.hpp"
#include "FlowerKaleid.hpp"
#include "MirrorKaleidoscope.hpp"
#include "MirrorRotation.hpp"
#include "RackModuleSource.hpp"
#include "ScreenCaptureEffect.hpp"
#include "SourceBlendFBO.hpp"

static inline bool reverseOuterTile(int tileIndex) {
  return (tileIndex & 1) != 0;
}

static inline float outerTileDisplayMin(float panelCenter, float halfExtent,
                                        float tileOffset, bool reversed) {
  return reversed ? (tileOffset - panelCenter - halfExtent)
                  : (panelCenter - halfExtent - tileOffset);
}

static inline int flowerKaleidTargetDim(float panelDim, float renderScale,
                                        int sourceDim) {
  static constexpr float FLOWER_SSAA = 2.f;
  static constexpr int FLOWER_MAX_DIM = 4096;
  float baseScale = std::max(renderScale, 1.f);
  float sourceScale =
      (panelDim > 0.f && sourceDim > 0) ? ((float)sourceDim / panelDim) : 1.f;
  float targetScale = std::max(
      baseScale, std::min(baseScale * FLOWER_SSAA, std::max(sourceScale, 1.f)));
  float targetDim = std::min(panelDim * targetScale, (float)FLOWER_MAX_DIM);
  return std::max((int)std::lround(targetDim), 1);
}

static inline int classicKaleidTargetDim(float panelDim, float renderScale,
                                         int sourceDim) {
  static constexpr float CLASSIC_SSAA = 3.f;
  static constexpr int CLASSIC_MAX_DIM = 4096;
  float baseScale = std::max(renderScale, 1.f);
  float sourceScale =
      (panelDim > 0.f && sourceDim > 0) ? ((float)sourceDim / panelDim) : 1.f;
  float targetScale = std::max(baseScale, std::min(baseScale * CLASSIC_SSAA,
                                                   std::max(sourceScale, 1.f)));
  float targetDim = std::min(panelDim * targetScale, (float)CLASSIC_MAX_DIM);
  return std::max((int)std::lround(targetDim), 1);
}

static inline float wrapToRange(float value, float minValue, float maxValue) {
  float range = maxValue - minValue;
  if (range <= 0.f) return minValue;
  float wrapped = fmodf(value - minValue, range);
  if (wrapped < 0.f) wrapped += range;
  return minValue + wrapped;
}

enum PortaloofRowIndex {
  ROW_SCALE = 0,
  ROW_SCALE_X,
  ROW_SCALE_Y,
  ROW_ROT,
  ROW_KALEIDO,
  ROW_TRANS_X,
  ROW_TRANS_Y,
  ROW_HUE,
  ROW_INVERT,
  ROW_CURVES,
  ROW_COUNT
};

struct PortaloofKaleidParamQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    int mode = (int)std::lround(getValue());
    if (mode == 0) return "Off";
    if (mode > 0) return string::f("Premium %d", mode);
    return string::f("Classic %d", -mode);
  }
};

enum class PortaloofSourceKind {
  NONE = 0,
  IMAGE,
  FULL_RACK,
  MODULE,
  RACK_RECT,
  SCREEN_RECT
};

struct PortaloofSourceConfig {
  PortaloofSourceKind kind = PortaloofSourceKind::NONE;
  std::string imagePath;
  int64_t moduleId = -1;
  math::Rect rect;

  bool hasSource() const {
    switch (kind) {
      case PortaloofSourceKind::IMAGE:
        return !imagePath.empty();
      case PortaloofSourceKind::FULL_RACK:
        return true;
      case PortaloofSourceKind::MODULE:
        return moduleId >= 0;
      case PortaloofSourceKind::RACK_RECT:
      case PortaloofSourceKind::SCREEN_RECT:
        return rect.pos.isFinite() && rect.size.isFinite() &&
               rect.size.x > 1.f && rect.size.y > 1.f;
      default:
        return false;
    }
  }

  void clear() {
    kind = PortaloofSourceKind::NONE;
    imagePath.clear();
    moduleId = -1;
    rect = math::Rect();
  }
};

// ─── Module ──────────────────────────────────────────────────────────────────

struct ComputerscarePortaloof : Module {
  enum ParamId {
    FREEZE_TOGGLE,
    INPUT_SOURCE_MIX,
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
    FREEZE_GATE_INPUT,  // gate: high=freeze, low=continuous
    INPUT_SOURCE_MIX_INPUT,
    NUM_INPUTS
  };

  float width = 20 * RACK_GRID_WIDTH;
  bool loadedJSON = false;
  bool tileEmptySpace = true;
  bool maintainAspect = true;
  bool backdropEnabled = false;
  bool emptyWindowInBgMode = true;
  bool transformPost = false;
  bool translateFirst =
      true;  // false = Kaleid > Translate, true = Translate > Kaleid
  bool hideUi = false;
  bool dimVisualsWithRoom = false;
  PortaloofSourceConfig sources[2];
  std::string loadedImagePath;
  int64_t rackSourceModuleId = -1;
  bool rackRectSourceEnabled = false;
  math::Rect rackRectSource;
  bool screenRectSourceEnabled = false;
  math::Rect screenRectSource;
  int legacyKaleidStyle = -1;
  bool legacyKaleidStylePending = false;

  // Effective row state — computed in process(), read in drawLayer()
  bool freezeMode = false;
  bool lastFreezeMode = false;
  float inputSourceMix = -1.f;
  bool rowEnabled[ROW_COUNT] = {};
  float rowValue[ROW_COUNT] = {1.f, 1.f, 1.f, 0.f, 0.f,
                               0.f, 0.f, 0.f, 0.f, 0.f};
  bool sourceRowEnabled[2][ROW_COUNT] = {};
  float sourceRowValue[2][ROW_COUNT] = {};

  std::atomic<bool> capturePending{false};
  std::atomic<bool> backdropCapturePending{false};

  ComputerscarePortaloof() {
    config(NUM_PARAMS, NUM_INPUTS, 0, 0);
    configSwitch(FREEZE_TOGGLE, 0.f, 1.f, 0.f, "Freeze", {"Off", "On"});
    configParam(INPUT_SOURCE_MIX, -1.f, 1.f, -1.f, "Input Source Mix");

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
    configParam<PortaloofKaleidParamQuantity>(KALEIDO_KNOB, -12.f, 12.f, 0.f,
                                              "Kaleido Mode");
    getParamQuantity(KALEIDO_KNOB)->snapEnabled = true;
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
    configParam(HUE_KNOB, -360.f, 360.f, 0.f, "Hue", "\u00b0");
    configParam(HUE_ATTEN, -1.f, 1.f, 0.f, "Hue CV Atten");
    configSwitch(INVERT_TOGGLE, 0.f, 1.f, 1.f, "Fold", {"Off", "On"});
    configParam(INVERT_KNOB, 0.f, 1.f, 0.2f, "Fold Frequency");
    configParam(INVERT_ATTEN, -1.f, 1.f, 0.f, "Fold CV Atten");
    configSwitch(CURVES_TOGGLE, 0.f, 1.f, 1.f, "Color Warp", {"Off", "On"});
    configParam(CURVES_KNOB, -1.f, 1.f, 0.f, "Color Warp");
    configParam(CURVES_ATTEN, -1.f, 1.f, 0.f, "Color Warp CV Atten");
    configParam(BACKDROP_ALPHA, 0.f, 1.f, 0.85f, "Backdrop Alpha");

    configInput(HUE_GATE_INPUT, "Hue Gate");
    configInput(INVERT_GATE_INPUT, "Invert Gate");
    configInput(CURVES_GATE_INPUT, "Color Warp Gate");
    configInput(HUE_CV_INPUT, "Hue CV");
    configInput(INVERT_CV_INPUT, "Invert CV");
    configInput(CURVES_CV_INPUT, "Color Warp CV");
    configInput(FREEZE_GATE_INPUT, "Freeze (gate)");
    configInput(INPUT_SOURCE_MIX_INPUT, "Input Source Mix CV");

    sources[0].kind = PortaloofSourceKind::FULL_RACK;
    sources[1].kind = PortaloofSourceKind::IMAGE;
  }

  void clearRackSources() {
    rackSourceModuleId = -1;
    rackRectSourceEnabled = false;
    rackRectSource = math::Rect();
    screenRectSourceEnabled = false;
    screenRectSource = math::Rect();
  }

  void setRackSourceModule(int64_t moduleId) {
    clearRackSources();
    rackSourceModuleId = moduleId;
  }

  void setRackRectSource(math::Rect r) {
    clearRackSources();
    rackRectSource = r;
    rackRectSourceEnabled = r.size.x > 1.f && r.size.y > 1.f;
  }

  void setScreenRectSource(math::Rect r) {
    clearRackSources();
    screenRectSource = r;
    screenRectSourceEnabled = r.size.x > 1.f && r.size.y > 1.f;
  }

  void clearSource(int index) {
    if (index < 0 || index >= 2) return;
    sources[index].clear();
  }

  void setSourceImage(int index, const std::string& path) {
    if (index < 0 || index >= 2) return;
    sources[index].clear();
    sources[index].kind = PortaloofSourceKind::IMAGE;
    sources[index].imagePath = path;
  }

  void setSourceFullRack(int index) {
    if (index < 0 || index >= 2) return;
    sources[index].clear();
    sources[index].kind = PortaloofSourceKind::FULL_RACK;
  }

  void setSourceModule(int index, int64_t moduleId) {
    if (index < 0 || index >= 2) return;
    sources[index].clear();
    sources[index].kind = PortaloofSourceKind::MODULE;
    sources[index].moduleId = moduleId;
  }

  void setSourceRackRect(int index, math::Rect r) {
    if (index < 0 || index >= 2) return;
    sources[index].clear();
    sources[index].kind = PortaloofSourceKind::RACK_RECT;
    sources[index].rect = r;
    if (!sources[index].hasSource()) sources[index].clear();
  }

  void setSourceScreenRect(int index, math::Rect r) {
    if (index < 0 || index >= 2) return;
    sources[index].clear();
    sources[index].kind = PortaloofSourceKind::SCREEN_RECT;
    sources[index].rect = r;
    if (!sources[index].hasSource()) sources[index].clear();
  }

  std::string getMixLabel(int index) const {
    if (index < 0 || index >= 2) return "";
    const PortaloofSourceConfig& s = sources[index];
    switch (s.kind) {
      case PortaloofSourceKind::IMAGE:
        return string::f("img%d", index + 1);
      case PortaloofSourceKind::MODULE:
        return string::f("Mod%d", index + 1);
      case PortaloofSourceKind::FULL_RACK:
      case PortaloofSourceKind::RACK_RECT:
      case PortaloofSourceKind::SCREEN_RECT:
        return "Rack";
      default:
        return "None";
    }
  }

  std::string getSourceMenuLabel(int index) const {
    if (index < 0 || index >= 2) return "Source";
    return string::f("Source %d: %s", index + 1, getMixLabel(index).c_str());
  }

  json_t* sourceToJson(const PortaloofSourceConfig& s) {
    json_t* sourceJ = json_object();
    json_object_set_new(sourceJ, "kind", json_integer((int)s.kind));
    if (!s.imagePath.empty())
      json_object_set_new(sourceJ, "imagePath",
                          json_string(s.imagePath.c_str()));
    json_object_set_new(sourceJ, "moduleId", json_integer(s.moduleId));
    json_object_set_new(sourceJ, "rect",
                        json_pack("[f, f, f, f]", s.rect.pos.x, s.rect.pos.y,
                                  s.rect.size.x, s.rect.size.y));
    return sourceJ;
  }

  void sourceFromJson(PortaloofSourceConfig& s, json_t* sourceJ) {
    if (!sourceJ) return;
    s.clear();
    json_t* kindJ = json_object_get(sourceJ, "kind");
    if (kindJ) s.kind = (PortaloofSourceKind)json_integer_value(kindJ);
    json_t* imagePathJ = json_object_get(sourceJ, "imagePath");
    if (imagePathJ) s.imagePath = json_string_value(imagePathJ);
    json_t* moduleIdJ = json_object_get(sourceJ, "moduleId");
    if (moduleIdJ) s.moduleId = json_integer_value(moduleIdJ);
    json_t* rectJ = json_object_get(sourceJ, "rect");
    if (rectJ) {
      double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
      json_unpack(rectJ, "[F, F, F, F]", &x, &y, &w, &h);
      s.rect = math::Rect((float)x, (float)y, (float)w, (float)h);
    }
    if (!s.hasSource()) s.clear();
  }

  void onRandomize(const RandomizeEvent& e) override {
    PortaloofSourceConfig preservedSources[2] = {sources[0], sources[1]};
    int64_t preservedRackSourceModuleId = rackSourceModuleId;
    bool preservedRackRectSourceEnabled = rackRectSourceEnabled;
    math::Rect preservedRackRectSource = rackRectSource;
    bool preservedScreenRectSourceEnabled = screenRectSourceEnabled;
    math::Rect preservedScreenRectSource = screenRectSource;
    for (int id = 0; id < NUM_PARAMS; id++) {
      if (id == FREEZE_TOGGLE) continue;
      if (id == INPUT_SOURCE_MIX) continue;
      if (id == BACKDROP_ALPHA) continue;
      paramQuantities[id]->randomize();
    }
    rackSourceModuleId = preservedRackSourceModuleId;
    rackRectSourceEnabled = preservedRackRectSourceEnabled;
    rackRectSource = preservedRackRectSource;
    screenRectSourceEnabled = preservedScreenRectSourceEnabled;
    screenRectSource = preservedScreenRectSource;
    sources[0] = preservedSources[0];
    sources[1] = preservedSources[1];
  }

  void process(const ProcessArgs& args) override {
    static const int toggleIds[ROW_COUNT] = {
        SCALE_TOGGLE,   SCALE_X_TOGGLE, SCALE_Y_TOGGLE, ROT_TOGGLE,
        KALEIDO_TOGGLE, TRANS_X_TOGGLE, TRANS_Y_TOGGLE, HUE_TOGGLE,
        INVERT_TOGGLE,  CURVES_TOGGLE};
    static const int knobIds[ROW_COUNT] = {
        SCALE_KNOB,   SCALE_X_KNOB, SCALE_Y_KNOB, ROT_KNOB,    KALEIDO_KNOB,
        TRANS_X_KNOB, TRANS_Y_KNOB, HUE_KNOB,     INVERT_KNOB, CURVES_KNOB};
    static const int attenIds[ROW_COUNT] = {
        SCALE_ATTEN,   SCALE_X_ATTEN, SCALE_Y_ATTEN, ROT_ATTEN,
        KALEIDO_ATTEN, TRANS_X_ATTEN, TRANS_Y_ATTEN, HUE_ATTEN,
        INVERT_ATTEN,  CURVES_ATTEN};
    static const int gateIds[ROW_COUNT] = {
        SCALE_GATE_INPUT,   SCALE_X_GATE_INPUT, SCALE_Y_GATE_INPUT,
        ROT_GATE_INPUT,     KALEIDO_GATE_INPUT, TRANS_X_GATE_INPUT,
        TRANS_Y_GATE_INPUT, HUE_GATE_INPUT,     INVERT_GATE_INPUT,
        CURVES_GATE_INPUT};
    static const int cvIds[ROW_COUNT] = {
        SCALE_CV_INPUT,   SCALE_X_CV_INPUT, SCALE_Y_CV_INPUT, ROT_CV_INPUT,
        KALEIDO_CV_INPUT, TRANS_X_CV_INPUT, TRANS_Y_CV_INPUT, HUE_CV_INPUT,
        INVERT_CV_INPUT,  CURVES_CV_INPUT};
    static const float mins[ROW_COUNT] = {0.1f, -5.f, -5.f,   -360.f, -12.f,
                                          -1.f, -1.f, -360.f, 0.f,    -1.f};
    static const float maxs[ROW_COUNT] = {4.f, 5.f, 5.f,   360.f, 12.f,
                                          1.f, 1.f, 360.f, 1.f,   1.f};
    static const float cvScale[ROW_COUNT] = {0.3f, 0.5f, 0.5f, 36.f, 1.1f,
                                             0.1f, 0.1f, 36.f, 0.1f, 0.1f};

    if (legacyKaleidStylePending) {
      float currentMode = params[KALEIDO_KNOB].getValue();
      if (currentMode != 0.f) {
        params[KALEIDO_KNOB].setValue(
            legacyKaleidStyle == 0 ? -fabsf(currentMode) : fabsf(currentMode));
      }
      legacyKaleidStylePending = false;
    }

    // Freeze mode: gate jack overrides button when connected
    bool gateConnected = inputs[FREEZE_GATE_INPUT].isConnected();
    freezeMode = gateConnected ? (inputs[FREEZE_GATE_INPUT].getVoltage() > 0.5f)
                               : (params[FREEZE_TOGGLE].getValue() > 0.5f);
    if (freezeMode && !lastFreezeMode) {
      capturePending.store(true);
      backdropCapturePending.store(true);
    }
    lastFreezeMode = freezeMode;

    float mixCv = inputs[INPUT_SOURCE_MIX_INPUT].isConnected()
                      ? inputs[INPUT_SOURCE_MIX_INPUT].getVoltage() / 5.f
                      : 0.f;
    inputSourceMix =
        clamp(params[INPUT_SOURCE_MIX].getValue() + mixCv, -1.f, 1.f);

    for (int i = 0; i < ROW_COUNT; i++) {
      bool gateConn = inputs[gateIds[i]].isConnected();
      float offset = params[knobIds[i]].getValue();
      float atten = params[attenIds[i]].getValue();

      int gateChannels = inputs[gateIds[i]].getChannels();
      int cvChannels = inputs[cvIds[i]].getChannels();
      bool toggleOn = params[toggleIds[i]].getValue() > 0.5f;
      for (int s = 0; s < 2; s++) {
        int gateChannel = gateChannels >= 2 ? s : 0;
        int cvChannel = cvChannels >= 2 ? s : 0;
        bool gateOn =
            !gateConn || inputs[gateIds[i]].getVoltage(gateChannel) > 0.5f;
        sourceRowEnabled[s][i] = toggleOn && gateOn;

        float cv = inputs[cvIds[i]].isConnected()
                       ? inputs[cvIds[i]].getVoltage(cvChannel)
                       : 0.f;
        float combined = offset + atten * cv * cvScale[i];
        bool wraps = (i == ROW_ROT || i == ROW_TRANS_X || i == ROW_TRANS_Y ||
                      i == ROW_HUE);
        sourceRowValue[s][i] = wraps ? wrapToRange(combined, mins[i], maxs[i])
                                     : clamp(combined, mins[i], maxs[i]);
      }

      rowEnabled[i] = sourceRowEnabled[0][i];
      rowValue[i] = sourceRowValue[0][i];
    }

    // Exponential scale map for Scale X (row 1) and Scale Y (row 2).
    // Maps knob [-5, 5] → [-5, -0.1] | [0.1, 5] with two segments:
    //   [0, 1] → [0.1, 1.0] quadratic (fine control near center)
    //   [1, 5] → [1.0, 5.0] exponential (coarser at extremes)
    // Default knob=1 → scale=1.0 (identity). Negative values mirror the axis.
    for (int si = ROW_SCALE_X; si <= ROW_SCALE_Y; si++) {
      for (int s = 0; s < 2; s++) {
        float k = sourceRowValue[s][si];
        float sign = (k >= 0.f) ? 1.f : -1.f;
        float a = fabsf(k);
        float result;
        if (a <= 1.f) {
          result = 0.1f + 0.9f * (a * a);
        } else {
          result = powf(5.f, (a - 1.f) / 4.f);
        }
        sourceRowValue[s][si] = sign * result;
      }
      rowValue[si] = sourceRowValue[0][si];
    }
  }

  void onReset(const ResetEvent& e) override {
    float freezeVal = params[FREEZE_TOGGLE].getValue();
    float mixVal = params[INPUT_SOURCE_MIX].getValue();
    Module::onReset(e);
    params[FREEZE_TOGGLE].setValue(freezeVal);
    params[INPUT_SOURCE_MIX].setValue(mixVal);
    backdropEnabled = false;
    lastFreezeMode = freezeMode;
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
    json_object_set_new(rootJ, "hideUi", json_boolean(hideUi));
    json_object_set_new(rootJ, "dimVisualsWithRoom",
                        json_boolean(dimVisualsWithRoom));
    json_t* sourcesJ = json_array();
    json_array_append_new(sourcesJ, sourceToJson(sources[0]));
    json_array_append_new(sourcesJ, sourceToJson(sources[1]));
    json_object_set_new(rootJ, "sources", sourcesJ);
    json_object_set_new(rootJ, "rackSourceModuleId",
                        json_integer(rackSourceModuleId));
    json_object_set_new(rootJ, "rackRectSourceEnabled",
                        json_boolean(rackRectSourceEnabled));
    json_object_set_new(
        rootJ, "rackRectSource",
        json_pack("[f, f, f, f]", rackRectSource.pos.x, rackRectSource.pos.y,
                  rackRectSource.size.x, rackRectSource.size.y));
    json_object_set_new(rootJ, "screenRectSourceEnabled",
                        json_boolean(screenRectSourceEnabled));
    json_object_set_new(
        rootJ, "screenRectSource",
        json_pack("[f, f, f, f]", screenRectSource.pos.x,
                  screenRectSource.pos.y, screenRectSource.size.x,
                  screenRectSource.size.y));
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
    json_t* huiJ = json_object_get(rootJ, "hideUi");
    if (huiJ) hideUi = json_boolean_value(huiJ);
    json_t* dimJ = json_object_get(rootJ, "dimVisualsWithRoom");
    if (dimJ) dimVisualsWithRoom = json_boolean_value(dimJ);
    bool loadedNewSources = false;
    json_t* sourcesJ = json_object_get(rootJ, "sources");
    if (sourcesJ && json_is_array(sourcesJ)) {
      sourceFromJson(sources[0], json_array_get(sourcesJ, 0));
      sourceFromJson(sources[1], json_array_get(sourcesJ, 1));
      loadedNewSources = true;
    }
    json_t* rsmJ = json_object_get(rootJ, "rackSourceModuleId");
    if (rsmJ) rackSourceModuleId = json_integer_value(rsmJ);
    json_t* rrseJ = json_object_get(rootJ, "rackRectSourceEnabled");
    if (rrseJ) rackRectSourceEnabled = json_boolean_value(rrseJ);
    json_t* rrsJ = json_object_get(rootJ, "rackRectSource");
    if (rrsJ) {
      double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
      json_unpack(rrsJ, "[F, F, F, F]", &x, &y, &w, &h);
      rackRectSource = math::Rect((float)x, (float)y, (float)w, (float)h);
    }
    json_t* srseJ = json_object_get(rootJ, "screenRectSourceEnabled");
    if (srseJ) screenRectSourceEnabled = json_boolean_value(srseJ);
    json_t* srsJ = json_object_get(rootJ, "screenRectSource");
    if (srsJ) {
      double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
      json_unpack(srsJ, "[F, F, F, F]", &x, &y, &w, &h);
      screenRectSource = math::Rect((float)x, (float)y, (float)w, (float)h);
    }
    json_t* ksJ = json_object_get(rootJ, "kaleidStyle");
    if (ksJ) {
      legacyKaleidStyle = (int)json_integer_value(ksJ);
      legacyKaleidStylePending = true;
    }
    json_t* lipJ = json_object_get(rootJ, "loadedImagePath");
    if (lipJ) loadedImagePath = json_string_value(lipJ);
    if (!loadedNewSources) {
      sources[0].clear();
      sources[0].kind = PortaloofSourceKind::FULL_RACK;
      sources[1].clear();
      if (rackRectSourceEnabled) {
        sources[1].kind = PortaloofSourceKind::RACK_RECT;
        sources[1].rect = rackRectSource;
      } else if (screenRectSourceEnabled) {
        sources[1].kind = PortaloofSourceKind::SCREEN_RECT;
        sources[1].rect = screenRectSource;
      } else if (rackSourceModuleId >= 0) {
        sources[1].kind = PortaloofSourceKind::MODULE;
        sources[1].moduleId = rackSourceModuleId;
      } else {
        sources[1].kind = PortaloofSourceKind::IMAGE;
        sources[1].imagePath = loadedImagePath;
      }
      if (!sources[1].hasSource()) sources[1].kind = PortaloofSourceKind::IMAGE;
    }
    loadedJSON = false;
  }
};
