#include <dirent.h>
#include <osdialog.h>

#include <atomic>

#include "../Computerscare.hpp"
#include "../ComputerscareResizableHandle.hpp"
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

// ─── Backdrop Widget ─────────────────────────────────────────────────────────
// Inserted as the bottom-most child of APP->scene->rack so it draws behind
// all modules.  Uses the same rendering pipeline as the main widget but maps
// the effect across the full visible viewport.

struct PortaloofBackdropWidget : widget::Widget {
  ComputerscarePortaloof* module = nullptr;
  ScreenCapture screenCap;
  ColorTransformFBO colorFBOs[2];
  SourceBlendFBO sourceBlendFBO;
  FlowerKaleidFBO flowerKaleidFBOs[2];
  PortaloofRackModuleSource rackSources[2];
  PortaloofRectSource rectSources[2];
  bool cachedRowEnabled[2][10] = {};
  float cachedRowValue[2][10] = {};
  bool hasValidCache = false;

  // Loaded image injection
  std::string lastImagePath[2];
  int loadedNvgImg[2] = {-1, -1};
  GLuint loadedTexId[2] = {0, 0};

  PortaloofBackdropWidget() {
    box.pos = math::Vec(0.f, 0.f);
    box.size = math::Vec(INFINITY, INFINITY);
  }

