#pragma once

#include "PortaloofModule.hpp"

// ─── Backdrop Widget ─────────────────────────────────────────────────────────
// Inserted as the bottom-most child of APP->scene->rack so it draws behind
// all modules.  Uses the same rendering pipeline as the main widget but maps
// the effect across the full visible viewport.

struct PortaloofBackdropWidget : widget::Widget {
  ComputerscarePortaloof* module = nullptr;
  ScreenCapture screenCap;
  ColorTransformFBO colorFBOs[2];
  SourceBlendFBO sourceBlendFBO;
  FlowerKaleidFBO flowerKaleidFBOs[2];
  PortaloofRackModuleSource rackSources[2];
  PortaloofRectSource rectSources[2];
  bool cachedRowEnabled[2][10] = {};
  float cachedRowValue[2][10] = {};
  bool hasValidCache = false;

  // Loaded image injection
  std::string lastImagePath[2];
  int loadedNvgImg[2] = {-1, -1};
  GLuint loadedTexId[2] = {0, 0};

  PortaloofBackdropWidget() {
    box.pos = math::Vec(0.f, 0.f);
    box.size = math::Vec(INFINITY, INFINITY);
  }

  void draw(const DrawArgs& args) override {
    if (!module || portaloofRenderingRackRectSource()) return;

    float vpW = args.clipBox.size.x;
    float vpH = args.clipBox.size.y;
    float vpX = args.clipBox.pos.x;
    float vpY = args.clipBox.pos.y;
    if (vpW <= 1.f || vpH <= 1.f) return;

    int fbW, fbH;
    glfwGetFramebufferSize(APP->window->win, &fbW, &fbH);

    bool doCapture = !module->freezeMode ||
                     module->backdropCapturePending.exchange(false) ||
                     !hasValidCache;
    if (doCapture) {
      screenCap.capture(args.vg);
      memcpy(cachedRowEnabled, module->sourceRowEnabled,
             sizeof(cachedRowEnabled));
      memcpy(cachedRowValue, module->sourceRowValue, sizeof(cachedRowValue));
      hasValidCache = true;
    }

    PortaloofInjectedSource renderSources[2] = {
        getSource(args.vg, screenCap, 0), getSource(args.vg, screenCap, 1)};
    bool hasSource1 = renderSources[0].isValid();
    bool hasSource2 = renderSources[1].isValid();
    if (!doCapture && !hasSource1 && !hasSource2) return;
    if (!hasSource1 && !hasSource2) return;
    float source2Amt = 0.5f * (1.f + module->inputSourceMix);

    // Transform post: use live params (knobs alter frozen image).
    // Transform pre:  use cached params (knobs only affect next snapshot).
    bool useLive = !module->freezeMode || module->transformPost;

    float baseAlpha =
        module->params[ComputerscarePortaloof::BACKDROP_ALPHA].getValue();
    float imgW = vpW;
    float imgHW = imgW * 0.5f;
    float hh = vpH * 0.5f;

    for (int renderSourceIndex = 0; renderSourceIndex < 2;
         renderSourceIndex++) {
      PortaloofInjectedSource currentSource = renderSources[renderSourceIndex];
      if (!currentSource.isValid()) continue;
      float sourceMixAlpha = 1.f;
      if (hasSource1 && hasSource2)
        sourceMixAlpha =
            renderSourceIndex == 0 ? (1.f - source2Amt) : source2Amt;
      if (sourceMixAlpha <= 0.f) continue;
      float alpha = baseAlpha * sourceMixAlpha;

      bool* en = (useLive || !hasValidCache)
                     ? module->sourceRowEnabled[renderSourceIndex]
                     : cachedRowEnabled[renderSourceIndex];
      float* rv = (useLive || !hasValidCache)
                      ? module->sourceRowValue[renderSourceIndex]
                      : cachedRowValue[renderSourceIndex];

      bool scaleOn = en[0], scaleXOn = en[1], scaleYOn = en[2];
      bool rotOn = en[3], kaliOn = en[4];
      bool txOn = en[5], tyOn = en[6];
      bool hueOn = en[7], invertOn = en[8], curvesOn = en[9];

      float scaleV = rv[0], scaleXV = rv[1], scaleYV = rv[2];
      float rotV = rv[3], kaliV = rv[4];
      float txV = rv[5], tyV = rv[6];
      float hueV = hueOn ? rv[7] : 0.f;
      float foldFreqV = invertOn ? (1.0f + rv[8] * 3.0f) : 1.0f;
      float warpV = curvesOn ? rv[9] : 0.f;

      GLuint srcTex = currentSource.texId;
      int srcImg = currentSource.nvgImg;
      bool flipInputUV = currentSource.flipInputUV;

      float sx = (scaleOn ? scaleV : 1.f) * (scaleXOn ? scaleXV : 1.f);
      float sy = (scaleOn ? scaleV : 1.f) * (scaleYOn ? scaleYV : 1.f);
      float txLocal = txOn ? txV * imgW : 0.f;
      float tyLocal = tyOn ? tyV * vpH : 0.f;

      int img = colorFBOs[renderSourceIndex].apply(args.vg, srcTex, srcImg, fbW,
                                                   fbH, hueV, warpV, foldFreqV,
                                                   flipInputUV);
      GLuint effectTex = (img == colorFBOs[renderSourceIndex].nvgImg &&
                          colorFBOs[renderSourceIndex].outTex != 0)
                             ? colorFBOs[renderSourceIndex].outTex
                             : srcTex;

      nvgSave(args.vg);
      nvgScissor(args.vg, vpX, vpY, vpW, vpH);
      nvgTranslate(args.vg, vpX + imgHW, vpY + hh);
      if (sx != 1.f || sy != 1.f) nvgScale(args.vg, sx, sy);

      bool tileOn = module->tileEmptySpace;
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
          if (module->translateFirst) {
            kaliTxOff = txLocal;
            kaliTyOff = tyLocal;
          } else {
            nvgTx = txOn ? (2.f * txLocal) : 0.f;
            nvgTy = tyOn ? (2.f * tyLocal) : 0.f;
            nvgTranslate(args.vg, nvgTx, nvgTy);
          }
        }
        int flowerTargetW = flowerKaleidTargetDim(imgW, renderScale, fbW);
        int flowerTargetH = flowerKaleidTargetDim(vpH, renderScale, fbH);
        float flowerScaleX = (imgW > 0.f) ? ((float)flowerTargetW / imgW) : 1.f;
        float flowerScaleY = (vpH > 0.f) ? ((float)flowerTargetH / vpH) : 1.f;
        int flowerImg = flowerKaleidFBOs[renderSourceIndex].apply(
            args.vg, effectTex, flowerTargetW, flowerTargetH, kaliSegments,
            rotOn ? rotV : 0.f, kaliTxOff * flowerScaleX,
            kaliTyOff * flowerScaleY, flipInputUV);
        if (flowerImg >= 0) {
          if (tileOn) {
            float pcx = -(txOn && !module->translateFirst ? nvgTx : 0.f);
            float pcy = -(tyOn && !module->translateFirst ? nvgTy : 0.f);
            float dispHW = imgHW / absSx;
            float dispHH = hh / absSy;
            int iMin = (int)ceilf((pcx - dispHW - imgHW) / imgW) - 1;
            int iMax = (int)floorf((pcx + dispHW + imgHW) / imgW) + 1;
            int jMin = (int)ceilf((pcy - dispHH - hh) / vpH) - 1;
            int jMax = (int)floorf((pcy + dispHH + hh) / vpH) + 1;
            iMin = std::max(iMin, -20);
            iMax = std::min(iMax, 20);
            jMin = std::max(jMin, -20);
            jMax = std::min(jMax, 20);
            for (int j = jMin; j <= jMax; j++) {
              for (int i = iMin; i <= iMax; i++) {
                float tileX = (float)i * imgW;
                float tileY = (float)j * vpH;
                bool reverseX = reverseOuterTile(i);
                bool reverseY = reverseOuterTile(j);
                nvgSave(args.vg);
                nvgTranslate(args.vg, tileX, tileY);
                if (reverseX || reverseY) {
                  nvgScale(args.vg, reverseX ? -1.f : 1.f,
                           reverseY ? -1.f : 1.f);
                }
                NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH,
                                             0.f, flowerImg, alpha);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, -imgHW, -hh, imgW, vpH);
                nvgFillPaint(args.vg, p);
                nvgFill(args.vg);
                nvgRestore(args.vg);
              }
            }
          } else {
            NVGpaint p = nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH, 0.f,
                                         flowerImg, alpha);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, -imgHW, -hh, imgW, vpH);
            nvgFillPaint(args.vg, p);
            nvgFill(args.vg);
          }
        }
      } else if (kaliMode < 0) {
        float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
        float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;

        float kaliTxOff = 0.f, kaliTyOff = 0.f;
        float nvgTx = 0.f, nvgTy = 0.f;
        if (txOn || tyOn) {
          if (module->translateFirst) {
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
        float rHW =
            ((imgW + 2.f * effTxAbs) * cosA + (vpH + 2.f * effTyAbs) * sinA) /
                (2.f * absSx) +
            4.f;
        float rHH =
            ((imgW + 2.f * effTxAbs) * sinA + (vpH + 2.f * effTyAbs) * cosA) /
                (2.f * absSy) +
            4.f;
        float pcx = -(txOn && !module->translateFirst ? nvgTx : 0.f);
        float pcy = -(tyOn && !module->translateFirst ? nvgTy : 0.f);
        float dispHW = imgHW / absSx;
        float dispHH = hh / absSy;

        if (tileOn) {
          int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW);
          int iMax = (int)floorf((pcx + rHW + imgHW) / imgW);
          int jMin = (int)ceilf((pcy - rHH - hh) / vpH);
          int jMax = (int)floorf((pcy + rHH + hh) / vpH);
          iMin = std::max(iMin, -20);
          iMax = std::min(iMax, 20);
          jMin = std::max(jMin, -20);
          jMax = std::min(jMax, 20);
          for (int j = jMin; j <= jMax; j++) {
            for (int i = iMin; i <= iMax; i++) {
              float tileX = (float)i * imgW;
              float tileY = (float)j * vpH;
              bool reverseX = reverseOuterTile(i);
              bool reverseY = reverseOuterTile(j);
              nvgSave(args.vg);
              nvgTranslate(args.vg, tileX, tileY);
              if (reverseX || reverseY) {
                nvgScale(args.vg, reverseX ? -1.f : 1.f, reverseY ? -1.f : 1.f);
              }
              float dX = outerTileDisplayMin(pcx, dispHW, tileX, reverseX);
              float dY = outerTileDisplayMin(pcy, dispHH, tileY, reverseY);
              drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH,
                               kaliSegments, alpha, rotOn, rotV, false, 0.f,
                               false, dX, dY, 2.f * dispHW, 2.f * dispHH,
                               kaliTxOff, kaliTyOff);
              nvgRestore(args.vg);
            }
          }
        } else {
          drawKaleidoscope(args.vg, img, imgHW, hh, imgW, vpH, rHW, rHH,
                           kaliSegments, alpha, rotOn, rotV, false, 0.f, false,
                           pcx - dispHW, pcy - dispHH, 2.f * dispHW,
                           2.f * dispHH, kaliTxOff, kaliTyOff);
        }
      } else {
        float cosA = rotOn ? fabsf(cosf(rotV * (float)M_PI / 180.f)) : 1.f;
        float sinA = rotOn ? fabsf(sinf(rotV * (float)M_PI / 180.f)) : 0.f;
        if (rotOn) applyRotation(args.vg, rotV);
        float txOffset = txOn ? txLocal : 0.f;
        float tyOffset = tyOn ? tyLocal : 0.f;
        if (txOn || tyOn) nvgTranslate(args.vg, txOffset, tyOffset);
        float txOffsetAbs = fabsf(txOffset);
        float tyOffsetAbs = fabsf(tyOffset);
        float rHW = ((imgW + 2.f * txOffsetAbs) * cosA +
                     (vpH + 2.f * tyOffsetAbs) * sinA) /
                        (2.f * absSx) +
                    4.f;
        float rHH = ((imgW + 2.f * txOffsetAbs) * sinA +
                     (vpH + 2.f * tyOffsetAbs) * cosA) /
                        (2.f * absSy) +
                    4.f;

        if (tileOn) {
          float pcx = -txOffset;
          float pcy = -tyOffset;
          int iMin = (int)ceilf((pcx - rHW - imgHW) / imgW);
          int iMax = (int)floorf((pcx + rHW + imgHW) / imgW);
          int jMin = (int)ceilf((pcy - rHH - hh) / vpH);
          int jMax = (int)floorf((pcy + rHH + hh) / vpH);
          iMin = std::max(iMin, -20);
          iMax = std::min(iMax, 20);
          jMin = std::max(jMin, -20);
          jMax = std::min(jMax, 20);
          for (int j = jMin; j <= jMax; j++) {
            for (int i = iMin; i <= iMax; i++) {
              float ox = -imgHW + i * imgW;
              float oy = -hh + j * vpH;
              NVGpaint p =
                  nvgImagePattern(args.vg, ox, oy, imgW, vpH, 0.f, img, alpha);
              nvgBeginPath(args.vg);
              nvgRect(args.vg, ox, oy, imgW, vpH);
              nvgFillPaint(args.vg, p);
              nvgFill(args.vg);
            }
          }
        } else {
          // Exact rect — no oversized fill so NVG won't tile/smear the edges.
          NVGpaint p =
              nvgImagePattern(args.vg, -imgHW, -hh, imgW, vpH, 0.f, img, alpha);
          nvgBeginPath(args.vg);
          nvgRect(args.vg, -imgHW, -hh, imgW, vpH);
          nvgFillPaint(args.vg, p);
          nvgFill(args.vg);
        }
      }

      nvgRestore(args.vg);
    }
  }

  PortaloofInjectedSource getSource(NVGcontext* vg,
                                    const ScreenCapture& capture, int index) {
    PortaloofInjectedSource out;
    if (!module || index < 0 || index >= 2) return out;
    const PortaloofSourceConfig& source = module->sources[index];
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
        return rackSources[index].render(vg, module->id);
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
};
