#pragma once
#include <nanovg_gl.h>

#include "rack.hpp"
using namespace rack;

// ── ScreenCapture
// ───────────────────────────────────────────────────────────── Grabs the
// actual OpenGL framebuffer every frame and stores it as an NVG image. Calling
// drawInPanel() re-renders that snapshot into an arbitrary rectangle — creating
// a one-frame-delayed feedback loop.
//
// The capture happens mid-frame (inside drawLayer), so it picks up all module
// panels that have already been drawn in this pass, plus everything we drew
// in previous passes. That's the feedback.
//
// GL notes:
//  - glCopyTexSubImage2D stays entirely on the GPU (no readback to CPU).
//  - We save/restore GL_TEXTURE_BINDING_2D around our calls so NVG's texture
//    state is undisturbed.
//  - NVG_IMAGE_FLIPY corrects for OpenGL's Y-up vs NVG's Y-down convention.

struct ScreenCapture {
  GLuint texId = 0;
  int nvgImg = -1;
  int lastFbW = 0;
  int lastFbH = 0;

  // Call once per drawLayer pass before drawing the panel image.
  // vg must be the active NVGcontext.
  void capture(NVGcontext* vg) {
    int fbW, fbH;
    glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);
    if (fbW <= 0 || fbH <= 0) return;

    bool needsRebuild = (texId == 0 || fbW != lastFbW || fbH != lastFbH);
    if (needsRebuild) {
      // Release old resources
      if (nvgImg >= 0) {
        nvgDeleteImage(vg, nvgImg);
        nvgImg = -1;
      }
      if (texId) {
        glDeleteTextures(1, &texId);
        texId = 0;
      }

      // Allocate a new GL texture at the current framebuffer resolution
      glGenTextures(1, &texId);
      GLint prevTex = 0;
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
      glBindTexture(GL_TEXTURE_2D, texId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      // Allocate storage (no pixel data yet)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbW, fbH, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
      glBindTexture(GL_TEXTURE_2D, prevTex);

      // Wrap the GL texture in an NVG image handle.
      // NVG_IMAGE_FLIPY flips Y on render, correcting GL's bottom-left origin.
      nvgImg =
          nvglCreateImageFromHandleGL2(vg, texId, fbW, fbH, NVG_IMAGE_FLIPY);
      lastFbW = fbW;
      lastFbH = fbH;
    }

    // Copy the current framebuffer into our texture (GPU-only blit)
    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glBindTexture(GL_TEXTURE_2D, texId);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fbW, fbH);
    glBindTexture(GL_TEXTURE_2D, prevTex);
  }

  // Draw the captured frame into `rect` in the current NVG coordinate space.
  // Call after capture().  alpha 0-1.
  void drawInPanel(NVGcontext* vg, math::Vec pos, math::Vec size, float alpha) {
    if (nvgImg < 0 || alpha <= 0.f) return;
    NVGpaint p =
        nvgImagePattern(vg, pos.x, pos.y, size.x, size.y, 0.f, nvgImg, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, size.y);
    nvgFillPaint(vg, p);
    nvgFill(vg);
  }

  // Does NOT free GPU resources — safe to call from a destructor that may
  // run without an active GL context. Rack will reclaim VRAM on exit.
  ~ScreenCapture() = default;
};
