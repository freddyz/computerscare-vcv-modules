#include "Computerscare.hpp"
#include "Portaloof/RackModuleSource.hpp"

struct LabsLevelsFBO {
  GLuint fbo = 0;
  GLuint outTex = 0;
  GLuint program = 0;
  GLuint vbo = 0;
  int nvgImg = -1;
  int lastW = 0;
  int lastH = 0;

  GLint uTex = -1;
  GLint uInBlack = -1;
  GLint uInWhite = -1;
  GLint uOutBlack = -1;
  GLint uOutWhite = -1;
  GLint uDarkAmount = -1;
  GLint uLumaInvert = -1;
  GLint uHuePreserve = -1;
  GLint uSaturation = -1;
  GLint uGamma = -1;
  GLint uShadowLift = -1;
  GLint uHighlightCeiling = -1;
  GLint uAccentPreserve = -1;
  GLint aPos = -1;
  bool initialized = false;

  static const char* vertSrc() {
    return "#version 120\n"
           "attribute vec2 pos;\n"
           "varying vec2 uv;\n"
           "void main() {\n"
           "  gl_Position = vec4(pos, 0.0, 1.0);\n"
           "  uv = vec2(pos.x * 0.5 + 0.5, pos.y * 0.5 + 0.5);\n"
           "}\n";
  }

  static const char* fragSrc() {
    return "#version 120\n"
           "varying vec2 uv;\n"
           "uniform sampler2D tex;\n"
           "uniform float inBlack;\n"
           "uniform float inWhite;\n"
           "uniform float outBlack;\n"
           "uniform float outWhite;\n"
           "uniform float darkAmount;\n"
           "uniform float lumaInvert;\n"
           "uniform float huePreserve;\n"
           "uniform float saturation;\n"
           "uniform float gammaValue;\n"
           "uniform float shadowLift;\n"
           "uniform float highlightCeiling;\n"
           "uniform float accentPreserve;\n"
           "float luminance(vec3 c) {\n"
           "  return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
           "}\n"
           "float max3(vec3 c) {\n"
           "  return max(max(c.r, c.g), c.b);\n"
           "}\n"
           "float min3(vec3 c) {\n"
           "  return min(min(c.r, c.g), c.b);\n"
           "}\n"
           "void main() {\n"
           "  vec4 col = texture2D(tex, uv);\n"
           "  float denom = max(inWhite - inBlack, 0.001);\n"
           "  vec3 t = clamp((col.rgb - vec3(inBlack)) / denom, 0.0, 1.0);\n"
           "  vec3 rgb = mix(vec3(outBlack), vec3(outWhite), t);\n"
           "  float baseLum = luminance(rgb);\n"
           "  rgb = clamp(mix(vec3(baseLum), rgb, saturation), 0.0, 1.0);\n"
           "  rgb = pow(rgb, vec3(1.0 / max(gammaValue, 0.001)));\n"
           "  vec3 originalRgb = rgb;\n"
           "  float lum = luminance(rgb);\n"
           "  float invLum = mix(lum, 1.0 - lum, lumaInvert);\n"
           "  float darkFloor = shadowLift * 0.75;\n"
           "  float darkTop = max(mix(0.02, 1.0, highlightCeiling), "
           "darkFloor + 0.001);\n"
           "  float darkLum = mix(darkFloor, darkTop, clamp(invLum, 0.0, "
           "1.0));\n"
           "  vec3 grayDark = vec3(darkLum);\n"
           "  vec3 hueDark = clamp(rgb * (darkLum / max(lum, 0.001)), 0.0, "
           "1.0);\n"
           "  vec3 darkRgb = mix(grayDark, hueDark, huePreserve);\n"
           "  float darkRgbLum = luminance(darkRgb);\n"
           "  darkRgb = mix(vec3(darkRgbLum), darkRgb, saturation);\n"
           "  darkRgb = mix(darkRgb, originalRgb, accentPreserve);\n"
           "  rgb = mix(rgb, clamp(darkRgb, 0.0, 1.0), darkAmount);\n"
           "  gl_FragColor = vec4(rgb, col.a);\n"
           "}\n";
  }

