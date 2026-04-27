#pragma once
#include <nanovg_gl.h>

#include "RackModuleSource.hpp"
#include "rack.hpp"

using namespace rack;

struct PortaloofTextureCopyFBO {
  GLuint fbo = 0;
  GLuint outTex = 0;
  GLuint program = 0;
  GLuint vbo = 0;
  int nvgImg = -1;
  int lastW = 0, lastH = 0;

  GLint uTex = -1;
  GLint uFlipY = -1;
  GLint aPos = -1;

  bool initialized = false;

  static const char* vertSrc() {
    return "#version 120\n"
           "attribute vec2 pos;\n"
           "varying vec2 uv;\n"
           "uniform float flipY;\n"
           "void main() {\n"
           "  gl_Position = vec4(pos, 0.0, 1.0);\n"
           "  uv = vec2(pos.x * 0.5 + 0.5, pos.y * flipY * 0.5 + 0.5);\n"
           "}\n";
  }

  static const char* fragSrc() {
    return "#version 120\n"
           "varying vec2 uv;\n"
           "uniform sampler2D tex;\n"
           "void main() {\n"
           "  gl_FragColor = texture2D(tex, uv);\n"
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
      DEBUG("PortaloofTextureCopyFBO shader compile error: %s", log);
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
      DEBUG("PortaloofTextureCopyFBO link error: %s", log);
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

  int apply(NVGcontext* vg, GLuint srcTex, int w, int h,
            bool flipInputUV = false) {
    if (!srcTex || w <= 0 || h <= 0) return -1;
    if (!initialized) init();
    if (!program || aPos < 0) return -1;

    resizeIfNeeded(vg, w, h);
    if (nvgImg < 0 || !outTex) return -1;

    GLint prevDrawFBO = 0, prevProg = 0, prevVBO = 0;
    GLint prevViewport[4] = {};
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
    glViewport(0, 0, w, h);
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(aPos);
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          nullptr);

    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(uTex, 0);
    glUniform1f(uFlipY, flipInputUV ? -1.0f : 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(aPos);

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

  PortaloofInjectedSource getSource() const {
    PortaloofInjectedSource out;
    out.nvgImg = nvgImg;
    out.texId = outTex;
    out.flipInputUV = false;
    return out;
  }
};
