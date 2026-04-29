#pragma once

#include <cmath>

#include "../Computerscare.hpp"
#include "NudiebugPlotDots.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

struct PlotDisplay {
  NVGLUframebuffer* framebuffer = nullptr;
  int lastPixelWidth = 0;
  int lastPixelHeight = 0;

  PlotDotsRenderer dotsRenderer;

  ~PlotDisplay() { destroyFramebuffer(); }

  void destroyFramebuffer() {
    if (!framebuffer) return;
    nvgluDeleteFramebuffer(framebuffer);
    framebuffer = nullptr;
    lastPixelWidth = 0;
    lastPixelHeight = 0;
  }

  float pixelRatio() const {
    if (!APP || !APP->window) return 1.f;
    return std::max(1.f, std::floor(APP->window->pixelRatio));
  }

  void resizeIfNeeded(NVGcontext* vg, float width, float height) {
    const float ratio = pixelRatio();
    const int pixelWidth =
        std::max(1, static_cast<int>(std::ceil(width * ratio)));
    const int pixelHeight =
        std::max(1, static_cast<int>(std::ceil(height * ratio)));

    if (framebuffer && pixelWidth == lastPixelWidth &&
        pixelHeight == lastPixelHeight) {
      return;
    }

    destroyFramebuffer();
    framebuffer = nvgluCreateFramebuffer(vg, pixelWidth, pixelHeight, 0);
    lastPixelWidth = framebuffer ? pixelWidth : 0;
    lastPixelHeight = framebuffer ? pixelHeight : 0;

    if (!framebuffer) return;

    GLint previousFbo = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFbo);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    nvgluBindFramebuffer(framebuffer);
    glViewport(0, 0, lastPixelWidth, lastPixelHeight);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2],
               previousViewport[3]);
  }

  void render(NVGcontext* vg, const Snapshot& snapshot,
              const DisplayOptions& options, float width, float height) {
    if (width <= 0.f || height <= 0.f || options.plotMode == PLOT_OFF) return;
    if (!APP || !APP->window || !APP->window->fbVg) return;

    resizeIfNeeded(vg, width, height);
    if (!framebuffer) return;

    NVGcontext* fbVg = APP->window->fbVg;
    const float ratio = pixelRatio();

    GLint previousFbo = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFbo);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    nvgluBindFramebuffer(framebuffer);
    nvgBeginFrame(fbVg, width, height, ratio);

    if (options.plotMode == PLOT_DOTS) {
      dotsRenderer.draw(fbVg, snapshot, width, height);
    }

    glViewport(0, 0, lastPixelWidth, lastPixelHeight);
    if (options.clearPlotPerFrame) {
      glClearColor(0.f, 0.f, 0.f, 0.f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
              GL_STENCIL_BUFFER_BIT);
    }
    nvgEndFrame(fbVg);
    nvgReset(fbVg);

    glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2],
               previousViewport[3]);
  }

  void draw(NVGcontext* vg, float width, float height) {
    if (!framebuffer || framebuffer->image < 0) return;

    nvgBeginPath(vg);
    nvgRect(vg, 0.f, 0.f, width, height);
    nvgFillPaint(vg, nvgImagePattern(vg, 0.f, 0.f, width, height, 0.f,
                                     framebuffer->image, 1.f));
    nvgFill(vg);
  }
};

}  // namespace nudiebug