  static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[512];
      glGetShaderInfoLog(shader, 512, nullptr, log);
      DEBUG("LabsLevelsFBO shader compile error: %s", log);
      glDeleteShader(shader);
      return 0;
    }
    return shader;
  }

  static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vs);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!vert || !frag) {
      glDeleteShader(vert);
      glDeleteShader(frag);
      return 0;
    }

    GLuint linked = glCreateProgram();
    glAttachShader(linked, vert);
    glAttachShader(linked, frag);
    glLinkProgram(linked);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(linked, GL_LINK_STATUS, &ok);
    if (!ok) {
      char log[512];
      glGetProgramInfoLog(linked, 512, nullptr, log);
      DEBUG("LabsLevelsFBO link error: %s", log);
      glDeleteProgram(linked);
      return 0;
    }
    return linked;
  }

  void init() {
    initialized = true;

    static const float quad[] = {-1.f, -1.f, 1.f, -1.f, 1.f,  1.f,
                                 -1.f, -1.f, 1.f, 1.f,  -1.f, 1.f};
    glGenBuffers(1, &vbo);
    GLint prevVBO = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);

    glGenFramebuffers(1, &fbo);

    program = linkProgram(vertSrc(), fragSrc());
    if (program) {
      uTex = glGetUniformLocation(program, "tex");
      uInBlack = glGetUniformLocation(program, "inBlack");
      uInWhite = glGetUniformLocation(program, "inWhite");
      uOutBlack = glGetUniformLocation(program, "outBlack");
      uOutWhite = glGetUniformLocation(program, "outWhite");
      uDarkAmount = glGetUniformLocation(program, "darkAmount");
      uLumaInvert = glGetUniformLocation(program, "lumaInvert");
      uHuePreserve = glGetUniformLocation(program, "huePreserve");
      uSaturation = glGetUniformLocation(program, "saturation");
      uGamma = glGetUniformLocation(program, "gammaValue");
      uShadowLift = glGetUniformLocation(program, "shadowLift");
      uHighlightCeiling = glGetUniformLocation(program, "highlightCeiling");
      uAccentPreserve = glGetUniformLocation(program, "accentPreserve");
      aPos = glGetAttribLocation(program, "pos");
    }
  }

  void resizeIfNeeded(NVGcontext* vg, int w, int h) {
    if (w == lastW && h == lastH && outTex && nvgImg >= 0) return;

    if (nvgImg >= 0) {
      nvgDeleteImage(vg, nvgImg);
      nvgImg = -1;
    }
    if (outTex) {
      glDeleteTextures(1, &outTex);
      outTex = 0;
    }

    glGenTextures(1, &outTex);
    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glBindTexture(GL_TEXTURE_2D, outTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    GLint prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           outTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glBindTexture(GL_TEXTURE_2D, prevTex);

    nvgImg = nvglCreateImageFromHandleGL2(vg, outTex, w, h, NVG_IMAGE_FLIPY);
    lastW = w;
    lastH = h;
  }

  int apply(NVGcontext* vg, GLuint srcTex, int srcImg, int fbW, int fbH,
            float inBlack, float inWhite, float outBlack, float outWhite,
            float darkAmount, float lumaInvert, float huePreserve,
            float saturation, float gammaValue, float shadowLift,
            float highlightCeiling, float accentPreserve) {
    if (!initialized) init();
    if (!program || fbW <= 0 || fbH <= 0) return srcImg;

    resizeIfNeeded(vg, fbW, fbH);
    if (nvgImg < 0) return srcImg;

    GLint prevDrawFBO = 0;
    GLint prevProg = 0;
    GLint prevVBO = 0;
    GLint prevViewport[4] = {};
    GLint prevActiveTex = 0;
    GLint prevTex0 = 0;
    GLboolean prevDepth = false;
    GLboolean prevBlend = false;
    GLboolean prevCull = false;
    GLboolean prevStencil = false;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTex);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
    glGetBooleanv(GL_DEPTH_TEST, &prevDepth);
    glGetBooleanv(GL_BLEND, &prevBlend);
    glGetBooleanv(GL_CULL_FACE, &prevCull);
    glGetBooleanv(GL_STENCIL_TEST, &prevStencil);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, fbW, fbH);
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (aPos >= 0) {
      glEnableVertexAttribArray(aPos);
      glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(uTex, 0);
    glUniform1f(uInBlack, inBlack);
    glUniform1f(uInWhite, inWhite);
    glUniform1f(uOutBlack, outBlack);
    glUniform1f(uOutWhite, outWhite);
    glUniform1f(uDarkAmount, darkAmount);
    glUniform1f(uLumaInvert, lumaInvert);
    glUniform1f(uHuePreserve, huePreserve);
    glUniform1f(uSaturation, saturation);
    glUniform1f(uGamma, gammaValue);
    glUniform1f(uShadowLift, shadowLift);
    glUniform1f(uHighlightCeiling, highlightCeiling);
    glUniform1f(uAccentPreserve, accentPreserve);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (aPos >= 0) glDisableVertexAttribArray(aPos);

    glBindFramebuffer(GL_FRAMEBUFFER, prevDrawFBO);
    glUseProgram(prevProg);
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
               prevViewport[3]);
    glBindTexture(GL_TEXTURE_2D, prevTex0);
    glActiveTexture(prevActiveTex);
    if (prevDepth)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);
    if (prevBlend)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);
    if (prevCull)
      glEnable(GL_CULL_FACE);
    else
      glDisable(GL_CULL_FACE);
    if (prevStencil)
      glEnable(GL_STENCIL_TEST);
    else
      glDisable(GL_STENCIL_TEST);

    return nvgImg;
  }

  ~LabsLevelsFBO() = default;
};

