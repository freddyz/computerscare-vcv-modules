#pragma once
#include <nanovg_gl.h>

#include "rack.hpp"

// Applies hue-shift/split, warp (solarize→invert / curves contrast), and
// fold-frequency to a captured GL texture via a custom GLSL shader + FBO.
//
// HUE:
//   Positive values rotate all colors together around the hue wheel.
//   Negative values split RGB channels through separate hue offsets, with
//   neutral points at 0 and -360 degrees.
//
// WARP (-1..1):
//   0        bypass
//   0..-0.5  progressively solarize (Sabattier fold effect)
//  -0.5..-1  solarize fades into full invert at -1
//   0..+1/3  black level rises (shadows crush to black)
//   1/3..2/3 black level holds, white level falls (maximum contrast)
//   2/3..+1  black level returns to 0, white level stays low (highlights blown)
//
// FOLD_FREQ (1..4, mapped from INVERT knob 0..1):
//   Scales fold frequency and posterize step count.
//   Adds per-channel chromatic divergence at higher values.
//
// If all params are at neutral values the FBO is bypassed for sources whose
// texture orientation already matches the FBO output path.
// Uses GLSL #version 120 / OpenGL 2.x to match VCV Rack's NANOVG_GL2 context.

struct ColorTransformFBO {
  GLuint fbo = 0;
  GLuint outTex = 0;
  GLuint program = 0;
  GLuint vbo = 0;
  int nvgImg = -1;
  int lastW = 0, lastH = 0;

  GLint uTex = -1;
  GLint uHue = -1;
  GLint uWarp = -1;
  GLint uFoldFreq = -1;
  GLint uFlipY = -1;
  GLint aPos = -1;

  bool initialized = false;

  // ── Shader sources (GLSL 1.20 / OpenGL 2.x) ──────────────────────────────

  static const char* vertSrc() {
    return "#version 120\n"
           "attribute vec2 pos;\n"
           "varying vec2 uv;\n"
           "uniform float flipY;\n"  // 1.0=normal (screencap), -1.0=flip (file
                                     // image)
           "void main() {\n"
           "  gl_Position = vec4(pos, 0.0, 1.0);\n"
           "  uv = vec2(pos.x * 0.5 + 0.5, pos.y * flipY * 0.5 + 0.5);\n"
           "}\n";
  }

