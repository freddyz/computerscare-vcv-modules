#pragma once
#include <nanovg_gl.h>

#include "RackModuleSource.hpp"
#include "rack.hpp"

using namespace rack;

struct SourceBlendFBO {
  GLuint fbo = 0;
  GLuint outTex = 0;
  GLuint program = 0;
  GLuint vbo = 0;
  int nvgImg = -1;
  int lastW = 0, lastH = 0;

  GLint uRackTex = -1;
  GLint uImageTex = -1;
  GLint uImageAmt = -1;
  GLint uBlendMode = -1;
  GLint uRackFlipY = -1;
  GLint uImageFlipY = -1;
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
           "uniform sampler2D rackTex;\n"
           "uniform sampler2D imageTex;\n"
           "uniform float imageAmt;\n"
           "uniform int blendMode;\n"
           "uniform float rackFlipY;\n"
           "uniform float imageFlipY;\n"
           "vec2 flippedUv(vec2 coord, float flipY) {\n"
           "  return vec2(coord.x, coord.y * flipY + (1.0 - flipY) * 0.5);\n"
           "}\n"
           "vec3 blendOverlay(vec3 b, vec3 t) {\n"
           "  vec3 low = 2.0 * b * t;\n"
           "  vec3 high = 1.0 - 2.0 * (1.0 - b) * (1.0 - t);\n"
           "  return mix(low, high, step(vec3(0.5), b));\n"
           "}\n"
           "vec3 blendColor(vec3 b, vec3 t, int mode) {\n"
           "  if (mode == 2) return min(b + t, 1.0);\n"
           "  if (mode == 4) return max(b - t, 0.0);\n"
           "  if (mode == 5) return b * t;\n"
           "  if (mode == 6) return 1.0 - (1.0 - b) * (1.0 - t);\n"
           "  if (mode == 7) return blendOverlay(b, t);\n"
           "  if (mode == 8) return abs(b - t);\n"
           "  if (mode == 9) return min(b, t);\n"
           "  if (mode == 10) return max(b, t);\n"
           "  return t;\n"
           "}\n"
           "void main() {\n"
           "  vec4 rackCol = texture2D(rackTex, flippedUv(uv, rackFlipY));\n"
           "  vec4 imageCol = texture2D(imageTex, flippedUv(uv, imageFlipY));\n"
           "  float amt = clamp(imageAmt, 0.0, 1.0);\n"
           "  if (blendMode == 0) {\n"
           "    gl_FragColor = mix(rackCol, imageCol, amt);\n"
           "    return;\n"
           "  }\n"
           "  float baseA = clamp(rackCol.a, 0.0, 1.0);\n"
           "  float topA = clamp(imageCol.a * amt, 0.0, 1.0);\n"
           "  vec3 baseRgb = baseA > 0.0 ? rackCol.rgb / baseA : vec3(0.0);\n"
           "  vec3 topRgb = imageCol.a > 0.0 ? imageCol.rgb / imageCol.a : vec3(0.0);\n"
           "  baseRgb = clamp(baseRgb, 0.0, 1.0);\n"
           "  topRgb = clamp(topRgb, 0.0, 1.0);\n"
           "  if (blendMode == 3) {\n"
           "    float outA = baseA * (1.0 - topA) + topA * (1.0 - baseA);\n"
           "    vec3 outRgb = (baseRgb * baseA * (1.0 - topA) + topRgb * topA * (1.0 - baseA)) / max(outA, 0.0001);\n"
           "    gl_FragColor = vec4(outRgb * outA, outA);\n"
           "    return;\n"
           "  }\n"
           "  vec3 blended = blendColor(baseRgb, topRgb, blendMode);\n"
           "  float outA = baseA + topA * (1.0 - baseA);\n"
           "  vec3 outRgb = (blended * topA + baseRgb * baseA * (1.0 - topA)) / max(outA, 0.0001);\n"
           "  gl_FragColor = vec4(outRgb * outA, outA);\n"
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
      DEBUG("SourceBlendFBO shader compile error: %s", log);
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
      DEBUG("SourceBlendFBO link error: %s", log);
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
      uRackTex = glGetUniformLocation(program, "rackTex");
      uImageTex = glGetUniformLocation(program, "imageTex");
      uImageAmt = glGetUniformLocation(program, "imageAmt");
      uBlendMode = glGetUniformLocation(program, "blendMode");
      uRackFlipY = glGetUniformLocation(program, "rackFlipY");
      uImageFlipY = glGetUniformLocation(program, "imageFlipY");
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

  int apply(NVGcontext* vg, GLuint rackTex, GLuint imageTex, int w, int h,
            float imageAmt, bool imageFlipY = true, bool rackFlipY = false,
            int blendMode = 0) {
    if (w <= 0 || h <= 0 || !rackTex || !imageTex) return -1;
    if (!initialized) init();
    if (!program || aPos < 0) return -1;

    resizeIfNeeded(vg, w, h);
    if (nvgImg < 0 || !outTex) return -1;

    GLint prevDrawFBO = 0, prevProg = 0, prevVBO = 0;
    GLint prevTex0 = 0, prevTex1 = 0, prevActiveTex = 0;
    GLint prevViewport[4] = {};
    GLboolean prevBlend = glIsEnabled(GL_BLEND);

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTex);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex1);
    glActiveTexture(prevActiveTex);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glUseProgram(program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rackTex);
    glUniform1i(uRackTex, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, imageTex);
    glUniform1i(uImageTex, 1);
    glUniform1f(uImageAmt, clamp(imageAmt, 0.f, 1.f));
    glUniform1i(uBlendMode, blendMode);
    glUniform1f(uRackFlipY, rackFlipY ? -1.f : 1.f);
    glUniform1f(uImageFlipY, imageFlipY ? -1.f : 1.f);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(aPos);
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(aPos);

    glBindFramebuffer(GL_FRAMEBUFFER, prevDrawFBO);
    glUseProgram(prevProg);
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
               prevViewport[3]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prevTex0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, prevTex1);
    glActiveTexture(prevActiveTex);
    if (prevBlend)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);

    return nvgImg;
  }

  PortaloofInjectedSource getSource() const {
    PortaloofInjectedSource out;
    out.nvgImg = nvgImg;
    out.texId = outTex;
    out.flipInputUV = false;
    return out;
  }

  ~SourceBlendFBO() = default;
};
