#pragma once

#include "Computerscare.hpp"

// SVG-style flat faces, matching the warped buttons. The socket stays at the
// widget center so cables remain aligned when perspective settings change.
struct PerspectivePentagonJack : ComputerscareSvgPort {
  enum class Profile { Raised, Recessed };
  Profile profile = Profile::Raised;
  bool pointingUp = true;
  float perspectiveAngle = 135.f;  // Degrees clockwise: 0 right, 90 down.
  float depth = 3.f;
  float distortion = .6f;
  unsigned shapeSeed = random::u32();

  PerspectivePentagonJack() { box.size = Vec(27, 29); }

  void configure(Profile appearance, bool up, float angleDegrees = 135.f,
                 float thickness = 3.f, float irregularity = .6f,
                 unsigned seed = 0) {
    profile = appearance;
    pointingUp = up;
    perspectiveAngle = angleDegrees;
    depth = thickness;
    distortion = irregularity;
    if (seed) shapeSeed = seed;
  }

  void draw(const DrawArgs& args) override {
    auto* vg = args.vg;
    nvgSave(vg);
    float extent = std::min(box.size.x, box.size.y);
    float thickness = clamp(depth, 0.f, extent * .18f);
    float wobble = clamp(distortion, 0.f, extent * .04f);
    float angle = perspectiveAngle * M_PI / 180.f;
    Vec shift(std::cos(angle) * thickness, std::sin(angle) * thickness);
    Vec center = box.size.mult(.5f);
    const Vec unit[] = {Vec(.5f, 0), Vec(.975f, .345f), Vec(.794f, .905f),
                        Vec(.206f, .905f), Vec(.025f, .345f)};
    Vec face[5], back[5];
    auto jitter = [&](unsigned corner) {
      unsigned bits = shapeSeed * 0x9e3779b9u + corner * 0x85ebca6bu;
      bits ^= bits >> 16;
      return (float(bits & 1023u) / 1023.f - .5f) * wobble;
    };
    for (int i = 0; i < 5; ++i) {
      Vec point = unit[i];
      if (!pointingUp) point.y = 1.f - point.y;
      // Leave room for either projection direction within the same hit area.
      float inset = thickness * (profile == Profile::Raised ? 1.8f : 1.f);
      Vec offset((point.x - .5f) * (box.size.x - inset - 1.f),
                 (point.y - .5f) * (box.size.y - inset - 1.f));
      face[i] = center + offset + Vec(jitter(i * 2), jitter(i * 2 + 1));
      back[i] = profile == Profile::Raised
                    ? face[i] + shift
                    : center + (face[i] - center).mult(.85f) + shift.mult(.4f);
    }
    auto polygon = [&](const Vec* points, int count, int grey) {
      nvgBeginPath(vg);
      nvgMoveTo(vg, points[0].x, points[0].y);
      for (int i = 1; i < count; ++i) nvgLineTo(vg, points[i].x, points[i].y);
      nvgClosePath(vg);
      nvgFillColor(vg, nvgRGB(grey, grey, grey));
      nvgFill(vg);
      nvgStrokeColor(vg, nvgRGB(0, 0, 0));
      nvgStrokeWidth(vg, .65f);
      nvgLineJoin(vg, NVG_BEVEL);
      nvgStroke(vg);
    };
    if (profile == Profile::Raised)
      polygon(back, 5, 112);
    else
      polygon(face, 5, 210);
    auto drawWalls = [&]() {
      for (int i = 0; i < 5; ++i) {
        int next = (i + 1) % 5;
        Vec edge = face[next] - face[i];
        float facing =
            (edge.y * shift.x - edge.x * shift.y) * (pointingUp ? 1.f : -1.f);
        if (profile == Profile::Raised && facing <= 0.f) continue;
        Vec wall[] = {face[i], face[next], back[next], back[i]};
        polygon(wall, 4,
                facing > 0.f ? (profile == Profile::Recessed ? 54 : 86)
                             : (profile == Profile::Recessed ? 110 : 162));
      }
    };
    drawWalls();
    polygon(profile == Profile::Raised ? face : back, 5,
            profile == Profile::Raised ? 234 : 172);
    // Only the surrounding pentagon is warped. A round socket sits on the
    // well floor; the near wall can hide a small part of its circular rim.
    Vec apertureCenter = center + Vec(jitter(23), jitter(24)).mult(.15f);
    if (profile == Profile::Recessed) apertureCenter += shift.mult(.65f);
    float radius = extent * (profile == Profile::Raised ? .195f : .205f);
    nvgBeginPath(vg);
    nvgCircle(vg, apertureCenter.x, apertureCenter.y, radius);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(25, 140, 115));
    nvgStrokeWidth(vg, extent * .045f);
    nvgStroke(vg);
    // Occlusion is painted by the pentagonal lip, not by warping the circle.
    if (profile == Profile::Recessed) drawWalls();
    nvgRestore(vg);
    PortWidget::draw(args);
  }
};

struct JackPerspectiveRaisedPointingUp : PerspectivePentagonJack {
  JackPerspectiveRaisedPointingUp() { configure(Profile::Raised, true); }
};
struct JackPerspectiveRaisedPointingDown : PerspectivePentagonJack {
  JackPerspectiveRaisedPointingDown() { configure(Profile::Raised, false); }
};
struct JackPerspectiveRecessedPointingUp : PerspectivePentagonJack {
  JackPerspectiveRecessedPointingUp() { configure(Profile::Recessed, true); }
};
struct JackPerspectiveRecessedPointingDown : PerspectivePentagonJack {
  JackPerspectiveRecessedPointingDown() { configure(Profile::Recessed, false); }
};