struct LabsModuleCapture {
  struct CacheEntry {
    NVGLUframebuffer* fbs[2] = {};
    math::Vec fbSize;
    int frontIndex = 0;
    bool hasFront = false;
    bool rendering = false;
  };

  CacheEntry cacheEntry;

  PortaloofInjectedSource render(NVGcontext* vg, app::ModuleWidget* mw,
                                 int64_t selfModuleId, float renderScale) {
    PortaloofInjectedSource out;
    if (!mw || !mw->module || mw->module->id == selfModuleId) return out;
    if (!mw->box.size.isFinite() || mw->box.size.x <= 1.f ||
        mw->box.size.y <= 1.f)
      return out;

    if (cacheEntry.rendering) return getFrontSource(vg, cacheEntry);

    renderScale = std::max(renderScale, 1.f);
    math::Vec newFbSize = mw->box.size.mult(renderScale).ceil();
    ensureFramebuffers(vg, cacheEntry, newFbSize);
    if (!cacheEntry.fbs[0] || !cacheEntry.fbs[1])
      return getFrontSource(vg, cacheEntry);

    int backIndex = 1 - cacheEntry.frontIndex;
    NVGLUframebuffer* backFb = cacheEntry.fbs[backIndex];
    if (!backFb) return getFrontSource(vg, cacheEntry);

    GLint prevDrawFBO = 0;
    GLint prevViewport[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    cacheEntry.rendering = true;

    nvgluBindFramebuffer(backFb);
    glViewport(0, 0, (GLsizei)newFbSize.x, (GLsizei)newFbSize.y);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    NVGcontext* fbVg = APP->window->fbVg;
    nvgBeginFrame(fbVg, mw->box.size.x, mw->box.size.y, renderScale);
    {
      widget::Widget::DrawArgs drawArgs;
      drawArgs.vg = fbVg;
      drawArgs.fb = backFb;
      drawArgs.clipBox = math::Rect(math::Vec(0.f, 0.f), mw->box.size);
      mw->draw(drawArgs);
      mw->drawLayer(drawArgs, 1);
    }
    nvgEndFrame(fbVg);
    nvgReset(fbVg);

    nvgluBindFramebuffer(nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, prevDrawFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
               prevViewport[3]);

    cacheEntry.frontIndex = backIndex;
    cacheEntry.hasFront = true;
    cacheEntry.rendering = false;
    return getFrontSource(vg, cacheEntry);
  }

 private:
  static void deleteFramebuffer(NVGLUframebuffer*& fb) {
    if (fb) {
      nvgluDeleteFramebuffer(fb);
      fb = nullptr;
    }
  }

  static PortaloofInjectedSource getFrontSource(NVGcontext* vg,
                                                CacheEntry& entry) {
    PortaloofInjectedSource out;
    if (!entry.hasFront) return out;
    NVGLUframebuffer* frontFb = entry.fbs[entry.frontIndex];
    if (!frontFb) return out;
    out.nvgImg = frontFb->image;
    out.texId = nvglImageHandleGL2(vg, out.nvgImg);
    out.flipInputUV = false;
    return out;
  }

  static void ensureFramebuffers(NVGcontext* vg, CacheEntry& entry,
                                 math::Vec newFbSize) {
    if (entry.fbs[0] && entry.fbs[1] && newFbSize.equals(entry.fbSize)) return;

    deleteFramebuffer(entry.fbs[0]);
    deleteFramebuffer(entry.fbs[1]);
    entry.fbSize = math::Vec();
    entry.hasFront = false;

    if (!newFbSize.isFinite() || newFbSize.isZero()) return;

    entry.fbs[0] =
        nvgluCreateFramebuffer(vg, (int)newFbSize.x, (int)newFbSize.y, 0);
    entry.fbs[1] =
        nvgluCreateFramebuffer(vg, (int)newFbSize.x, (int)newFbSize.y, 0);
    if (entry.fbs[0] && entry.fbs[1]) {
      entry.fbSize = newFbSize;
      entry.frontIndex = 0;
    } else {
      deleteFramebuffer(entry.fbs[0]);
      deleteFramebuffer(entry.fbs[1]);
    }
  }
};

struct ComputerscareLabs : Module {
  enum ParamIds {
    ENABLE_PARAM,
    SELECT_PARAM,
    IN_BLACK_PARAM,
    IN_WHITE_PARAM,
    OUT_BLACK_PARAM,
    OUT_WHITE_PARAM,
    DARK_AMOUNT_PARAM,
    LUMA_INVERT_PARAM,
    HUE_PRESERVE_PARAM,
    SATURATION_PARAM,
    GAMMA_PARAM,
    SHADOW_LIFT_PARAM,
    HIGHLIGHT_CEILING_PARAM,
    ACCENT_PRESERVE_PARAM,
    NUM_PARAMS
  };
  enum InputIds { NUM_INPUTS };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  int64_t targetModuleId = -1;

