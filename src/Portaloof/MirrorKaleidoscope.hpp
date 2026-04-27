#pragma once
#include "MirrorFlip.hpp"
#include "MirrorRotation.hpp"
#include "rack.hpp"
using namespace rack;

// ── kSector
// ─────────────────────────────────────────────────────────────────── Clips to
// the intersection of the sector rect and the display bounds, then applies
// rotation/flip before painting the image pattern.
//
// We compute the intersection manually and call nvgScissor (not
// nvgIntersectScissor). nvgIntersectScissor can produce a zero/negative-extent
// scissor when the sector is outside the display; NanoVG's GLSL shader treats
// scissorExt <= 0 as "no clip", which bleeds outside module bounds.  Computing
// the intersection ourselves lets us return early if it is empty, guaranteeing
// a non-degenerate scissor.
//
// dspX/Y/W/H: display bounds in the CURRENT coordinate space (same space as
// clipX/Y/W/H, i.e. before rotation).  For a tile at offset (i,j) this is the
// display shifted by
// -(i*imgW, j*mirrorH) relative to the tile origin.
static inline void kSector(NVGcontext* vg, float clipX, float clipY,
                           float clipW, float clipH, float ox, float oy,
                           float ex, float ey, float coverHW, float coverHH,
                           int img, float alpha, bool rotOn, float rotDeg,
                           bool mirrOn, float mirrDeg, bool clipToImage,
                           float dspX, float dspY, float dspW, float dspH,
                           float txOff = 0.f, float tyOff = 0.f) {
  nvgSave(vg);
  // Manual intersection of sector clip with display bounds.
  float isX = std::max(clipX, dspX);
  float isY = std::max(clipY, dspY);
  float isW = std::min(clipX + clipW, dspX + dspW) - isX;
  float isH = std::min(clipY + clipH, dspY + dspH) - isY;
  if (isW <= 0.f || isH <= 0.f) {
    nvgRestore(vg);
    return;
  }
  nvgScissor(vg, isX, isY, isW, isH);
  if (rotOn) {
    bool hFlip = (ex < 0.f);
    bool vFlip = (ey < 0.f);
    float effectiveRot = (hFlip != vFlip) ? -rotDeg : rotDeg;
    applyRotation(vg, effectiveRot);
  }
  if (mirrOn) applyFlip(vg, mirrDeg);
  // Apply image-content offset with sign correction for flipped sectors so the
  // mirror boundary stays seamless: a flipped sector must shift in the opposite
  // NVG direction to show the same image content at the shared edge.
  float patOx = ox + (ex < 0.f ? -txOff : txOff);
  float patOy = oy + (ey < 0.f ? -tyOff : tyOff);
  float absEx = fabsf(ex);
  float absEy = fabsf(ey);

  if (!clipToImage && (txOff != 0.f || tyOff != 0.f)) {
    // Explicit tiling: draw enough image tiles to cover the full cover rect
    // without UV going outside [0,1].  This prevents GL_CLAMP_TO_EDGE smearing
    // when the pattern origin is shifted by txOff/tyOff (Translate > Kaleid
    // mode).  The scissor (already set above) clips each tile to the sector.
    //
    // For either sign of ex/ey we compute the UV range of the cover rect and
    // iterate over integer UV tiles that span it.  When ex < 0 the UV axis is
    // reversed so min/max are swapped — fabsf(uvMaxX - uvMinX) handles both.
    float uvMinX = std::min(((-coverHW) - patOx) / ex, (coverHW - patOx) / ex);
    float uvMaxX = std::max(((-coverHW) - patOx) / ex, (coverHW - patOx) / ex);
    float uvMinY = std::min(((-coverHH) - patOy) / ey, (coverHH - patOy) / ey);
    float uvMaxY = std::max(((-coverHH) - patOy) / ey, (coverHH - patOy) / ey);
    int k0 = (int)floorf(uvMinX);
    int numK = (int)ceilf(uvMaxX - uvMinX) + 2;
    int j0 = (int)floorf(uvMinY);
    int numJ = (int)ceilf(uvMaxY - uvMinY) + 2;
    numK = std::min(numK, 20);
    numJ = std::min(numJ, 20);
    for (int jt = 0; jt < numJ; jt++) {
      for (int it = 0; it < numK; it++) {
        float tileOx = patOx + (float)(k0 + it) * ex;
        float tileOy = patOy + (float)(j0 + jt) * ey;
        NVGpaint p =
            nvgImagePattern(vg, tileOx, tileOy, ex, ey, 0.f, img, alpha);
        nvgBeginPath(vg);
        float rx = ex < 0.f ? tileOx + ex : tileOx;
        float ry = ey < 0.f ? tileOy + ey : tileOy;
        nvgRect(vg, rx, ry, absEx, absEy);
        nvgFillPaint(vg, p);
        nvgFill(vg);
      }
    }
  } else {
    NVGpaint p = nvgImagePattern(vg, patOx, patOy, ex, ey, 0.f, img, alpha);
    nvgBeginPath(vg);
    if (clipToImage) {
      // Draw only the actual image rect to prevent the pattern from tiling into
      // empty space.
      float rx = ex < 0.f ? patOx + ex : patOx;
      float ry = ey < 0.f ? patOy + ey : patOy;
      nvgRect(vg, rx, ry, absEx, absEy);
    } else {
      // Large rect covers full panel even after rotation; used for tiling mode.
      nvgRect(vg, -coverHW, -coverHH, 2.f * coverHW, 2.f * coverHH);
    }
    nvgFillPaint(vg, p);
    nvgFill(vg);
  }
  nvgRestore(vg);
}