  static const char* fragSrc() {
    return "#version 120\n"
           "varying vec2 uv;\n"
           "uniform sampler2D tex;\n"
           "uniform float hue;\n"       // degrees -360..360
           "uniform float warp;\n"      // -1..1  (negative=solarize/invert,
                                        // positive=posterize/crush)
           "uniform float foldFreq;\n"  // 1..4   (fold count / step count /
                                        // chromatic divergence)
           "\n"
           // ── RGB ↔ HSL ─────────────────────────────────────────────────────
           "vec3 rgb2hsl(vec3 c) {\n"
           "  float hi = max(max(c.r,c.g),c.b);\n"
           "  float lo = min(min(c.r,c.g),c.b);\n"
           "  float d  = hi - lo;\n"
           "  float l  = (hi + lo) * 0.5;\n"
           "  float s  = d < 0.0001 ? 0.0 : d / (1.0 - abs(2.0*l - 1.0));\n"
           "  float h  = 0.0;\n"
           "  if (d > 0.0001) {\n"
           "    if      (hi == c.r) h = mod((c.g - c.b) / d, 6.0);\n"
           "    else if (hi == c.g) h = (c.b - c.r) / d + 2.0;\n"
           "    else                h = (c.r - c.g) / d + 4.0;\n"
           "    h /= 6.0;\n"
           "  }\n"
           "  return vec3(h, s, l);\n"
           "}\n"
           "float h2rgb(float p, float q, float t) {\n"
           "  t = fract(t);\n"
           "  if (t < 1.0/6.0) return p + (q-p)*6.0*t;\n"
           "  if (t < 0.5)      return q;\n"
           "  if (t < 2.0/3.0)  return p + (q-p)*(2.0/3.0 - t)*6.0;\n"
           "  return p;\n"
           "}\n"
           "vec3 hsl2rgb(vec3 c) {\n"
           "  if (c.y < 0.0001) return vec3(c.z);\n"
           "  float q = c.z < 0.5 ? c.z*(1.0+c.y) : c.z+c.y - c.z*c.y;\n"
           "  float p = 2.0*c.z - q;\n"
           "  return vec3(h2rgb(p,q,c.x+1.0/3.0), h2rgb(p,q,c.x), "
           "h2rgb(p,q,c.x-1.0/3.0));\n"
           "}\n"
           "vec3 hueTransform(vec3 rgb, float h) {\n"
           "  vec3 hsl = rgb2hsl(rgb);\n"
           "  if (h >= 0.0) {\n"
           "    hsl.x = fract(hsl.x + h / 360.0);\n"
           "    return hsl2rgb(hsl);\n"
           "  }\n"
           "  float cycle = clamp(-h / 360.0, 0.0, 1.0);\n"
           "  float amount = sin(cycle * 3.14159265);\n"
           "  float sat = clamp(hsl.y * (1.0 + amount * 3.0) + amount * "
           "0.25, 0.0, 1.0);\n"
           "  float wheel = cycle + 0.08 * amount * sin(hsl.x * 18.8495559 + "
           "cycle * 6.2831853);\n"
           "  vec3 shifted = hsl2rgb(vec3(fract(hsl.x + wheel), sat, hsl.z));\n"
           "  float spread = amount * 0.16;\n"
           "  vec3 warm = hsl2rgb(vec3(fract(hsl.x + wheel + spread), sat, "
           "hsl.z));\n"
           "  vec3 cool = hsl2rgb(vec3(fract(hsl.x + wheel - spread), sat, "
           "hsl.z));\n"
           "  vec3 split = vec3(warm.r, shifted.g, cool.b);\n"
           "  vec3 outRgb = mix(shifted, split, amount * 0.45);\n"
           "  outRgb = clamp((outRgb - 0.5) * (1.0 + amount * 1.2) + 0.5, 0.0, "
           "1.0);\n"
           "  return mix(rgb, outRgb, amount);\n"
           "}\n"
           "\n"
           // ── Negative side: solarize → full invert ─────────────────────────
           // t [0..1]: 0=bypass, 0..0.5=blend into solarize,
           // 0.5..1=solarize→invert chanOffset shifts the fold phase per
           // channel for chromatic divergence
           "float solarizeToInvert(float v, float t, float freq, float "
           "chanOffset) {\n"
           "  float tSol    = min(t * 2.0, 1.0);\n"
           "  float tInv    = max((t - 0.5) * 2.0, 0.0);\n"
           "  float phase   = (v + chanOffset * 0.08) * 3.14159265 * freq;\n"
           "  float sol     = abs(sin(phase));\n"
           "  float withSol = mix(v, sol, tSol);\n"
           "  return          mix(withSol, 1.0 - v, tInv);\n"
           "}\n"
           "\n"
           // ── Positive side: 3-phase curves contrast
           // ──────────────────────────────────────────
           // Phase 1 (t 0..1/3): black level rises  → shadows crush to black
           // Phase 2 (t 1/3..2/3): black holds, white falls → approaching max
           // Phase 3 (t 2/3..1): both held at extremes → maximum contrast
           // freq still drives chromatic divergence per-channel.
           "float curvesContrast(float v, float t, float freq) {\n"
           "  float diverge  = (freq - 1.0) / 3.0;\n"
           "  float maxBlack = 0.45;\n"
           "  float minWhite = 0.55;\n"
           "  float bl, wl;\n"
           "  if (t < 0.3333) {\n"
           "    float s = t * 3.0;\n"
           "    bl = s * maxBlack; wl = 1.0;\n"
           "  } else if (t < 0.6667) {\n"
           "    float s = (t - 0.3333) * 3.0;\n"
           "    bl = maxBlack; wl = 1.0 - s * (1.0 - minWhite);\n"
           "  } else {\n"
           "    bl = maxBlack; wl = minWhite;\n"
           "  }\n"
           "  float range = max(wl - bl, 0.001);\n"
           "  return clamp((v - bl) / range, 0.0, 1.0);\n"
           "}\n"
           "\n"
           "vec3 warpColor(vec3 rgb, float w, float freq) {\n"
           "  float diverge = (freq - 1.0) / 3.0;\n"  // 0..1 chromatic spread
           "  if (w < 0.0) {\n"
           "    float t = -w;\n"
           "    return vec3(\n"
           "      solarizeToInvert(rgb.r, t, freq, 0.0),\n"
           "      solarizeToInvert(rgb.g, t, freq, diverge * 0.5),\n"
           "      solarizeToInvert(rgb.b, t, freq, diverge * 1.0)\n"
           "    );\n"
           "  } else {\n"
           "    float t = w;\n"
           "    float r = curvesContrast(rgb.r, t, freq);\n"
           "    float g = curvesContrast(rgb.g, t, freq * (1.0 + diverge * "
           "0.15));\n"
           "    float b = curvesContrast(rgb.b, t, freq * (1.0 + diverge * "
           "0.30));\n"
           "    return vec3(r, g, b);\n"
           "  }\n"
           "}\n"
           "\n"
           "void main() {\n"
           "  vec4 col = texture2D(tex, uv);\n"
           "  vec3 rgb = col.rgb;\n"
           "  if (abs(hue) > 0.01) {\n"
           "    rgb = hueTransform(rgb, hue);\n"
           "  }\n"
           "  if (abs(warp) > 0.01) {\n"
           "    rgb = warpColor(rgb, warp, foldFreq);\n"
           "  }\n"
           "  gl_FragColor = vec4(rgb, col.a);\n"
           "}\n";
  }

