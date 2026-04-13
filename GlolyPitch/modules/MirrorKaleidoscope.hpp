#pragma once
#include "rack.hpp"
#include "MirrorRotation.hpp"
#include "MirrorFlip.hpp"
using namespace rack;

// ── kSector ───────────────────────────────────────────────────────────────────
// Intersects the current outer scissor with clipRect (in unrotated space so the
// intersection with the panel scissor is correct), then applies rotation/flip
// before painting the image pattern.  This avoids nvgIntersectScissor giving
// wrong results when called in a rotated coordinate frame.
static inline void kSector(NVGcontext* vg,
                            float clipX, float clipY, float clipW, float clipH,
                            float ox,    float oy,    float ex,    float ey,
                            float coverHW, float coverHH,
                            int img, float alpha,
                            bool rotOn, float rotDeg,
                            bool mirrOn, float mirrDeg) {
  nvgSave(vg);
  // Scissor in unrotated space — intersection with outer panel scissor is correct here
  nvgIntersectScissor(vg, clipX, clipY, clipW, clipH);
  // Apply rotation/flip after clipping so the image is rotated inside the sector
  if (rotOn)  applyRotation(vg, rotDeg);
  if (mirrOn) applyFlip(vg, mirrDeg);
  NVGpaint p = nvgImagePattern(vg, ox, oy, ex, ey, 0.f, img, alpha);
  nvgBeginPath(vg);
  // Draw a rect large enough that the sector clip does all the shaping
  nvgRect(vg, -coverHW, -coverHH, 2.f * coverHW, 2.f * coverHH);
  nvgFillPaint(vg, p);
  nvgFill(vg);
  nvgRestore(vg);
}