  ComputerscareLabs() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configSwitch(ENABLE_PARAM, 0.f, 1.f, 1.f, "Levels overlay", {"Off", "On"});
    getParamQuantity(ENABLE_PARAM)->randomizeEnabled = false;
    configButton(SELECT_PARAM, "Select module to process");
    configParam(IN_BLACK_PARAM, 0.f, 1.f, 0.f, "Input Black", "%", 0.f, 100.f);
    configParam(IN_WHITE_PARAM, 0.f, 1.f, 1.f, "Input White", "%", 0.f, 100.f);
    configParam(OUT_BLACK_PARAM, 0.f, 1.f, 0.f, "Output Black", "%", 0.f,
                100.f);
    configParam(OUT_WHITE_PARAM, 0.f, 1.f, 1.f, "Output White", "%", 0.f,
                100.f);
    configParam(DARK_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Dark Amount", "%", 0.f,
                100.f);
    configParam(LUMA_INVERT_PARAM, 0.f, 1.f, 1.f, "Luma Invert", "%", 0.f,
                100.f);
    configParam(HUE_PRESERVE_PARAM, 0.f, 1.f, 1.f, "Hue Preserve", "%", 0.f,
                100.f);
    configParam(SATURATION_PARAM, 0.f, 2.f, 1.f, "Saturation", "%", 0.f, 100.f);
    configParam(GAMMA_PARAM, 0.25f, 4.f, 1.f, "Gamma");
    configParam(SHADOW_LIFT_PARAM, 0.f, 1.f, 0.f, "Shadow Lift", "%", 0.f,
                100.f);
    configParam(HIGHLIGHT_CEILING_PARAM, 0.f, 1.f, 0.35f, "Highlight Ceiling",
                "%", 0.f, 100.f);
    configParam(ACCENT_PRESERVE_PARAM, 0.f, 1.f, 0.f, "Accent Preserve", "%",
                0.f, 100.f);
  }

