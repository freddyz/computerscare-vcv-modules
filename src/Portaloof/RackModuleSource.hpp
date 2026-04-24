#pragma once

#include <nanovg_gl.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "rack.hpp"

using namespace rack;

struct PortaloofInjectedSource {
  GLuint texId = 0;
  int nvgImg = -1;
  bool flipInputUV = false;

  bool isValid() const { return texId != 0 && nvgImg >= 0; }
};

struct PortaloofRackModuleSource {
  int64_t moduleId = -1;

  struct CacheEntry {
    NVGLUframebuffer* fbs[2] = {};
    math::Vec fbSize;
    int frontIndex = 0;
    bool hasFront = false;
    bool rendering = false;
  };

  static std::unordered_map<int64_t, CacheEntry>& cache() {
    static std::unordered_map<int64_t, CacheEntry> entries;
    return entries;
  }

  bool hasSource() const { return moduleId >= 0; }

  void clear() { moduleId = -1; }

  void setModule(int64_t id) { moduleId = id; }

  app::ModuleWidget* getTargetModule(int64_t selfModuleId = -1) const {
    if (!hasSource() || !APP || !APP->scene || !APP->scene->rack)
      return nullptr;
    app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    if (!mw || !mw->module) return nullptr;
    if (mw->module->id == selfModuleId) return nullptr;
    return mw;
  }

  std::string describe(int64_t selfModuleId = -1) const {
    app::ModuleWidget* mw = getTargetModule(selfModuleId);
    if (!mw) return hasSource() ? "Missing module" : "None";
    std::string name = mw->model ? mw->model->name : "Module";
    return string::f("%s (#%lld)", name.c_str(), (long long)mw->module->id);
  }

  std::vector<app::ModuleWidget*> getSelectableModules(
      int64_t selfModuleId = -1) const {
    std::vector<app::ModuleWidget*> modules;
    if (!APP || !APP->scene || !APP->scene->rack) return modules;
    modules = APP->scene->rack->getModules();
    modules.erase(std::remove_if(modules.begin(), modules.end(),
                                 [=](app::ModuleWidget* mw) {
                                   return !mw || !mw->module ||
                                          mw->module->id == selfModuleId;
                                 }),
                  modules.end());
    std::sort(modules.begin(), modules.end(),
              [](const app::ModuleWidget* a, const app::ModuleWidget* b) {
                if (a->box.pos.y != b->box.pos.y)
                  return a->box.pos.y < b->box.pos.y;
                if (a->box.pos.x != b->box.pos.x)
                  return a->box.pos.x < b->box.pos.x;
                return a->module->id < b->module->id;
              });
    return modules;
  }

  PortaloofInjectedSource render(NVGcontext* vg, int64_t selfModuleId = -1) {
    PortaloofInjectedSource out;
    app::ModuleWidget* mw = getTargetModule(selfModuleId);
    if (!mw) return out;

    math::Vec moduleSize = mw->box.size;
    if (!moduleSize.isFinite() || moduleSize.x <= 1.f || moduleSize.y <= 1.f)
      return out;

    CacheEntry& entry = cache()[mw->module->id];

    // Mutual Portaloof routing can recurse (A renders B while B renders A).
    // When that happens, return the previously completed frame instead of
    // rendering live again.
    if (entry.rendering) return getFrontSource(vg, entry);

    float pixelRatio = std::max(1.f, std::floor(APP->window->pixelRatio));
    math::Vec newFbSize = moduleSize.mult(pixelRatio).ceil();
    ensureFramebuffers(vg, entry, newFbSize);
    if (!entry.fbs[0] || !entry.fbs[1]) return getFrontSource(vg, entry);

    int backIndex = 1 - entry.frontIndex;
    NVGLUframebuffer* backFb = entry.fbs[backIndex];
    if (!backFb) return getFrontSource(vg, entry);

    GLint prevDrawFBO = 0;
    GLint prevViewport[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    entry.rendering = true;

    nvgluBindFramebuffer(backFb);
    glViewport(0, 0, (GLsizei)newFbSize.x, (GLsizei)newFbSize.y);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    NVGcontext* fbVg = APP->window->fbVg;
    nvgBeginFrame(fbVg, moduleSize.x, moduleSize.y, pixelRatio);
    {
      widget::Widget::DrawArgs args;
      args.vg = fbVg;
      args.fb = backFb;
      args.clipBox = math::Rect(math::Vec(0.f, 0.f), moduleSize);
      mw->draw(args);
      mw->drawLayer(args, 1);
    }
    nvgEndFrame(fbVg);
    nvgReset(fbVg);

    nvgluBindFramebuffer(nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, prevDrawFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
               prevViewport[3]);

    entry.frontIndex = backIndex;
    entry.hasFront = true;
    entry.rendering = false;
    return getFrontSource(vg, entry);
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