// ── drawKaleidoscope ──────────────────────────────────────────────────────────
// All coordinates in NVG space centered at the mirror panel center.
// hw/hh   = half-width/height of the mirror panel.
// w/h     = full width/height (= 2*hw, 2*hh).
// coverHW/HH = half-size of the circumscribed rect (covers panel at any angle).
// mode 1-12: see below.
// rotOn/rotDeg and mirrOn/mirrDeg are applied per-sector after sector clipping.
inline void drawKaleidoscope(NVGcontext* vg, int img,
                              float hw, float hh, float w, float h,
                              float coverHW, float coverHH,
                              int mode, float alpha,
                              bool rotOn, float rotDeg,
                              bool mirrOn, float mirrDeg) {

// Convenience macro — draws a sector using the precomputed cover size
#define KS(cx,cy,cw,ch, ox,oy,ex,ey) \
    kSector(vg, cx,cy,cw,ch, ox,oy,ex,ey, coverHW,coverHH, img, alpha, rotOn,rotDeg,mirrOn,mirrDeg)

  switch (mode) {

    // ── 2-fold ────────────────────────────────────────────────────────────────
    case 1:
      // Left | right
      KS(-hw,-hh, hw,h,  -hw,-hh,  w, h);
      KS(  0,-hh, hw,h,   hw,-hh, -w, h);
      break;

    case 2:
      // Top | bottom
      KS(-hw,-hh, w,hh,  -hw,-hh,  w, h);
      KS(-hw,  0, w,hh,  -hw, hh,  w,-h);
      break;

    // ── 4-fold (source = full panel) ──────────────────────────────────────────
    case 3:
      // Classic quad: TL tiled to all four quadrants with H/V flip
      KS(-hw,-hh, hw,hh,  -hw,-hh,  w, h);
      KS(  0,-hh, hw,hh,   hw,-hh, -w, h);
      KS(-hw,  0, hw,hh,  -hw, hh,  w,-h);
      KS(  0,  0, hw,hh,   hw, hh, -w,-h);
      break;

    // ── 4-fold (source = centre half) ────────────────────────────────────────
    case 4: {
      // Tile only the central 50% of each axis → zoomed-in symmetry
      float hw2 = hw * 0.5f, hh2 = hh * 0.5f;
      KS(-hw,-hh, hw,hh,  -hw2,-hh2,  w*0.5f, h*0.5f);
      KS(  0,-hh, hw,hh,   hw2,-hh2, -w*0.5f, h*0.5f);
      KS(-hw,  0, hw,hh,  -hw2, hh2,  w*0.5f,-h*0.5f);
      KS(  0,  0, hw,hh,   hw2, hh2, -w*0.5f,-h*0.5f);
      break;
    }

    // ── 4-fold (source = centre quarter) ─────────────────────────────────────
    case 5: {
      float qw = hw * 0.5f, qh = hh * 0.5f;
      KS(-hw,-hh, hw,hh,  -qw,-qh,  w*0.25f, h*0.25f);
      KS(  0,-hh, hw,hh,   qw,-qh, -w*0.25f, h*0.25f);
      KS(-hw,  0, hw,hh,  -qw, qh,  w*0.25f,-h*0.25f);
      KS(  0,  0, hw,hh,   qw, qh, -w*0.25f,-h*0.25f);
      break;
    }

    // ── 4 vertical strips (double H-period) ───────────────────────────────────
    case 6: {
      // Divides width into 4 equal strips; adjacent strips are H-mirrors.
      // Each strip maps only the left half of the screen source.
      float qw = hw * 0.5f;  // strip half-width
      KS(-hw,   -hh, qw, h,  -hw, -hh,  w, h);   // strip A  normal
      KS(-hw+qw,-hh, qw, h,   hw, -hh, -w, h);   // strip B  h-flip
      KS(  0,   -hh, qw, h,  -hw, -hh,  w, h);   // strip C  normal
      KS(  0+qw,-hh, qw, h,   hw, -hh, -w, h);   // strip D  h-flip
      break;
    }

    // ── 4 horizontal strips (double V-period) ─────────────────────────────────
    case 7: {
      float qh = hh * 0.5f;
      KS(-hw,-hh,   w, qh,  -hw, -hh,  w, h);
      KS(-hw,-hh+qh,w, qh,  -hw,  hh,  w,-h);
      KS(-hw,  0,   w, qh,  -hw, -hh,  w, h);
      KS(-hw,  0+qh,w, qh,  -hw,  hh,  w,-h);
      break;
    }

    // ── 3 vertical strips (triple period: L | flip | L) ───────────────────────
    case 8: {
      float tw = w / 3.f;  // one-third of full width (= 2hw/3)
      float x0 = -hw, x1 = -hw + tw, x2 = -hw + 2.f*tw;
      KS(x0,-hh, tw, h,   x0,   -hh,  w, h);   // strip 1  normal
      KS(x1,-hh, tw, h,   x2+tw,-hh, -w, h);   // strip 2  flip
      KS(x2,-hh, tw, h,   x0,   -hh,  w, h);   // strip 3  normal
      break;
    }

    // ── 3 horizontal strips ────────────────────────────────────────────────────
    case 9: {
      float th = h / 3.f;
      float y0 = -hh, y1 = -hh + th, y2 = -hh + 2.f*th;
      KS(-hw, y0, w, th,  -hw,  y0,    w, h);
      KS(-hw, y1, w, th,  -hw,  y2+th, w,-h);
      KS(-hw, y2, w, th,  -hw,  y0,    w, h);
      break;
    }

    // ── 4-fold with each quadrant internally H-mirrored (8-sector approx) ─────
    case 10: {
      float qw = hw * 0.5f;  // eighth-panel width
      // TL quadrant split L/R
      KS(-hw,  -hh, qw,hh,  -hw,-hh,  w, h);
      KS(-hw+qw,-hh, qw,hh,  hw,-hh, -w, h);
      // TR (h-flip of TL)
      KS(  0,  -hh, qw,hh,   hw,-hh, -w, h);
      KS(  0+qw,-hh, qw,hh, -hw,-hh,  w, h);
      // BL (v-flip of TL)
      KS(-hw,   0,  qw,hh,  -hw, hh,  w,-h);
      KS(-hw+qw, 0, qw,hh,   hw, hh, -w,-h);
      // BR (both flips)
      KS(  0,   0,  qw,hh,   hw, hh, -w,-h);
      KS(  0+qw, 0, qw,hh,  -hw, hh,  w,-h);
      break;
    }

    // ── 4-fold from top-right quadrant ────────────────────────────────────────
    case 11: {
      // Source quadrant is TR; pattern extents flip accordingly.
      KS(-hw,-hh, hw,hh,   hw,-hh, -w, h);   // TL: TR h-flipped
      KS(  0,-hh, hw,hh,    0,-hh,  w, h);   // TR: TR normal
      KS(-hw,  0, hw,hh,   hw, hh, -w,-h);   // BL: TR both
      KS(  0,  0, hw,hh,    0, hh,  w,-h);   // BR: TR v-flipped
      break;
    }

    // ── 4-fold from bottom-right quadrant ─────────────────────────────────────
    case 12:
    default: {
      KS(-hw,-hh, hw,hh,   hw, hh, -w,-h);   // TL: BR both-flipped
      KS(  0,-hh, hw,hh,    0, hh,  w,-h);   // TR: BR v-flipped
      KS(-hw,  0, hw,hh,   hw,-hh, -w, h);   // BL: BR h-flipped
      KS(  0,  0, hw,hh,    0,-hh,  w, h);   // BR: BR normal
      break;
    }
  }

#undef KS
}