  bool isEnabled() { return params[ENABLE_PARAM].getValue() > 0.5f; }
  void setTargetModule(int64_t id) { targetModuleId = id; }
  void clearTargetModule() { targetModuleId = -1; }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "targetModuleId", json_integer(targetModuleId));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* targetModuleIdJ = json_object_get(rootJ, "targetModuleId");
    if (targetModuleIdJ) {
      targetModuleId = json_integer_value(targetModuleIdJ);
    }
  }
};

struct LabsTargetOverlay : widget::TransparentWidget {
  ComputerscareLabs* module = nullptr;
  LabsModuleCapture capture;
  LabsLevelsFBO levelsFBO;

  void step() override {
    if (APP && APP->scene && APP->scene->rack) {
      box.size = APP->scene->rack->box.size;
    }
    TransparentWidget::step();
  }

  app::ModuleWidget* getTargetModuleWidget() const {
    if (!module || module->targetModuleId < 0 || !APP || !APP->scene ||
        !APP->scene->rack) {
      return nullptr;
    }

    app::ModuleWidget* mw = APP->scene->rack->getModule(module->targetModuleId);
    if (!mw || !mw->module) {
      return nullptr;
    }
    return mw;
  }

  void draw(const DrawArgs& args) override {
    if (!module || !module->isEnabled()) {
      return;
    }

    app::ModuleWidget* target = getTargetModuleWidget();
    if (!target || !APP || !APP->scene || !APP->scene->rack) {
      return;
    }

    float zoom = APP->scene->rack->getAbsoluteZoom();
    if (zoom <= 0.f) {
      return;
    }

    float renderScale = std::max(1.f, APP->window->pixelRatio * zoom);
    PortaloofInjectedSource rendered =
        capture.render(args.vg, target, module->id, renderScale);
    if (!rendered.isValid()) {
      return;
    }

    math::Vec fbSize = target->box.size.mult(renderScale).ceil();
    int img = levelsFBO.apply(
        args.vg, rendered.texId, rendered.nvgImg, (int)fbSize.x, (int)fbSize.y,
        module->params[ComputerscareLabs::IN_BLACK_PARAM].getValue(),
        module->params[ComputerscareLabs::IN_WHITE_PARAM].getValue(),
        module->params[ComputerscareLabs::OUT_BLACK_PARAM].getValue(),
        module->params[ComputerscareLabs::OUT_WHITE_PARAM].getValue(),
        module->params[ComputerscareLabs::DARK_AMOUNT_PARAM].getValue(),
        module->params[ComputerscareLabs::LUMA_INVERT_PARAM].getValue(),
        module->params[ComputerscareLabs::HUE_PRESERVE_PARAM].getValue(),
        module->params[ComputerscareLabs::SATURATION_PARAM].getValue(),
        module->params[ComputerscareLabs::GAMMA_PARAM].getValue(),
        module->params[ComputerscareLabs::SHADOW_LIFT_PARAM].getValue(),
        module->params[ComputerscareLabs::HIGHLIGHT_CEILING_PARAM].getValue(),
        module->params[ComputerscareLabs::ACCENT_PRESERVE_PARAM].getValue());
    if (img < 0) {
      return;
    }

    NVGpaint paint =
        nvgImagePattern(args.vg, target->box.pos.x, target->box.pos.y,
                        target->box.size.x, target->box.size.y, 0.f, img, 1.f);
    nvgBeginPath(args.vg);
    nvgRect(args.vg, target->box.pos.x, target->box.pos.y, target->box.size.x,
            target->box.size.y);
    nvgFillPaint(args.vg, paint);
    nvgFill(args.vg);
  }
};

struct LabsModuleSelectionOverlay : widget::OpaqueWidget {
  ComputerscareLabs* module = nullptr;

  LabsModuleSelectionOverlay() {
    if (APP && APP->scene) {
      box.size = APP->scene->box.size;
    }
  }