  void draw(const DrawArgs& args) override {
    if (!module || portaloofRenderingRackRectSource()) return;

    float vpW = args.clipBox.size.x;
    float vpH = args.clipBox.size.y;
    float vpX = args.clipBox.pos.x;
    float vpY = args.clipBox.pos.y;
    if (vpW <= 1.f || vpH <= 1.f) return;

    int fbW, fbH;
    glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

    bool doCapture = !module->freezeMode ||
                     module->backdropCapturePending.exchange(false) ||
                     !hasValidCache;
    if (doCapture) {
      screenCap.capture(args.vg);
      memcpy(cachedRowEnabled, module->sourceRowEnabled,
             sizeof(cachedRowEnabled));
      memcpy(cachedRowValue, module->sourceRowValue, sizeof(cachedRowValue));
      hasValidCache = true;
    }

    PortaloofInjectedSource renderSources[2] = {
        getSource(args.vg, screenCap, 0), getSource(args.vg, screenCap, 1)};
    bool hasSource1 = renderSources[0].isValid();
    bool hasSource2 = renderSources[1].isValid();
    if (!doCapture && !hasSource1 && !hasSource2) return;
    if (!hasSource1 && !hasSource2) return;
    float source2Amt = 0.5f * (1.f + module->inputSourceMix);

    // Transform post: use live params (knobs alter frozen image).
    // Transform pre:  use cached params (knobs only affect next snapshot).
    bool useLive = !module->freezeMode || module->transformPost;

    float baseAlpha =
        module->params[ComputerscarePortaloof::BACKDROP_ALPHA].getValue();
    float imgW = vpW;
    float imgHW = imgW * 0.5f;
    float hh = vpH * 0.5f;

    for (int renderSourceIndex = 0; renderSourceIndex < 2;
         renderSourceIndex++) {
      PortaloofInjectedSource currentSource = renderSources[renderSourceIndex];
      if (!currentSource.isValid()) continue;
      float sourceMixAlpha = 1.f;
      if (hasSource1 && hasSource2)
        sourceMixAlpha =
            renderSourceIndex == 0 ? (1.f - source2Amt) : source2Amt;
      if (sourceMixAlpha <= 0.f) continue;
      float alpha = baseAlpha * sourceMixAlpha;

      bool* en = (useLive || !hasValidCache)
                     ? module->sourceRowEnabled[renderSourceIndex]
                     : cachedRowEnabled[renderSourceIndex];
      float* rv = (useLive || !hasValidCache)
                      ? module->sourceRowValue[renderSourceIndex]
                      : cachedRowValue[renderSourceIndex];

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

      GLuint srcTex = currentSource.texId;
      int srcImg = currentSource.nvgImg;
      bool flipInputUV = currentSource.flipInputUV;

      float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
      float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
      float txLocal = txOn ? txV * imgW : 0.f;
      float tyLocal = tyOn ? tyV * vpH : 0.f;

      int img = colorFBOs[renderSourceIndex].apply(args.vg, srcTex, srcImg, fbW,
                                                   fbH, hueV, warpV, foldFreqV,
                                                   flipInputUV);
      GLuint effectTex = (img == colorFBOs[renderSourceIndex].nvgImg &&
                          colorFBOs[renderSourceIndex].outTex != 0)
                             ? colorFBOs[renderSourceIndex].outTex
                             : srcTex;

      nvgSave(args.vg);
      nvgScissor(args.vg, vpX, vpY, vpW, vpH);
      nvgTranslate(args.vg, vpX + imgHW, vpY + hh);
      if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

      bool tileOn = module->tileEmptySpace;
      int kaliMode = kaliOn ? (int)kaliV : 0;
      int kaliSegments = std::abs(kaliMode);

      float absSx = std::max(fabsf(sx), 0.01f);
      float absSy = std::max(fabsf(sy), 0.01f);
      float renderScale =
          std::max(APP->window->windowRatio * getAbsoluteZoom(), 1.f);

      if (kaliMode > 0) {
        float kaliTxOff = 0.f, kaliTyOff = 0.f;
        float nvgTx = 0.f, nvgTy = 0.f;
        if (txOn || tyOn) {
          if (module->translateFirst) {
            kaliTxOff = txLocal;
            kaliTyOff = tyLocal;
          } else {
            nvgTx = txOn ? (2.f * txLocal) : 0.f;
            nvgTy = tyOn ? (2.f * tyLocal) : 0.f;
            nvgTranslate(args.vg, nvgTx, nvgTy);
          }
        }
        int flowerTargetW = flowerKaleidTargetDim(imgW, renderScale, fbW);
        int flowerTargetH = flowerKaleidTargetDim(vpH, renderScale, fbH);
        float flowerScaleX = (imgW > 0.f) ? ((float)flowerTargetW / imgW) : 1.f;
        float flowerScaleY = (vpH > 0.f) ? ((float)flowerTargetH / vpH) : 1.f;
        int flowerImg = flowerKaleidFBOs[renderSourceIndex].apply(
            args.vg, effectTex, flowerTargetW, flowerTargetH, kaliSegments,
            rotOn ? rotV : 0.f, kaliTxOff * flowerScaleX,
            kaliTyOff * flowerScaleY, flipInputUV);
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
                float tileX = (float)i * imgW;
                float tileY = (float)j * vpH;
                bool reverseX = reverseOuterTile(i);
                bool reverseY = reverseOuterTile(j);
                nvgSave(args.vg);
                nvgTranslate(args.vg, tileX, tileY);
                if (reverseX || reverseY) {
                  nvgScale(args.vg, reverseX ? -1.f : 1.f,
                           reverseY ? -1.f : 1.f);
                }
                NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH,
                                             0.f, flowerImg, alpha);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, -imgHW, -hh, imgW, vpH);
                nvgFillPaint(args.vg, p);
                nvgFill(args.vg);
                nvgRestore(args.vg);
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
      } else if (kaliMode < 0) {
        float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
        float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;

        float kaliTxOff = 0.f, kaliTyOff = 0.f;
        float nvgTx = 0.f, nvgTy = 0.f;
        if (txOn || tyOn) {
          if (module->translateFirst) {
            kaliTxOff = txLocal;
            kaliTyOff = tyLocal;
          } else {
            nvgTx = txOn ? (2.f * txLocal) : 0.f;
            nvgTy = tyOn ? (2.f * tyLocal) : 0.f;
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
              float tileX = (float)i * imgW;
              float tileY = (float)j * vpH;
              bool reverseX = reverseOuterTile(i);
              bool reverseY = reverseOuterTile(j);
              nvgSave(args.vg);
              nvgTranslate(args.vg, tileX, tileY);
              if (reverseX || reverseY) {
                nvgScale(args.vg, reverseX ? -1.f : 1.f, reverseY ? -1.f : 1.f);
              }
              float dX = outerTileDisplayMin(pcx, dispHW, tileX, reverseX);
              float dY = outerTileDisplayMin(pcy, dispHH, tileY, reverseY);
              drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH,
                               kaliSegments, alpha, rotOn, rotV, false, 0.f,
                               false, dX, dY, 2.f * dispHW, 2.f * dispHH,
                               kaliTxOff, kaliTyOff);
              nvgRestore(args.vg);
            }
          }
        } else {
          drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH,
                           kaliSegments, alpha, rotOn, rotV, false, 0.f, false,
                           pcx - dispHW, pcy - dispHH, 2.f * dispHW,
                           2.f * dispHH, kaliTxOff, kaliTyOff);
        }
      } else {
        float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
        float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
        if (rotOn) applyRotation(args.vg, rotV);
        float txOffset = txOn ? txLocal : 0.f;
        float tyOffset = tyOn ? tyLocal : 0.f;
        if (txOn || tyOn) nvgTranslate(args.vg, txOffset, tyOffset);
        float txOffsetAbs = fabsf(txOffset);
        float tyOffsetAbs = fabsf(tyOffset);
        float rHW = ((imgW + 2.f * txOffsetAbs) * cosA +
                     (vpH + 2.f * tyOffsetAbs) * sinA) /
                        (2.f * absSx) +
                    4.f;
        float rHH = ((imgW + 2.f * txOffsetAbs) * sinA +
                     (vpH + 2.f * tyOffsetAbs) * cosA) /
                        (2.f * absSy) +
                    4.f;

        if (tileOn) {
          float pcx = -txOffset;
          float pcy = -tyOffset;
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
  }

  PortaloofInjectedSource getSource(NVGcontext* vg,
                                    const ScreenCapture& capture, int index) {
    PortaloofInjectedSource out;
    if (!module || index < 0 || index >= 2) return out;
    const PortaloofSourceConfig& source = module->sources[index];
    switch (source.kind) {
      case PortaloofSourceKind::IMAGE:
        if (source.imagePath != lastImagePath[index]) {
          if (loadedNvgImg[index] >= 0) {
            nvgDeleteImage(vg, loadedNvgImg[index]);
            loadedNvgImg[index] = -1;
            loadedTexId[index] = 0;
          }
          lastImagePath[index] = source.imagePath;
          if (!lastImagePath[index].empty()) {
            loadedNvgImg[index] =
                nvgCreateImage(vg, lastImagePath[index].c_str(), 0);
            if (loadedNvgImg[index] >= 0)
              loadedTexId[index] = nvglImageHandleGL2(vg, loadedNvgImg[index]);
          }
        }
        out.nvgImg = loadedNvgImg[index];
        out.texId = loadedTexId[index];
        out.flipInputUV = true;
        return out;
      case PortaloofSourceKind::FULL_RACK:
        out.nvgImg = capture.nvgImg;
        out.texId = capture.texId;
        out.flipInputUV = false;
        return out;
      case PortaloofSourceKind::MODULE:
        rackSources[index].setModule(source.moduleId);
        return rackSources[index].render(vg, module->id);
      case PortaloofSourceKind::RACK_RECT:
        rectSources[index].setRect(source.rect);
        return rectSources[index].renderRack(vg, source.rect);
      case PortaloofSourceKind::SCREEN_RECT:
        rectSources[index].setRect(source.rect);
        return rectSources[index].render(vg, capture.nvgImg, source.rect);
      default:
        return out;
    }
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

struct PortaloofRectSelectionOverlay : widget::OpaqueWidget {
  enum Mode { RACK_RECT, SCREEN_RECT };

  ComputerscarePortaloof* module = nullptr;
  Mode mode = SCREEN_RECT;
  int sourceIndex = 0;
  bool selecting = false;
  math::Vec start;
  math::Vec end;

  PortaloofRectSelectionOverlay() {
    if (APP && APP->scene) box.size = APP->scene->box.size;
  }

  void step() override {
    if (APP && APP->scene) box.size = APP->scene->box.size;
    OpaqueWidget::step();
  }

  void draw(const DrawArgs& args) override {
    if (!selecting) return;
    math::Rect r = math::Rect::fromCorners(start, end);
    if (r.size.x <= 0.f || r.size.y <= 0.f) return;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, RECT_ARGS(r));
    nvgFillColor(args.vg, nvgRGBAf(1.f, 0.f, 0.f, 0.18f));
    nvgFill(args.vg);
    nvgStrokeWidth(args.vg, 2.f);
    nvgStrokeColor(args.vg, nvgRGBAf(1.f, 0.f, 0.f, 0.7f));
    nvgStroke(args.vg);
  }

  void onButton(const ButtonEvent& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      start = e.pos;
      end = e.pos;
      selecting = true;
      e.consume(this);
      return;
    }
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
      requestDelete();
      e.consume(this);
      return;
    }
    OpaqueWidget::onButton(e);
  }

  void onDragMove(const DragMoveEvent& e) override {
    if (selecting) end = end.plus(e.mouseDelta);
  }

  void onDragEnd(const DragEndEvent& e) override {
    if (!selecting || !module) {
      requestDelete();
      return;
    }
    selecting = false;
    math::Rect sceneRect = math::Rect::fromCorners(start, end);
    if (sceneRect.size.x > 2.f && sceneRect.size.y > 2.f) {
      if (mode == RACK_RECT)
        module->setSourceRackRect(sourceIndex, sceneRectToRackRect(sceneRect));
      else
        module->setSourceScreenRect(sourceIndex, sceneRect);
    }
    requestDelete();
  }

  static math::Rect sceneRectToRackRect(math::Rect sceneRect) {
    if (!APP || !APP->scene || !APP->scene->rack) return math::Rect();
    float z = APP->scene->rack->getAbsoluteZoom();
    if (z <= 0.f) return math::Rect();
    math::Vec rackOrigin = APP->scene->rack->getAbsoluteOffset(math::Vec());
    return math::Rect(sceneRect.pos.minus(rackOrigin).div(z),
                      sceneRect.size.div(z));
  }
};

