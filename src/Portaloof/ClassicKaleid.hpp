#pragma once
#include <nanovg_gl.h>

#include "rack.hpp"
using namespace rack;

struct ClassicKaleidFBO {
  GLuint fbo = 0;
  GLuint outTex = 0;
  GLuint program = 0;
  GLuint vbo = 0;
  int nvgImg = -1;
  int lastW = 0, lastH = 0;

  GLint uTex = -1;
  GLint uPanelSize = -1;
  GLint uMode = -1;
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
           "uniform int mode;\n"
           "uniform float rotRad;\n"
           "uniform vec2 translatePx;\n"
           "uniform float flipY;\n"
           "\n"
           "vec2 rotatePoint(vec2 p, float a) {\n"
           "  float c = cos(a);\n"
           "  float s = sin(a);\n"
           "  return vec2(c * p.x - s * p.y, s * p.x + c * p.y);\n"
           "}\n"
           "\n"
           "float mirror01(float v) {\n"
           "  float t = fract(v);\n"
           "  return 1.0 - abs(t * 2.0 - 1.0);\n"
           "}\n"
           "\n"
           "vec2 mirrorRepeat(vec2 v) {\n"
           "  return vec2(mirror01(v.x), mirror01(v.y));\n"
           "}\n"
           "\n"
           "vec2 samplePattern(vec2 p, vec4 pat, float angle) {\n"
           "  vec2 exey = pat.zw;\n"
           "  vec2 q = rotatePoint(p, -angle);\n"
           "  vec2 origin = pat.xy + vec2(exey.x < 0.0 ? -translatePx.x : "
           "translatePx.x,\n"
           "                               exey.y < 0.0 ? -translatePx.y : "
           "translatePx.y);\n"
           "  return fract((q - origin) / exey);\n"
           "}\n"
           "\n"
           "void main() {\n"
           "  float w = panelSize.x;\n"
           "  float h = panelSize.y;\n"
           "  float hw = w * 0.5;\n"
           "  float hh = h * 0.5;\n"
           "  vec2 p = uv * panelSize - vec2(hw, hh);\n"
           "  vec4 pat = vec4(-hw, -hh, w, h);\n"
           "  bool hFlip = false;\n"
           "  bool vFlip = false;\n"
           "\n"
           "  if (mode >= 6) {\n"
           "    vec2 px = rotatePoint(p + translatePx, -rotRad);\n"
           "    vec2 q = px / panelSize + vec2(0.5, 0.5);\n"
           "    vec2 srcUv;\n"
           "    if (mode == 6) {\n"
           "      srcUv = vec2(mirror01(q.x), fract(q.y));\n"
           "    } else if (mode == 7) {\n"
           "      srcUv = vec2(fract(q.x), mirror01(q.y));\n"
           "    } else if (mode == 8) {\n"
           "      srcUv = mirrorRepeat(q);\n"
           "    } else if (mode == 9) {\n"
           "      srcUv = mirrorRepeat(vec2(q.x + q.y, q.y - q.x));\n"
           "    } else if (mode == 10) {\n"
           "      srcUv = mirrorRepeat(vec2(q.x * 2.0 + q.y,\n"
           "                                q.y * 2.0 - q.x));\n"
           "    } else if (mode == 11) {\n"
           "      vec2 warp = mirrorRepeat(q) - vec2(0.5, 0.5);\n"
           "      srcUv = mirrorRepeat(vec2(q.x + abs(warp.y) * 1.5,\n"
           "                                q.y + abs(warp.x) * 1.5));\n"
           "    } else {\n"
           "      srcUv = mirrorRepeat(vec2(q.x * 2.0 + q.y * 2.0,\n"
           "                                q.y * 2.0 - q.x * 2.0));\n"
           "    }\n"
           "    float sampleY = (flipY > 0.0) ? (1.0 - srcUv.y) : srcUv.y;\n"
           "    gl_FragColor = texture2D(tex, vec2(srcUv.x, sampleY));\n"
           "    return;\n"
           "  }\n"
           "\n"
           "  if (mode == 1) {\n"
           "    if (p.x >= 0.0) { pat = vec4(hw, -hh, -w, h); hFlip = true; }\n"
           "  } else if (mode == 2) {\n"
           "    if (p.y >= 0.0) { pat = vec4(-hw, hh, w, -h); vFlip = true; }\n"
           "  } else if (mode == 3) {\n"
           "    if (p.x >= 0.0 && p.y < 0.0) { pat = vec4(hw, -hh, -w, h); "
           "hFlip = true; }\n"
           "    else if (p.x < 0.0 && p.y >= 0.0) { pat = vec4(-hw, hh, w, "
           "-h); vFlip = true; }\n"
           "    else if (p.x >= 0.0 && p.y >= 0.0) { pat = vec4(hw, hh, -w, "
           "-h); hFlip = true; vFlip = true; }\n"
           "  } else if (mode == 4) {\n"
           "    float hw2 = hw * 0.5; float hh2 = hh * 0.5;\n"
           "    pat = vec4(-hw2, -hh2, w * 0.5, h * 0.5);\n"
           "    if (p.x >= 0.0 && p.y < 0.0) { pat = vec4(hw2, -hh2, -w * "
           "0.5, h * 0.5); hFlip = true; }\n"
           "    else if (p.x < 0.0 && p.y >= 0.0) { pat = vec4(-hw2, hh2, w * "
           "0.5, -h * 0.5); vFlip = true; }\n"
           "    else if (p.x >= 0.0 && p.y >= 0.0) { pat = vec4(hw2, hh2, -w "
           "* 0.5, -h * 0.5); hFlip = true; vFlip = true; }\n"
           "  } else if (mode == 5) {\n"
           "    float qw = hw * 0.5; float qh = hh * 0.5;\n"
           "    pat = vec4(-qw, -qh, w * 0.25, h * 0.25);\n"
           "    if (p.x >= 0.0 && p.y < 0.0) { pat = vec4(qw, -qh, -w * 0.25, "
           "h * 0.25); hFlip = true; }\n"
           "    else if (p.x < 0.0 && p.y >= 0.0) { pat = vec4(-qw, qh, w * "
           "0.25, -h * 0.25); vFlip = true; }\n"
           "    else if (p.x >= 0.0 && p.y >= 0.0) { pat = vec4(qw, qh, -w * "
           "0.25, -h * 0.25); hFlip = true; vFlip = true; }\n"
           "  } else if (mode == 6) {\n"
           "    float qw = hw * 0.5;\n"
           "    if (p.x >= -hw + qw && p.x < 0.0) { pat = vec4(hw, -hh, -w, "
           "h); hFlip = true; }\n"
           "    else if (p.x >= qw) { pat = vec4(hw, -hh, -w, h); hFlip = "
           "true; }\n"
           "  } else if (mode == 7) {\n"
           "    float qh = hh * 0.5;\n"
           "    if (p.y >= -hh + qh && p.y < 0.0) { pat = vec4(-hw, hh, w, "
           "-h); vFlip = true; }\n"
           "    else if (p.y >= qh) { pat = vec4(-hw, hh, w, -h); vFlip = "
           "true; }\n"
           "  } else if (mode == 8) {\n"
           "    float tw = w / 3.0;\n"
           "    float x0 = -hw; float x2 = -hw + 2.0 * tw;\n"
           "    if (p.x >= x0 + tw && p.x < x2) { pat = vec4(x2 + tw, -hh, "
           "-w, h); hFlip = true; }\n"
           "    else { pat = vec4(x0, -hh, w, h); }\n"
           "  } else if (mode == 9) {\n"
           "    float th = h / 3.0;\n"
           "    float y0 = -hh; float y2 = -hh + 2.0 * th;\n"
           "    if (p.y >= y0 + th && p.y < y2) { pat = vec4(-hw, y2 + th, w, "
           "-h); vFlip = true; }\n"
           "    else { pat = vec4(-hw, y0, w, h); }\n"
           "  } else if (mode == 10) {\n"
           "    float qw = hw * 0.5;\n"
           "    bool rightHalf = p.x >= 0.0;\n"
           "    bool bottomHalf = p.y >= 0.0;\n"
           "    bool innerRight = mod(p.x + hw, qw * 2.0) >= qw;\n"
           "    hFlip = innerRight != rightHalf;\n"
           "    vFlip = bottomHalf;\n"
           "    pat = vec4(hFlip ? hw : -hw, vFlip ? hh : -hh, hFlip ? -w : "
           "w, vFlip ? -h : h);\n"
           "  } else if (mode == 11) {\n"
           "    if (p.x < 0.0 && p.y < 0.0) { pat = vec4(hw, -hh, -w, h); "
           "hFlip = true; }\n"
           "    else if (p.x >= 0.0 && p.y < 0.0) { pat = vec4(0.0, -hh, w, "
           "h); }\n"
           "    else if (p.x < 0.0 && p.y >= 0.0) { pat = vec4(hw, hh, -w, "
           "-h); hFlip = true; vFlip = true; }\n"
           "    else { pat = vec4(0.0, hh, w, -h); vFlip = true; }\n"
           "  } else {\n"
           "    if (p.x < 0.0 && p.y < 0.0) { pat = vec4(hw, hh, -w, -h); "
           "hFlip = true; vFlip = true; }\n"
           "    else if (p.x >= 0.0 && p.y < 0.0) { pat = vec4(0.0, hh, w, "
           "-h); vFlip = true; }\n"
           "    else if (p.x < 0.0 && p.y >= 0.0) { pat = vec4(hw, -hh, -w, "
           "h); hFlip = true; }\n"
           "    else { pat = vec4(0.0, -hh, w, h); }\n"
           "  }\n"
           "\n"
           "  float angle = (hFlip != vFlip) ? -rotRad : rotRad;\n"
           "  vec2 srcUv = samplePattern(p, pat, angle);\n"
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
      DEBUG("ClassicKaleidFBO shader compile error: %s", log);
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
      DEBUG("ClassicKaleidFBO link error: %s", log);
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
      uMode = glGetUniformLocation(program, "mode");
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

  int apply(NVGcontext* vg, GLuint srcTex, int outW, int outH, int mode,
            float rotDeg, float txOff, float tyOff, bool flipInputUV = false) {
    if (srcTex == 0 || mode < 1) return -1;
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
    glUniform1i(uMode, mode);
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

  ~ClassicKaleidFBO() = default;
};