  void step() override {
    if (APP && APP->scene) {
      box.size = APP->scene->box.size;
    }
    OpaqueWidget::step();
  }

  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, 0.08f));
    nvgFill(args.vg);
  }

  static math::Vec sceneToRackPos(math::Vec scenePos) {
    if (!APP || !APP->scene || !APP->scene->rack) {
      return math::Vec();
    }

    float zoom = APP->scene->rack->getAbsoluteZoom();
    if (zoom <= 0.f) {
      return math::Vec();
    }

    math::Vec rackOrigin = APP->scene->rack->getAbsoluteOffset(math::Vec());
    return scenePos.minus(rackOrigin).div(zoom);
  }

  app::ModuleWidget* getModuleAt(math::Vec rackPos) const {
    if (!module || !APP || !APP->scene || !APP->scene->rack) {
      return nullptr;
    }

    std::vector<app::ModuleWidget*> modules = APP->scene->rack->getModules();
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
      app::ModuleWidget* mw = *it;
      if (!mw || !mw->module || mw->module == module) {
        continue;
      }

      if (mw->box.contains(rackPos)) {
        return mw;
      }
    }

    return nullptr;
  }

  void onButton(const ButtonEvent& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      app::ModuleWidget* mw = getModuleAt(sceneToRackPos(e.pos));
      if (module && mw && mw->module) {
        module->setTargetModule(mw->module->id);
      }
      requestDelete();
      e.consume(this);
      return;
    }

    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      requestDelete();
      e.consume(this);
      return;
    }

    OpaqueWidget::onButton(e);
  }
};

struct LabsOnOffButton : ComputerscareBlankButton {
  LabsOnOffButton() {
    momentary = false;
    shadow->opacity = 0.f;
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
  }

  bool isOn() {
    ParamQuantity* pq = getParamQuantity();
    return pq && pq->getValue() > 0.5f;
  }

  void step() override {
    ComputerscareBlankButton::step();
    setIconPressed(isOn());
  }

  void draw(const DrawArgs& args) override {
    ComputerscareBlankButton::draw(args);

    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    float textXOffset = isOn() ? 2.3f : -1.2f;
    float textYOffset = isOn() ? 3.6f : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + textXOffset,
            box.size.y * 0.48f + textYOffset, "ON", NULL);
  }
};

struct LabsSelectButton : ComputerscareBlankButton {
  LabsSelectButton() {
    shadow->opacity = 0.f;
    iconUpPos = Vec(0.f, 0.f);
    iconDownOffset = Vec(0.f, 0.f);
  }

  bool isPressed() {
    ParamQuantity* pq = getParamQuantity();
    return pq && pq->getValue() > 0.5f;
  }

  void step() override {
    ComputerscareBlankButton::step();
    setIconPressed(isPressed());
  }

  void draw(const DrawArgs& args) override {
    ComputerscareBlankButton::draw(args);

    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 9.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    float textXOffset = isPressed() ? 2.3f : -1.2f;
    float textYOffset = isPressed() ? 3.6f : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + textXOffset,
            box.size.y * 0.48f + textYOffset, "SEL", NULL);
  }
};

struct LabsPanelLabels : widget::TransparentWidget {
  void drawLabel(const DrawArgs& args, float x, float y, const char* label,
                 float fontSize = 8.5f) {
    nvgFontSize(args.vg, fontSize);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    nvgText(args.vg, x, y, label, NULL);
  }

  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    drawLabel(args, 18.f, 100.f, "IN B");
    drawLabel(args, 42.f, 100.f, "OUT B");
    drawLabel(args, 18.f, 144.f, "IN W");
    drawLabel(args, 42.f, 144.f, "OUT W");
    drawLabel(args, 18.f, 188.f, "DARK");
    drawLabel(args, 42.f, 188.f, "L INV");
    drawLabel(args, 18.f, 232.f, "HUE");
    drawLabel(args, 42.f, 232.f, "SAT");
    drawLabel(args, 18.f, 276.f, "GAM");
    drawLabel(args, 42.f, 276.f, "LIFT");
    drawLabel(args, 18.f, 320.f, "CEIL");
    drawLabel(args, 42.f, 320.f, "ACC");
  }
};