struct ComputerscarePortaloofWidget : ModuleWidget {
  struct HiddenPortPlacement {
    PortWidget* port = nullptr;
    Vec visiblePos;
  };

  BGPanel* bgPanel;
  ComputerscareResizeHandle* rightHandle;
  ScreenCapture screenCap;
  ColorTransformFBO colorFBOs[2];
  SourceBlendFBO sourceBlendFBO;
  FlowerKaleidFBO flowerKaleidFBOs[2];
  PortaloofRackModuleSource rackSources[2];
  PortaloofRectSource rectSources[2];
  PortaloofBackdropWidget* backdropWidget = nullptr;
  std::shared_ptr<window::Svg> panelSvg;
  std::shared_ptr<window::Svg> headerSvg;
  std::vector<HiddenPortPlacement> hiddenPortPlacements;
  bool portsInHiddenLayout = false;
  bool hiddenUiApplied = false;
  bool lastHideUiForSizing = false;

  // Cache for triggered mode — holds the last rendered frame's params
  bool cachedRowEnabled[2][10] = {};
  float cachedRowValue[2][10] = {};
  bool hasValidCache = false;

  // Loaded image injection
  std::string lastImagePath[2];
  int loadedNvgImg[2] = {-1, -1};
  GLuint loadedTexId[2] = {0, 0};
  SmallLetterDisplay* mixLeftLabel = nullptr;
  SmallLetterDisplay* mixRightLabel = nullptr;

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

    const float CONT_JACK_X = 64.f;
    const float MIX_JACK_X = 122.f;

    auto addHdrLabel = [&](float x, const char* text) {
      SmallLetterDisplay* lbl = new SmallLetterDisplay();
      lbl->box.pos = Vec(x, HDR_JACK_Y - 11.f);  // above jack
      lbl->box.size = Vec(50.f, 14.f);
      lbl->fontSize = 12;
      lbl->value = text;
      lbl->textColor = nvgRGBf(0.1f, 0.1f, 0.1f);
      lbl->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      lbl->breakRowWidth = 50.f;
      addChild(lbl);
    };

