#pragma once

#include <nanovg_gl.h>

#include "RackModuleSource.hpp"
#include "rack.hpp"

using namespace rack;

struct PortaloofLayerFBO {
  NVGLUframebuffer* fb = nullptr;
  math::Vec fbSize;
  int lastW = 0;
  int lastH = 0;
  GLint prevFBO = 0;
  GLint prevViewport[4] = {};

  bool resizeIfNeeded(NVGcontext* vg, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (fb && w == lastW && h == lastH) return true;

    if (fb) {
      nvgluDeleteFramebuffer(fb);
      fb = nullptr;
    }
    fb = nvgluCreateFramebuffer(vg, w, h, 0);
    if (!fb) {
      lastW = 0;
      lastH = 0;
      return false;
    }
    lastW = w;
    lastH = h;
    fbSize = math::Vec(w, h);
    return true;
  }

  NVGcontext* begin(NVGcontext* vg, float logicalW, float logicalH,
                    float pixelRatio) {
    int pxW = std::max((int)std::lround(logicalW * pixelRatio), 1);
    int pxH = std::max((int)std::lround(logicalH * pixelRatio), 1);
    if (!resizeIfNeeded(vg, pxW, pxH)) return nullptr;

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    nvgluBindFramebuffer(fb);
    glViewport(0, 0, pxW, pxH);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    NVGcontext* fbVg = APP->window->fbVg;
    nvgBeginFrame(fbVg, logicalW, logicalH, pixelRatio);
    return fbVg;
  }

  void end() {
    NVGcontext* fbVg = APP->window->fbVg;
    nvgEndFrame(fbVg);
    nvgReset(fbVg);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
               prevViewport[3]);
  }

  PortaloofInjectedSource getSource(NVGcontext* vg) const {
    PortaloofInjectedSource out;
    if (!fb || fb->image < 0) return out;
    out.nvgImg = fb->image;
    out.texId = nvglImageHandleGL2(vg, fb->image);
    out.flipInputUV = false;
    return out;
  }

  ~PortaloofLayerFBO() {
    if (fb) nvgluDeleteFramebuffer(fb);
  }
};