struct ComputerscareLabsWidget : ModuleWidget {
  LabsTargetOverlay* targetOverlay = nullptr;
  dsp::SchmittTrigger selectTrigger;

  ComputerscareLabsWidget(ComputerscareLabs* module) {
    setModule(module);
    box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
    panel->box.size = box.size;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareLabsPanel.svg")));
    addChild(panel);

    LabsPanelLabels* labels = new LabsPanelLabels();
    labels->box.size = box.size;
    addChild(labels);

    addParam(createParamCentered<LabsOnOffButton>(
        Vec(box.size.x / 2.f, 32.f), module, ComputerscareLabs::ENABLE_PARAM));

    addParam(createParamCentered<LabsSelectButton>(
        Vec(box.size.x / 2.f, 63.f), module, ComputerscareLabs::SELECT_PARAM));

    addParam(createParamCentered<SmallKnob>(Vec(18.f, 116.f), module,
                                            ComputerscareLabs::IN_BLACK_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(42.f, 116.f), module, ComputerscareLabs::OUT_BLACK_PARAM));
    addParam(createParamCentered<SmallKnob>(Vec(18.f, 160.f), module,
                                            ComputerscareLabs::IN_WHITE_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(42.f, 160.f), module, ComputerscareLabs::OUT_WHITE_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(18.f, 204.f), module, ComputerscareLabs::DARK_AMOUNT_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(42.f, 204.f), module, ComputerscareLabs::LUMA_INVERT_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(18.f, 248.f), module, ComputerscareLabs::HUE_PRESERVE_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(42.f, 248.f), module, ComputerscareLabs::SATURATION_PARAM));
    addParam(createParamCentered<SmallKnob>(Vec(18.f, 292.f), module,
                                            ComputerscareLabs::GAMMA_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(42.f, 292.f), module, ComputerscareLabs::SHADOW_LIFT_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(18.f, 336.f), module, ComputerscareLabs::HIGHLIGHT_CEILING_PARAM));
    addParam(createParamCentered<SmallKnob>(
        Vec(42.f, 336.f), module, ComputerscareLabs::ACCENT_PRESERVE_PARAM));
  }

  ~ComputerscareLabsWidget() { removeTargetOverlay(); }

  void step() override {
    ComputerscareLabs* labs = dynamic_cast<ComputerscareLabs*>(module);
    if (labs && selectTrigger.process(
                    labs->params[ComputerscareLabs::SELECT_PARAM].getValue())) {
      startModuleSelection(labs);
    }

    if (labs) {
      ensureTargetOverlay(labs);
    }

    ModuleWidget::step();
  }

  void onRemove(const RemoveEvent& e) override {
    removeTargetOverlay();
    ModuleWidget::onRemove(e);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareLabs* labs = dynamic_cast<ComputerscareLabs*>(module);
    if (!labs) {
      return;
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuItem("Select module to process", "",
                                  [=]() { startModuleSelection(labs); }));
    menu->addChild(createMenuItem("Clear selected module", "",
                                  [=]() { labs->clearTargetModule(); }));
  }

  void startModuleSelection(ComputerscareLabs* labs) {
    if (!labs || !APP || !APP->scene) {
      return;
    }

    LabsModuleSelectionOverlay* overlay = new LabsModuleSelectionOverlay();
    overlay->module = labs;
    APP->scene->addChild(overlay);
  }

  void ensureTargetOverlay(ComputerscareLabs* labs) {
    if (targetOverlay || !labs || !APP || !APP->scene || !APP->scene->rack) {
      return;
    }

    targetOverlay = new LabsTargetOverlay();
    targetOverlay->module = labs;
    targetOverlay->box.size = APP->scene->rack->box.size;
    APP->scene->rack->addChildBelow(targetOverlay,
                                    APP->scene->rack->getCableContainer());
  }

  void removeTargetOverlay() {
    if (!targetOverlay) {
      return;
    }

    targetOverlay->requestDelete();
    targetOverlay = nullptr;
  }
};

Model* modelComputerscareLabs =
    createModel<ComputerscareLabs, ComputerscareLabsWidget>(
        "computerscare-labs");