    addHdrLabel(CONT_JACK_X + HDR_BTN_DX, "FREEZE");
    addParam(createParam<SmallIsoButton>(
        Vec(CONT_JACK_X + HDR_BTN_DX, HDR_JACK_Y + HDR_BTN_DY), module,
        ComputerscarePortaloof::FREEZE_TOGGLE));
    trackInputPort(
        createInput<InPort>(Vec(CONT_JACK_X, HDR_JACK_Y), module,
                            ComputerscarePortaloof::FREEZE_GATE_INPUT));

    addHdrLabel(MIX_JACK_X + HDR_BTN_DX - 4.f, "MIX");
    addParam(createParam<SmallKnob>(
        Vec(MIX_JACK_X + HDR_BTN_DX + 1.f, HDR_JACK_Y + HDR_BTN_DY + 5.f),
        module, ComputerscarePortaloof::INPUT_SOURCE_MIX));
    trackInputPort(
        createInput<InPort>(Vec(MIX_JACK_X, HDR_JACK_Y), module,
                            ComputerscarePortaloof::INPUT_SOURCE_MIX_INPUT));
    {
      mixLeftLabel = new SmallLetterDisplay();
      mixLeftLabel->box.pos =
          Vec(MIX_JACK_X + HDR_BTN_DX - 18.f, HDR_JACK_Y + 18.f);
      mixLeftLabel->box.size = Vec(34.f, 14.f);
      mixLeftLabel->fontSize = 10;
      mixLeftLabel->letterSpacing = 1.5f;
      mixLeftLabel->value = "Rack";
      mixLeftLabel->textColor = nvgRGBf(0.1f, 0.1f, 0.1f);
      mixLeftLabel->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      mixLeftLabel->breakRowWidth = 34.f;
      addChild(mixLeftLabel);

      mixRightLabel = new SmallLetterDisplay();
      mixRightLabel->box.pos =
          Vec(MIX_JACK_X + HDR_BTN_DX + 8.f, HDR_JACK_Y + 18.f);
      mixRightLabel->box.size = Vec(44.f, 14.f);
      mixRightLabel->fontSize = 10;
      mixRightLabel->letterSpacing = 1.5f;
      mixRightLabel->value = "img2";
      mixRightLabel->textColor = nvgRGBf(0.1f, 0.1f, 0.1f);
      mixRightLabel->baseColor = COLOR_COMPUTERSCARE_TRANSPARENT;
      mixRightLabel->breakRowWidth = 44.f;
      addChild(mixRightLabel);
    }

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
      trackInputPort(
          createInput<OutPort>(Vec(30.f, y - 17.f), module, gateInputIds[i]));
      trackInputPort(
          createInput<InPort>(Vec(68.f, y - 14.f), module, cvInputIds[i]));
      addParam(
          createParam<SmallKnob>(Vec(100.f, y - 9.f), module, attenIds[i]));
      addParam(
          createParam<SmoothKnob>(Vec(122.f, y - 13.f), module, knobIds[i]));
    }

    headerSvg = APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/panels/portaloof-header.svg"));
  }

  void trackInputPort(PortWidget* port) {
    addInput(port);
    HiddenPortPlacement placement;
    placement.port = port;
    placement.visiblePos = port->box.pos;
    hiddenPortPlacements.push_back(placement);
  }

  void updateHiddenPortLayout(bool hideUi) {
    if (portsInHiddenLayout == hideUi) return;

    for (size_t i = 0; i < hiddenPortPlacements.size(); i++) {
      HiddenPortPlacement& p = hiddenPortPlacements[i];
      if (!p.port) continue;

      if (hideUi) {
        p.port->box.pos = Vec(0.f, RACK_GRID_HEIGHT - p.port->box.size.y);
      } else {
        p.port->box.pos = p.visiblePos;
      }
    }

    portsInHiddenLayout = hideUi;
  }

  void updateHiddenUiVisibility(bool hideUi) {
    if (hiddenUiApplied == hideUi) return;

    for (Widget* child : children) {
      child->setVisible(!hideUi || child == rightHandle);
    }

    hiddenUiApplied = hideUi;
  }

  float getHiddenWidth(ComputerscarePortaloof* m) const {
    float fullWidth = m ? m->width : box.size.x;
    return std::max(fullWidth - DISPLAY_X, RACK_GRID_WIDTH);
  }

  void drawBrowserPreview(const DrawArgs& args) {
    // Create the fake module once with random params + a random doc image
    if (!browserModule) {
      browserModule = new ComputerscarePortaloof();
      browserModule->setSourceImage(1, pickRandomDocImage());
      browserModule->inputSourceMix = -1.f;
      browserModule->params[ComputerscarePortaloof::INPUT_SOURCE_MIX].setValue(
          -1.f);

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
      for (int s = 0; s < 2; s++) {
        memcpy(browserModule->sourceRowEnabled[s], browserModule->rowEnabled,
               sizeof(browserModule->rowEnabled));
        memcpy(browserModule->sourceRowValue[s], browserModule->rowValue,
               sizeof(browserModule->rowValue));
      }

      browserModule->freezeMode = false;
      browserModule->maintainAspect = true;
      browserModule->tileEmptySpace = true;
    }

    // Swap in the fake module, run the normal drawLayer pipeline, swap back
    module = browserModule;
    drawLayer(args, 1);
    module = nullptr;
  }

  void draw(const DrawArgs& args) override {
    ComputerscarePortaloof* m = dynamic_cast<ComputerscarePortaloof*>(module);
    if (m && m->hideUi) {
      drawChild(rightHandle, args);
      return;
    }

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
    bool hideUi = m && m->hideUi;
    bool bgActive = backdropWidget != nullptr;
    bool renderInWindow = !bgActive || (m && !m->emptyWindowInBgMode);
    if (portaloofRenderingRackRectSource()) {
      if (!hideUi) ModuleWidget::drawLayer(args, layer);
      return;
    }
    if (layer == 1 && module && APP->scene && APP->scene->rack &&
        renderInWindow) {
      bool doCapture =
          !m->freezeMode || m->capturePending.exchange(false) || !hasValidCache;
      bool hasConfiguredSource =
          m->sources[0].hasSource() || m->sources[1].hasSource();

      if (m && (doCapture || hasConfiguredSource || hasValidCache)) {
        float baseAlpha = 0.85f;

        // Choose live params on capture, or based on transform pre/post setting
        bool useLive = doCapture || !m->freezeMode || m->transformPost;

        const float displayX = hideUi ? 0.f : DISPLAY_X;
        float mirrorW = box.size.x - displayX;
        float mirrorH = box.size.y;

        if (mirrorW <= 2.f) {
          if (!hideUi) ModuleWidget::drawLayer(args, layer);
          return;
        }

        if (doCapture) {
          screenCap.capture(args.vg);
          // Update cache with current params
          memcpy(cachedRowEnabled, m->sourceRowEnabled,
                 sizeof(cachedRowEnabled));
          memcpy(cachedRowValue, m->sourceRowValue, sizeof(cachedRowValue));
          hasValidCache = true;
        }

        int fbW, fbH;
        glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

        PortaloofInjectedSource renderSources[2] = {
            getSource(args.vg, screenCap, m, 0),
            getSource(args.vg, screenCap, m, 1)};
        bool hasSource1 = renderSources[0].isValid();
        bool hasSource2 = renderSources[1].isValid();
        if (!hasSource1 && !hasSource2) {
          if (!hideUi) ModuleWidget::drawLayer(args, layer);
          return;
        }
        float source2Amt = 0.5f * (1.f + m->inputSourceMix);

        nvgSave(args.vg);
        if (m->dimVisualsWithRoom) {
          float b = math::clamp(rack::settings::rackBrightness, 0.f, 1.f);
          nvgGlobalTint(args.vg, nvgRGBAf(b, b, b, 1.f));
        }

        float hw =
            mirrorW * 0.5f;  // display half-width (for scissor/translate)
        float hh = mirrorH * 0.5f;

        // In aspect-ratio mode, size the image to the framebuffer's natural
        // aspect. The scissor clips pillarboxes; scaleX/Y are independent NVG
        // transforms.
        float imgW =
            m->maintainAspect ? (mirrorH * (float)fbW / (float)fbH) : mirrorW;
        float imgHW = imgW * 0.5f;

        bool drewStageBg = false;
        for (int renderSourceIndex = 0; renderSourceIndex < 2;
             renderSourceIndex++) {
          PortaloofInjectedSource currentSource =
              renderSources[renderSourceIndex];
          if (!currentSource.isValid()) continue;
          float sourceMixAlpha = 1.f;
          if (hasSource1 && hasSource2)
            sourceMixAlpha =
                renderSourceIndex == 0 ? (1.f - source2Amt) : source2Amt;
          if (sourceMixAlpha <= 0.f) continue;
          float alpha = baseAlpha * sourceMixAlpha;

          bool* en = (useLive || !hasValidCache)
                         ? m->sourceRowEnabled[renderSourceIndex]
                         : cachedRowEnabled[renderSourceIndex];
          float* rv = (useLive || !hasValidCache)
                          ? m->sourceRowValue[renderSourceIndex]
                          : cachedRowValue[renderSourceIndex];

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
          float foldFreqV = invertOn ? (1.0f + rv[8] * 3.0f) : 1.0f;
          float warpV = curvesOn ? rv[9] : 0.f;

          GLuint srcTex = currentSource.texId;
          int srcImg = currentSource.nvgImg;
          bool flipInputUV = currentSource.flipInputUV;

          float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
          float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
          // Translation is relative to one image width/height
          float txLocal = txOn ? txV * imgW : 0.f;
          float tyLocal = tyOn ? tyV * mirrorH : 0.f;

          // Grey stage background — drawn in raw display coords, isolated from
          // any scale/rotation transforms applied to the image rendering below.
          if (!drewStageBg) {
            drewStageBg = true;
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
          int img = colorFBOs[renderSourceIndex].apply(args.vg, srcTex, srcImg,
                                                       fbW, fbH, hueV, warpV,
                                                       foldFreqV, flipInputUV);
          GLuint effectTex = (img == colorFBOs[renderSourceIndex].nvgImg &&
                              colorFBOs[renderSourceIndex].outTex != 0)
                                 ? colorFBOs[renderSourceIndex].outTex
                                 : srcTex;

          int kaliMode = kaliOn ? (int)kaliV : 0;
          int kaliSegments = std::abs(kaliMode);

          float absSx = std::max(fabsf(sx), 0.01f);
          float absSy = std::max(fabsf(sy), 0.01f);
          float renderScale =
              std::max(APP->window->windowRatio * getAbsoluteZoom(), 1.f);

          if (kaliMode > 0) {
            float kaliTxOff = 0.f, kaliTyOff = 0.f;
            float nvgTx = 0.f, nvgTy = 0.f;
            if (txOn || tyOn) {
              if (m->translateFirst) {
                kaliTxOff = txLocal;
                kaliTyOff = tyLocal;
              } else {
                nvgTx = txOn ? (2.f * txLocal) : 0.f;
                nvgTy = tyOn ? (2.f * tyLocal) : 0.f;
                nvgTranslate(args.vg, nvgTx, nvgTy);
              }
            }
            int flowerTargetW = flowerKaleidTargetDim(imgW, renderScale, fbW);
            int flowerTargetH =
                flowerKaleidTargetDim(mirrorH, renderScale, fbH);
            float flowerScaleX =
                (imgW > 0.f) ? ((float)flowerTargetW / imgW) : 1.f;
            float flowerScaleY =
                (mirrorH > 0.f) ? ((float)flowerTargetH / mirrorH) : 1.f;
            int flowerImg = flowerKaleidFBOs[renderSourceIndex].apply(
                args.vg, effectTex, flowerTargetW, flowerTargetH, kaliSegments,
                rotOn ? rotV : 0.f, kaliTxOff * flowerScaleX,
                kaliTyOff * flowerScaleY, flipInputUV);
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
                    float tileX = (float)i * imgW;
                    float tileY = (float)j * mirrorH;
                    bool reverseX = reverseOuterTile(i);
                    bool reverseY = reverseOuterTile(j);
                    nvgSave(args.vg);
                    nvgTranslate(args.vg, tileX, tileY);
                    if (reverseX || reverseY) {
                      nvgScale(args.vg, reverseX ? -1.f : 1.f,
                               reverseY ? -1.f : 1.f);
                    }
                    NVGpaint p =
                        nvgImagePattern(args.vg, -imgHW, -hh, imgW, mirrorH,
                                        0.f, flowerImg, alpha);
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, -imgHW, -hh, imgW, mirrorH);
                    nvgFillPaint(args.vg, p);
                    nvgFill(args.vg);
                    nvgRestore(args.vg);
                  }
                }
              } else {
                NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW,
                                             mirrorH, 0.f, flowerImg, alpha);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, -imgHW, -hh, imgW, mirrorH);
                nvgFillPaint(args.vg, p);
                nvgFill(args.vg);
              }
            }
          } else if (kaliMode < 0) {
            // No global rotation here — each sector applies its own rotation
            // with sign correction for flipped sectors (see kSector in
            // MirrorKaleidoscope.hpp).
            float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
            float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;

            // Algorithm mode determines whether translate shifts the kali
            // center
            // ("Kaleid > Translate", the default) or scrolls the image content
            // through the fixed kali geometry ("Translate > Kaleid").
            float kaliTxOff = 0.f, kaliTyOff = 0.f;
            float nvgTx = 0.f, nvgTy = 0.f;
            if (txOn || tyOn) {
              if (m->translateFirst) {
                kaliTxOff = txLocal;
                kaliTyOff = tyLocal;
              } else {
                nvgTx = txOn ? (2.f * txLocal) : 0.f;
                nvgTy = tyOn ? (2.f * tyLocal) : 0.f;
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
                  float tileX = (float)i * imgW;
                  float tileY = (float)j * mirrorH;
                  bool reverseX = reverseOuterTile(i);
                  bool reverseY = reverseOuterTile(j);
                  nvgSave(args.vg);
                  nvgTranslate(args.vg, tileX, tileY);
                  if (reverseX || reverseY) {
                    nvgScale(args.vg, reverseX ? -1.f : 1.f,
                             reverseY ? -1.f : 1.f);
                  }
                  float dX = outerTileDisplayMin(pcx, dispHW, tileX, reverseX);
                  float dY = outerTileDisplayMin(pcy, dispHH, tileY, reverseY);
                  drawKaleidoscope(args.vg, img, imgHW, hh, imgW, mirrorH, rHW,
                                   rHH, kaliSegments, alpha, rotOn, rotV, false,
                                   0.f, false, dX, dY, 2.f * dispHW,
                                   2.f * dispHH, kaliTxOff, kaliTyOff);
                  nvgRestore(args.vg);
                }
              }
            } else {
              drawKaleidoscope(args.vg, img, imgHW, hh, imgW, mirrorH, rHW, rHH,
                               kaliSegments, alpha, rotOn, rotV, false, 0.f,
                               false, pcx - dispHW, pcy - dispHH, 2.f * dispHW,
                               2.f * dispHH, kaliTxOff, kaliTyOff);

              // Overlay opaque grey strips covering display area outside the
              // image bounds. Drawing grey ON TOP means its soft NVG edges
              // blend grey-into-grey (invisible) rather than grey-into-image
              // (smear). A 2px overlap into the image eats any residual tiling
              // bleed at the image boundary.
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
            // No kaleidoscope — apply rotation then translate in image space so
            // one full 0-10V sweep always traverses one wrapped image cycle.
            float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
            float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
            if (rotOn) applyRotation(args.vg, rotV);
            float txOffset = txOn ? txLocal : 0.f;
            float tyOffset = tyOn ? tyLocal : 0.f;
            if (txOn || tyOn) nvgTranslate(args.vg, txOffset, tyOffset);
            float txOffsetAbs = fabsf(txOffset);
            float tyOffsetAbs = fabsf(tyOffset);
            float rHW = ((imgW + 2.f * txOffsetAbs) * cosA +
                         (box.size.y + 2.f * tyOffsetAbs) * sinA) /
                            (2.f * absSx) +
                        4.f;
            float rHH = ((imgW + 2.f * txOffsetAbs) * sinA +
                         (box.size.y + 2.f * tyOffsetAbs) * cosA) /
                            (2.f * absSy) +
                        4.f;

            if (tileOn) {
              float pcx = -txOffset;
              float pcy = -tyOffset;
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
        }
        nvgRestore(args.vg);

        // Redraw SVG panel clipped to the overlap strip
        // (DISPLAY_X–CONTROLS_WIDTH) so the display peeks under the panel
        // without covering the controls.
        if (!hideUi && panelSvg && panelSvg->handle) {
          float svgW = panelSvg->handle->width;
          float svgH = panelSvg->handle->height;
          if (svgW > 0.f && svgH > 0.f) {
            nvgSave(args.vg);
            nvgScissor(args.vg, DISPLAY_X - 1.f, 0.f,
                       CONTROLS_WIDTH - DISPLAY_X + 1.f, RACK_GRID_HEIGHT);
            float b = math::clamp(rack::settings::rackBrightness, 0.f, 1.f);
            nvgGlobalTint(args.vg, nvgRGBAf(b, b, b, 1.f));
            nvgScale(args.vg, CONTROLS_WIDTH / svgW, RACK_GRID_HEIGHT / svgH);
            window::svgDraw(args.vg, panelSvg->handle);
            nvgRestore(args.vg);
          }
        }
      }
    }

    if (!hideUi) ModuleWidget::drawLayer(args, layer);

    // Header drawn last — on top of everything including kaleidoscope
    if (!hideUi && layer == 1 && headerSvg && headerSvg->handle) {
      float svgW = headerSvg->handle->width;
      float svgH = headerSvg->handle->height;
      const float drawW = 170.f;
      const float drawH = drawW * (svgH / svgW);
      float b = math::clamp(rack::settings::rackBrightness, 0.f, 1.f);
      nvgSave(args.vg);
      nvgGlobalTint(args.vg, nvgRGBAf(b, b, b, 1.f));
      nvgTranslate(args.vg, 3.f, 3.f);
      nvgScale(args.vg, drawW / svgW, drawH / svgH);
      window::svgDraw(args.vg, headerSvg->handle);
      nvgRestore(args.vg);
    }
  }

  PortaloofInjectedSource getSource(NVGcontext* vg,
                                    const ScreenCapture& capture,
                                    ComputerscarePortaloof* m, int index) {
    PortaloofInjectedSource out;
    if (!m || index < 0 || index >= 2) return out;
    const PortaloofSourceConfig& source = m->sources[index];
    switch (source.kind) {
      case PortaloofSourceKind::IMAGE:
        if (source.imagePath != lastImagePath[index]) {
          if (loadedNvgImg[index] >= 0) {
            nvgDeleteImage(vg, loadedNvgImg[index]);
            loadedNvgImg[index] = -1;
            loadedTexId[index] = 0;
          }
          lastImagePath[index] = source.imagePath;
          if (!lastImagePath[index].empty()) {
            loadedNvgImg[index] =
                nvgCreateImage(vg, lastImagePath[index].c_str(), 0);
            if (loadedNvgImg[index] >= 0)
              loadedTexId[index] = nvglImageHandleGL2(vg, loadedNvgImg[index]);
          }
        }
        out.nvgImg = loadedNvgImg[index];
        out.texId = loadedTexId[index];
        out.flipInputUV = true;
        return out;
      case PortaloofSourceKind::FULL_RACK:
        out.nvgImg = capture.nvgImg;
        out.texId = capture.texId;
        out.flipInputUV = false;
        return out;
      case PortaloofSourceKind::MODULE:
        rackSources[index].setModule(source.moduleId);
        return rackSources[index].render(vg, m->id);
      case PortaloofSourceKind::RACK_RECT:
        rectSources[index].setRect(source.rect);
        return rectSources[index].renderRack(vg, source.rect);
      case PortaloofSourceKind::SCREEN_RECT:
        rectSources[index].setRect(source.rect);
        return rectSources[index].render(vg, capture.nvgImg, source.rect);
      default:
        return out;
    }
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscarePortaloof* m = dynamic_cast<ComputerscarePortaloof*>(module);
    if (!m) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Sources"));
    auto appendSourceMenu = [=](Menu* menu, int sourceIndex) {
      const PortaloofSourceConfig& source = m->sources[sourceIndex];
      menu->addChild(createMenuLabel(m->getSourceMenuLabel(sourceIndex)));
      menu->addChild(createMenuItem("Clear source", "",
                                    [=]() { m->clearSource(sourceIndex); }));
      menu->addChild(createCheckMenuItem(
          "Full Rack Window", "",
          [=]() {
            return m->sources[sourceIndex].kind ==
                   PortaloofSourceKind::FULL_RACK;
          },
          [=]() { m->setSourceFullRack(sourceIndex); }));
      menu->addChild(createMenuItem("Load image...", "", [=]() {
        char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
        if (pathC) {
          m->setSourceImage(sourceIndex, pathC);
          std::free(pathC);
        }
      }));
      menu->addChild(createMenuItem("Select rack rectangle", "", [=]() {
        if (!APP || !APP->scene) return;
        auto* overlay = new PortaloofRectSelectionOverlay();
        overlay->module = m;
        overlay->sourceIndex = sourceIndex;
        overlay->mode = PortaloofRectSelectionOverlay::RACK_RECT;
        APP->scene->addChild(overlay);
      }));
      menu->addChild(createMenuItem("Select window rectangle", "", [=]() {
        if (!APP || !APP->scene) return;
        auto* overlay = new PortaloofRectSelectionOverlay();
        overlay->module = m;
        overlay->sourceIndex = sourceIndex;
        overlay->mode = PortaloofRectSelectionOverlay::SCREEN_RECT;
        APP->scene->addChild(overlay);
      }));
      menu->addChild(createSubmenuItem("Module", "", [=](Menu* menu) {
        PortaloofRackModuleSource selector;
        selector.setModule(source.moduleId);
        menu->addChild(createMenuLabel(selector.describe(m->id)));
        for (app::ModuleWidget* mw : selector.getSelectableModules(m->id)) {
          std::string label = string::f(
              "%s (#%lld)", mw->model ? mw->model->name.c_str() : "Module",
              (long long)mw->module->id);
          menu->addChild(createCheckMenuItem(
              label, "",
              [=]() {
                return m->sources[sourceIndex].kind ==
                           PortaloofSourceKind::MODULE &&
                       m->sources[sourceIndex].moduleId == mw->module->id;
              },
              [=]() { m->setSourceModule(sourceIndex, mw->module->id); }));
        }
      }));
    };
    menu->addChild(
        createSubmenuItem(m->getSourceMenuLabel(0), "",
                          [=](Menu* menu) { appendSourceMenu(menu, 0); }));
    menu->addChild(
        createSubmenuItem(m->getSourceMenuLabel(1), "",
                          [=](Menu* menu) { appendSourceMenu(menu, 1); }));
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
    menu->addChild(createBoolPtrMenuItem("Hide UI", "", &m->hideUi));
    menu->addChild(createBoolPtrMenuItem("Dim visuals with room", "",
                                         &m->dimVisualsWithRoom));
    menu->addChild(createBoolPtrMenuItem("Render as rack background", "",
                                         &m->backdropEnabled));
    menu->addChild(
        createBoolPtrMenuItem("Transform when frozen", "", &m->transformPost));
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
  }

  void onPathDrop(const PathDropEvent& e) override {
    if (!module || e.paths.empty()) return;
    std::string path = e.paths[0];
    std::string ext = system::getExtension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
      auto* m = dynamic_cast<ComputerscarePortaloof*>(module);
      if (m) m->setSourceImage(1, path);
      e.consume(this);
    }
  }

  void step() override {
    if (module) {
      ComputerscarePortaloof* m = dynamic_cast<ComputerscarePortaloof*>(module);
      updateHiddenPortLayout(m->hideUi);
      updateHiddenUiVisibility(m->hideUi);
      if (mixLeftLabel) mixLeftLabel->value = m->getMixLabel(0);
      if (mixRightLabel) mixRightLabel->value = m->getMixLabel(1);

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
      bool hideUiChanged = m->hideUi != lastHideUiForSizing;
      rightHandle->minWidth = m->hideUi ? RACK_GRID_WIDTH : CONTROLS_WIDTH;
      float visibleWidth = m->hideUi ? getHiddenWidth(m) : m->width;

      if (!m->loadedJSON) {
        if (m->sources[1].kind == PortaloofSourceKind::IMAGE &&
            m->sources[1].imagePath.empty())
          m->sources[1].imagePath = pickRandomDocImage();
        box.size.x = visibleWidth;
        if (!windowEmpty)
          bgPanel->box.size.x = m->hideUi ? 0.f : (m->width - DISPLAY_X);
        else
          bgPanel->box.size.x = 0;
        rightHandle->box.pos.x = visibleWidth - rightHandle->box.size.x;
        m->loadedJSON = true;
      } else if (hideUiChanged) {
        float rightEdge = box.pos.x + box.size.x;
        box.size.x = visibleWidth;
        box.pos.x = rightEdge - box.size.x;
        if (APP->scene && APP->scene->rack)
          APP->scene->rack->requestModulePos(this, box.pos);
        rightHandle->box.pos.x = visibleWidth - rightHandle->box.size.x;
      } else if (m->hideUi && box.size.x != visibleWidth) {
        m->width =
            std::max(box.size.x + DISPLAY_X, DISPLAY_X + RACK_GRID_WIDTH);
        visibleWidth = getHiddenWidth(m);
        rightHandle->box.pos.x = visibleWidth - rightHandle->box.size.x;
      } else if (!m->hideUi && box.size.x != m->width) {
        m->width = box.size.x;
        if (!windowEmpty) bgPanel->box.size.x = box.size.x - DISPLAY_X;
        rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
      } else {
        if (box.size.x != visibleWidth) box.size.x = visibleWidth;
        // Keep panel width in sync when emptyWindowInBgMode toggles
        float desiredPanelW =
            (windowEmpty || m->hideUi) ? 0.f : (box.size.x - DISPLAY_X);
        if (bgPanel->box.size.x != desiredPanelW)
          bgPanel->box.size.x = desiredPanelW;
        rightHandle->box.pos.x = visibleWidth - rightHandle->box.size.x;
      }
      lastHideUiForSizing = m->hideUi;
    }
    ModuleWidget::step();
  }
};

Model* modelComputerscarePortaloof =
    createModel<ComputerscarePortaloof, ComputerscarePortaloofWidget>(
        "computerscare-portaloof");