// ── drawKaleidoscope
// ────────────────────────────────────────────────────────── All coordinates in
// NVG space centered at the mirror panel center. hw/hh   = half-width/height of
// the mirror panel. w/h     = full width/height (= 2*hw, 2*hh). coverHW/HH =
// half-size of the circumscribed rect (covers panel at any angle). mode 1-12:
// see below. rotOn/rotDeg and mirrOn/mirrDeg are applied per-sector after
// sector clipping. dspX/Y/W/H: display bounds in the pre-rotation coordinate
// space of this call. For tile (i,j) this is (pcx - dispHW - i*imgW, pcy -
// dispHH - j*mirrorH, 2*dispHW, 2*dispHH).
inline void drawKaleidoscope(NVGcontext* vg, int img, float hw, float hh,
                             float w, float h, float coverHW, float coverHH,
                             int mode, float alpha, bool rotOn, float rotDeg,
                             bool mirrOn, float mirrDeg, bool clipToImage,
                             float dspX, float dspY, float dspW, float dspH,
                             float txOff = 0.f, float tyOff = 0.f) {
// Convenience macro — draws a sector using the precomputed cover size and
// display bounds.  txOff/tyOff are forwarded to kSector, which applies them
// with sign correction per flip axis so the mirror boundary stays seamless.
#define KS(cx, cy, cw, ch, ox, oy, ex, ey)                                     \
  kSector(vg, cx, cy, cw, ch, ox, oy, ex, ey, coverHW, coverHH, img, alpha,    \
          rotOn, rotDeg, mirrOn, mirrDeg, clipToImage, dspX, dspY, dspW, dspH, \
          txOff, tyOff)

  switch (mode) {
    // ── 2-fold
    // ────────────────────────────────────────────────────────────────
    case 1:
      // Left | right
      KS(-hw, -hh, hw, h, -hw, -hh, w, h);
      KS(0, -hh, hw, h, hw, -hh, -w, h);
      break;

    case 2:
      // Top | bottom
      KS(-hw, -hh, w, hh, -hw, -hh, w, h);
      KS(-hw, 0, w, hh, -hw, hh, w, -h);
      break;

    // ── 4-fold (source = full panel)
    // ──────────────────────────────────────────
    case 3:
      // Classic quad: TL tiled to all four quadrants with H/V flip
      KS(-hw, -hh, hw, hh, -hw, -hh, w, h);
      KS(0, -hh, hw, hh, hw, -hh, -w, h);
      KS(-hw, 0, hw, hh, -hw, hh, w, -h);
      KS(0, 0, hw, hh, hw, hh, -w, -h);
      break;

    // ── 4-fold (source = centre half) ────────────────────────────────────────
    case 4: {
      // Tile only the central 50% of each axis → zoomed-in symmetry
      float hw2 = hw * 0.5f, hh2 = hh * 0.5f;
      KS(-hw, -hh, hw, hh, -hw2, -hh2, w * 0.5f, h * 0.5f);
      KS(0, -hh, hw, hh, hw2, -hh2, -w * 0.5f, h * 0.5f);
      KS(-hw, 0, hw, hh, -hw2, hh2, w * 0.5f, -h * 0.5f);
      KS(0, 0, hw, hh, hw2, hh2, -w * 0.5f, -h * 0.5f);
      break;
    }

    // ── 4-fold (source = centre quarter) ─────────────────────────────────────
    case 5: {
      float qw = hw * 0.5f, qh = hh * 0.5f;
      KS(-hw, -hh, hw, hh, -qw, -qh, w * 0.25f, h * 0.25f);
      KS(0, -hh, hw, hh, qw, -qh, -w * 0.25f, h * 0.25f);
      KS(-hw, 0, hw, hh, -qw, qh, w * 0.25f, -h * 0.25f);
      KS(0, 0, hw, hh, qw, qh, -w * 0.25f, -h * 0.25f);
      break;
    }

    // ── 4 vertical strips (double H-period)
    // ───────────────────────────────────
    case 6: {
      // Divides width into 4 equal strips; adjacent strips are H-mirrors.
      // Each strip maps only the left half of the screen source.
      float qw = hw * 0.5f;                      // strip half-width
      KS(-hw, -hh, qw, h, -hw, -hh, w, h);       // strip A  normal
      KS(-hw + qw, -hh, qw, h, hw, -hh, -w, h);  // strip B  h-flip
      KS(0, -hh, qw, h, -hw, -hh, w, h);         // strip C  normal
      KS(0 + qw, -hh, qw, h, hw, -hh, -w, h);    // strip D  h-flip
      break;
    }

    // ── 4 horizontal strips (double V-period)
    // ─────────────────────────────────
    case 7: {
      float qh = hh * 0.5f;
      KS(-hw, -hh, w, qh, -hw, -hh, w, h);
      KS(-hw, -hh + qh, w, qh, -hw, hh, w, -h);
      KS(-hw, 0, w, qh, -hw, -hh, w, h);
      KS(-hw, 0 + qh, w, qh, -hw, hh, w, -h);
      break;
    }

    // ── 3 vertical strips (triple period: L | flip | L)
    // ───────────────────────
    case 8: {
      float tw = w / 3.f;  // one-third of full width (= 2hw/3)
      float x0 = -hw, x1 = -hw + tw, x2 = -hw + 2.f * tw;
      KS(x0, -hh, tw, h, x0, -hh, w, h);        // strip 1  normal
      KS(x1, -hh, tw, h, x2 + tw, -hh, -w, h);  // strip 2  flip
      KS(x2, -hh, tw, h, x0, -hh, w, h);        // strip 3  normal
      break;
    }

    // ── 3 horizontal strips
    // ────────────────────────────────────────────────────
    case 9: {
      float th = h / 3.f;
      float y0 = -hh, y1 = -hh + th, y2 = -hh + 2.f * th;
      KS(-hw, y0, w, th, -hw, y0, w, h);
      KS(-hw, y1, w, th, -hw, y2 + th, w, -h);
      KS(-hw, y2, w, th, -hw, y0, w, h);
      break;
    }

    // ── 4-fold with each quadrant internally H-mirrored (8-sector approx)
    // ─────
    case 10: {
      float qw = hw * 0.5f;  // eighth-panel width
      // TL quadrant split L/R
      KS(-hw, -hh, qw, hh, -hw, -hh, w, h);
      KS(-hw + qw, -hh, qw, hh, hw, -hh, -w, h);
      // TR (h-flip of TL)
      KS(0, -hh, qw, hh, hw, -hh, -w, h);
      KS(0 + qw, -hh, qw, hh, -hw, -hh, w, h);
      // BL (v-flip of TL)
      KS(-hw, 0, qw, hh, -hw, hh, w, -h);
      KS(-hw + qw, 0, qw, hh, hw, hh, -w, -h);
      // BR (both flips)
      KS(0, 0, qw, hh, hw, hh, -w, -h);
      KS(0 + qw, 0, qw, hh, -hw, hh, w, -h);
      break;
    }

    // ── 4-fold from top-right quadrant
    // ────────────────────────────────────────
    case 11: {
      // Source quadrant is TR; pattern extents flip accordingly.
      KS(-hw, -hh, hw, hh, hw, -hh, -w, h);  // TL: TR h-flipped
      KS(0, -hh, hw, hh, 0, -hh, w, h);      // TR: TR normal
      KS(-hw, 0, hw, hh, hw, hh, -w, -h);    // BL: TR both
      KS(0, 0, hw, hh, 0, hh, w, -h);        // BR: TR v-flipped
      break;
    }

    // ── 4-fold from bottom-right quadrant
    // ─────────────────────────────────────
    case 12:
    default: {
      KS(-hw, -hh, hw, hh, hw, hh, -w, -h);  // TL: BR both-flipped
      KS(0, -hh, hw, hh, 0, hh, w, -h);      // TR: BR v-flipped
      KS(-hw, 0, hw, hh, hw, -hh, -w, h);    // BL: BR h-flipped
      KS(0, 0, hw, hh, 0, -hh, w, h);        // BR: BR normal
      break;
    }
  }

#undef KS
}