  // ── Shader helpers ────────────────────────────────────────────────────────

  static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[512];
      glGetShaderInfoLog(s, 512, nullptr, log);
      DEBUG("ColorTransformFBO shader compile error: %s", log);
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
      DEBUG("ColorTransformFBO link error: %s", log);
      glDeleteProgram(p);
      return 0;
    }
    return p;
  }

  // ── One-time init ─────────────────────────────────────────────────────────

  void init() {
    initialized = true;

    // Full-screen quad (two triangles, [-1,1]×[-1,1])
    static const float quad[] = {-1.f, -1.f, 1.f, -1.f, 1.f,  1.f,
                                 -1.f, -1.f, 1.f, 1.f,  -1.f, 1.f};
    glGenBuffers(1, &vbo);
    GLint prevVBO;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);

    glGenFramebuffers(1, &fbo);

    program = linkProgram(vertSrc(), fragSrc());
    if (program) {
      uTex = glGetUniformLocation(program, "tex");
      uHue = glGetUniformLocation(program, "hue");
      uWarp = glGetUniformLocation(program, "warp");
      uFoldFreq = glGetUniformLocation(program, "foldFreq");
      uFlipY = glGetUniformLocation(program, "flipY");
      aPos = glGetAttribLocation(program, "pos");
    }
  }

  // ── Resize output texture when framebuffer resolution changes ─────────────

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

    GLint prevTex;
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

    GLint prevFBO;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           outTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

    nvgImg = nvglCreateImageFromHandleGL2(vg, outTex, w, h, NVG_IMAGE_FLIPY);
    lastW = w;
    lastH = h;
  }

  // ── Apply and return the NVG image handle to use for drawing ─────────────
  // warpV:     -1..1  (negative = solarize→invert, positive = posterize+crush)
  // foldFreqV: 1..4   (mapped from INVERT knob 0..1 via 1.0 + v * 3.0)
  // Returns srcImg if all params are neutral and no orientation normalization
  // is needed.

  // flipInputUV: pass true when srcTex is from a file-loaded image (stb_image
  // convention: row 0 = visual top) so the FBO output matches screen-capture
  // orientation before NVG_IMAGE_FLIPY is applied.
  int apply(NVGcontext* vg, GLuint srcTex, int srcImg, int fbW, int fbH,
            float hueV, float warpV, float foldFreqV,
            bool flipInputUV = false) {
    if (!flipInputUV && fabsf(hueV) <= 0.01f && fabsf(warpV) <= 0.01f)
      return srcImg;

    if (!initialized) init();
    if (!program) return srcImg;

    resizeIfNeeded(vg, fbW, fbH);
    if (nvgImg < 0) return srcImg;

    // ── Save GL state ─────────────────────────────────────────────────────
    GLint prevDrawFBO, prevProg, prevVBO;
    GLint prevViewport[4];
    GLint prevActiveTex, prevTex0;
    GLboolean prevDepth, prevBlend, prevCull, prevStencil;

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

    // ── Render to FBO ─────────────────────────────────────────────────────
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, fbW, fbH);
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (aPos >= 0) {
      glEnableVertexAttribArray(aPos);
      glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(uTex, 0);
    glUniform1f(uHue, hueV);
    glUniform1f(uWarp, warpV);
    glUniform1f(uFoldFreq, foldFreqV);
    glUniform1f(uFlipY, flipInputUV ? -1.0f : 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (aPos >= 0) glDisableVertexAttribArray(aPos);

    // ── Restore GL state ──────────────────────────────────────────────────
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

  ~ColorTransformFBO() = default;
};
