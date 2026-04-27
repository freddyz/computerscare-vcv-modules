#pragma once
#include <nanovg_gl.h>

#include "rack.hpp"
using namespace rack;

// Shader-backed radial kaleidoscope. It folds the output angle into a single
// mirrored wedge, then samples the source texture with explicit wrapping so the
// content stays seamless under Translate > Kaleid motion.
struct FlowerKaleidFBO {
  GLuint fbo = 0;
  GLuint outTex = 0;
  GLuint program = 0;
  GLuint vbo = 0;
  int nvgImg = -1;
  int lastW = 0, lastH = 0;

  GLint uTex = -1;
  GLint uPanelSize = -1;
  GLint uPetals = -1;
  GLint uRotRad = -1;
  GLint uTranslate = -1;
  GLint uFlipY = -1;
  GLint aPos = -1;

  bool initialized = false;

  static const char* vertSrc() {
    return "#version 120\n"
           "attribute vec2 pos;\n"
           "varying vec2 uv;\n"
           "void main() {\n"
           "  gl_Position = vec4(pos, 0.0, 1.0);\n"
           "  uv = pos * 0.5 + 0.5;\n"
           "}\n";
  }

  static const char* fragSrc() {
    return "#version 120\n"
           "varying vec2 uv;\n"
           "uniform sampler2D tex;\n"
           "uniform vec2 panelSize;\n"
           "uniform float petals;\n"
           "uniform float rotRad;\n"
           "uniform vec2 translatePx;\n"
           "uniform float flipY;\n"
           "const float PI = 3.14159265358979323846;\n"
           "\n"
           "float posmod(float x, float y) {\n"
           "  return mod(mod(x, y) + y, y);\n"
           "}\n"
           "\n"
           "vec2 wrap01(vec2 v) {\n"
           "  return vec2(fract(v.x), fract(v.y));\n"
           "}\n"
           "\n"
           "void main() {\n"
           "  vec2 halfSize = panelSize * 0.5;\n"
           "  vec2 p = uv * panelSize - halfSize;\n"
           "  float r = length(p);\n"
           "  float wedge = PI / max(petals, 1.0);\n"
           "  float ang = atan(p.y, p.x) + rotRad;\n"
           "  float folded = posmod(ang, 2.0 * wedge);\n"
           "  folded = abs(folded - wedge);\n"
           "  vec2 src = vec2(cos(folded), sin(folded)) * r + translatePx;\n"
           "  vec2 srcUv = wrap01((src + halfSize) / panelSize);\n"
           "  float sampleY = (flipY > 0.0) ? (1.0 - srcUv.y) : srcUv.y;\n"
           "  gl_FragColor = texture2D(tex, vec2(srcUv.x, sampleY));\n"
           "}\n";
  }

  static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[512];
      glGetShaderInfoLog(s, 512, nullptr, log);
      DEBUG("FlowerKaleidFBO shader compile error: %s", log);
      glDeleteShader(s);
      return 0;
    }
    return s;
  }

  static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
      glDeleteShader(v);
      glDeleteShader(f);
      return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
      char log[512];
      glGetProgramInfoLog(p, 512, nullptr, log);
      DEBUG("FlowerKaleidFBO link error: %s", log);
      glDeleteProgram(p);
      return 0;
    }
    return p;
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
      uPanelSize = glGetUniformLocation(program, "panelSize");
      uPetals = glGetUniformLocation(program, "petals");
      uRotRad = glGetUniformLocation(program, "rotRad");
      uTranslate = glGetUniformLocation(program, "translatePx");
      uFlipY = glGetUniformLocation(program, "flipY");
      aPos = glGetAttribLocation(program, "pos");
    }
  }

  void resizeIfNeeded(NVGcontext* vg, int w, int h) {
    if (w == lastW && h == lastH) return;
    if (nvgImg >= 0) {
      nvgDeleteImage(vg, nvgImg);
      nvgImg = -1;
    }
    if (outTex) {
      glDeleteTextures(1, &outTex);
      outTex = 0;
    }

    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glGenTextures(1, &outTex);
    glBindTexture(GL_TEXTURE_2D, outTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glBindTexture(GL_TEXTURE_2D, prevTex);

    GLint prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           outTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

    nvgImg = nvglCreateImageFromHandleGL2(vg, outTex, w, h, NVG_IMAGE_FLIPY);
    lastW = w;
    lastH = h;
  }

  int apply(NVGcontext* vg, GLuint srcTex, int outW, int outH, int petals,
            float rotDeg, float txOff, float tyOff, bool flipInputUV = false) {
    if (srcTex == 0 || petals < 1) return -1;

    if (!initialized) init();
    if (!program) return -1;

    outW = std::max(outW, 1);
    outH = std::max(outH, 1);
    resizeIfNeeded(vg, outW, outH);
    if (nvgImg < 0) return -1;

    GLint prevDrawFBO = 0, prevProg = 0, prevVBO = 0;
    GLint prevViewport[4];
    GLint prevActiveTex = 0, prevTex0 = 0;
    GLboolean prevDepth = GL_FALSE, prevBlend = GL_FALSE, prevCull = GL_FALSE,
              prevStencil = GL_FALSE;

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
    glViewport(0, 0, outW, outH);
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (aPos >= 0) {
      glEnableVertexAttribArray(aPos);
      glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(uTex, 0);
    glUniform2f(uPanelSize, (float)outW, (float)outH);
    glUniform1f(uPetals, (float)petals);
    glUniform1f(uRotRad, rotDeg * (float)M_PI / 180.f);
    glUniform2f(uTranslate, txOff, tyOff);
    glUniform1f(uFlipY, flipInputUV ? -1.0f : 1.0f);
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

  ~FlowerKaleidFBO() = default;
};
