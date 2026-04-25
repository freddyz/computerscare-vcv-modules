#pragma once

#include "PortaloofBackdropWidget.hpp"

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
