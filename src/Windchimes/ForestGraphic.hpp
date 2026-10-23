#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace windchimes {
// Stable procedural choices per scene; effect controls reshape the forest,
// while the wind bends it continuously. Roots stay in radial stage coordinates.
struct ForestGraphic {
  static constexpr int maxTrees = 96;
  struct Tree {
    float angle, radius, shape, species, phase;
    float barkHue, leafHue, girth, lean, taper, branching;
  };
  struct Motion {
    Vec position;
    float flexibility = .5f;
    bool enabled = false;
  };
  struct Debris {
    Vec position, velocity;
    float spin = 0, angle = 0, size = 0;
    float altitude = 0, verticalSpeed = 0, gravity = 60.f;
    float birthScale = 1.f, groundedTime = 0.f;
    bool leaf = false;
  };
  std::array<Tree, maxTrees> trees;
  std::array<int, maxTrees> damage{};
  std::array<float, maxTrees> stress{}, uprootStress{}, fall{};
  std::array<Vec, maxTrees> fallDirection{};
  std::array<Debris, 128> debris{};
  unsigned debrisHead = 0;
  double lastTime = 0.;
  explicit ForestGraphic(uint32_t seed) {
    auto roll = [&]() {
      seed = seed * 1664525u + 1013904223u;
      return (seed >> 8) * (1.f / 16777216.f);
    };
    float rotation = roll() * 6.2831853f;
    for (int i = 0; i < maxTrees; ++i) {
      trees[i] = {rotation + (i < 3 ? i * 2.094395f : (i - 3) * 2.399963f) +
                      (roll() - .5f) * .3f,
                  roll(),
                  roll(),
                  roll(),
                  roll() * 6.2831853f,
                  roll(),
                  roll(),
                  roll(),
                  roll(),
                  roll(),
                  roll()};
    }
  }
  static Vec windSway(const Tree& tree, Vec wind, double time) {
    float phase = float(time * (.6f + .8f * tree.shape)) + tree.phase;
    return Vec(wind.x * (.35f + .18f * std::sin(phase)),
               wind.y * (.3f + .15f * std::sin(phase * .83f + tree.lean)));
  }
  static Vec windTwig(const Tree& tree, Vec wind, double time) {
    float phase = float(time * (1.7f + tree.branching)) + tree.phase;
    return Vec(wind.x * .2f * std::sin(phase),
               wind.y * .15f * std::sin(phase * 1.23f));
  }
  static float stageNear(Vec ground, float width, float height) {
    return std::max(0.f, 1.f - std::hypot(2.f * ground.x / width - 1.f,
                                          2.f * ground.y / height - 1.f));
  }
  static float stageScale(Vec ground, float width, float height) {
    return .35f + .85f * stageNear(ground, width, height);
  }
  static bool depthBefore(float nearA, float yA, float nearB, float yB) {
    // Paint from front to rear: rear roots overlap central trunks even
    // though radial distance still makes their trees and debris smaller.
    return yA == yB ? nearA < nearB : yA < yB;
  }
  static void stepDebris(Debris& bit, Vec wind, float dt, float width,
                         float height, float horizon) {
    bool airborne = bit.altitude > 0.f;
    if (!airborne) {
      bit.groundedTime += dt;
      if (bit.groundedTime >= 5.f) {
        bit.size = 0.f;
        return;
      }
    }
    if (airborne) {
      bit.verticalSpeed -= bit.gravity * dt;
      bit.altitude = std::max(0.f, bit.altitude + bit.verticalSpeed * dt);
      if (bit.altitude == 0.f) {
        bit.verticalSpeed = 0.f;
        bit.velocity *= .18f;
      }
    }
    // Land at this tree's radial depth, then slide along that ground.
    // There is no continuous screen-down fall and no wraparound.
    float push = airborne ? (bit.leaf ? 65.f : 24.f) : (bit.leaf ? 48.f : 14.f);
    float threshold = airborne ? 0.f : (bit.leaf ? .07f : .25f);
    float force = std::max(0.f, wind.norm() - threshold);
    Vec target =
        wind * (push * force * stageScale(bit.position, width, height));
    bit.velocity +=
        (target - bit.velocity) * std::min(1.f, dt * (airborne ? 1.5f : 5.f));
    bit.position += bit.velocity * dt;
    if (bit.position.x < 0 || bit.position.x > width ||
        bit.position.y < horizon || bit.position.y > height) {
      bit.size = 0.f;
      return;
    }
    bit.angle += bit.spin * dt * force * (airborne ? 1.f : .2f);
  }
  static float densityFor(float expanse, float thicket, float undergrowth,
                          float canopy) {
    float lighting = .15f + .65f * canopy + .2f * undergrowth;
    float species = std::max(
        0.f,
        std::min(1.f, .5f + .35f * std::sin(expanse * 4.f - undergrowth * 3.f) +
                          .25f * std::sin(canopy * 5.f + thicket * 2.f)));
    float a = lighting * 6.2f, b = species * 5.1f, c = thicket * 7.4f,
          d = expanse * 5.9f, e = undergrowth * 4.7f;
    return std::max(
        0.f, std::min(1.f, .5f + .26f * std::sin(a - b * .73f + c * .4f) +
                               .24f * std::sin(d + e * .65f - a * .45f)));
  }
  void draw(NVGcontext* vg, float width, float height, double time,
            float expanse, float thicket, float undergrowth, float canopy,
            const std::array<Vec, 20>& flow, float turbulence,
            const std::array<Motion, 16>& motions, float windStrength) {
    float reverbMix = .15f + .65f * canopy + .2f * undergrowth;
    float reverbSize = std::max(
        0.f,
        std::min(1.f, .5f + .35f * std::sin(expanse * 4.f - undergrowth * 3.f) +
                          .25f * std::sin(canopy * 5.f + thicket * 2.f)));
    float delayMix = thicket, delayTime = expanse, feedback = undergrowth;
    float dt = lastTime > 0
                   ? std::min(.05f, std::max(0.f, float(time - lastTime)))
                   : 0.f;
    lastTime = time;
    auto sat = [](float v) { return std::max(0.f, std::min(1.f, v)); };
    reverbMix = sat(reverbMix);
    reverbSize = sat(reverbSize);
    delayMix = sat(delayMix);
    delayTime = sat(delayTime);
    feedback = sat(feedback);
    float light = .25f + .65f * reverbMix;
    NVGcolor ground =
        nvgLerpRGBA(nvgRGB(12, 27, 27), nvgRGB(27, 38, 31), light);
    NVGcolor sky =
        nvgLerpRGBA(nvgRGB(22, 39, 49), nvgRGB(62, 77, 67), reverbMix);
    float horizon = height * (.045f + .025f * delayTime);
    nvgSave(vg);
    nvgIntersectScissor(vg, 0, 0, width, height);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, width, height);
    nvgFillColor(vg, ground);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, width, horizon);
    nvgFillPaint(
        vg, nvgLinearGradient(vg, 0, 0, 0, horizon, nvgRGB(16, 29, 41), sky));
    nvgFill(vg);
    // A high, shallow horizon and restrained contours keep the scene usable.
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, horizon);
    nvgBezierTo(vg, width * .28f, horizon - 3, width * .72f, horizon + 3, width,
                horizon);
    nvgStrokeColor(vg, nvgRGBA(111, 135, 112, 65));
    nvgStrokeWidth(vg, .7f);
    nvgStroke(vg);
    for (int contour = 0; contour < 7; ++contour) {
      float y = horizon + (height - horizon) * (contour + 1) / 8.f;
      float hill =
          (4.f + feedback * 18.f) * std::sin(contour * 1.7f + delayTime * 3.f);
      nvgBeginPath(vg);
      nvgMoveTo(vg, 0, y);
      nvgBezierTo(vg, width * .3f, y + hill, width * .7f, y - hill, width, y);
      nvgStrokeColor(vg, nvgRGBA(115, 121, 81, 10 + int(reverbMix * 9)));
      nvgStrokeWidth(vg, .6f);
      nvgStroke(vg);
    }
    struct Placement {
      int index;
      float x, y, near;
    };
    std::array<Placement, maxTrees> placements;
    // Cross-coupled space controls, like Foly Pace's parameter mashings.
    float density = densityFor(expanse, thicket, undergrowth, canopy);
    int count = 22 + int(density * 74.f);
    for (int i = 0; i < count; ++i) {
      const auto& t = trees[i];
      float radius = i < 3 ? .10f + .075f * t.radius : .24f + .23f * t.radius;
      radius += (delayTime - .5f) * (i < 3 ? .025f : .035f);
      float angle = t.angle + delayMix * .2f * std::sin(t.phase);
      placements[i] = {i, .5f + radius * std::cos(angle),
                       .5f + radius * std::sin(angle), 1.f - radius * 2.f};
    }
    std::sort(placements.begin(), placements.begin() + count,
              [](const Placement& a, const Placement& b) {
                return depthBefore(a.near, a.y, b.near, b.y);
              });
    auto flowAt = [&](float x, float y) {
      float gx = sat(x) * 4, gy = sat(y) * 3;
      int ix = std::min(3, int(gx)), iy = std::min(2, int(gy));
      float tx = gx - ix, ty = gy - iy;
      return (flow[iy * 5 + ix] * (1 - tx) + flow[iy * 5 + ix + 1] * tx) *
                 (1 - ty) +
             (flow[(iy + 1) * 5 + ix] * (1 - tx) +
              flow[(iy + 1) * 5 + ix + 1] * tx) *
                 ty;
    };
    std::array<int, 128> debrisOrder{};
    int debrisCount = 0;
    for (int index = 0; index < int(debris.size()); ++index) {
      auto& bit = debris[index];
      if (bit.size <= 0) continue;
      Vec wind = flowAt(bit.position.x / width, bit.position.y / height);
      stepDebris(bit, wind, dt, width, height, horizon);
      if (bit.size > 0) debrisOrder[debrisCount++] = index;
    }
    std::sort(debrisOrder.begin(), debrisOrder.begin() + debrisCount,
              [&](int a, int b) {
                return depthBefore(stageNear(debris[a].position, width, height),
                                   debris[a].position.y,
                                   stageNear(debris[b].position, width, height),
                                   debris[b].position.y);
              });
    auto drawDebris = [&](int index) {
      const auto& bit = debris[index];
      float scale = stageScale(bit.position, width, height);
      float size = bit.size * scale / bit.birthScale;
      Vec screen = bit.position - Vec(0, bit.altitude * scale);
      float x = std::cos(bit.angle) * size, y = std::sin(bit.angle) * size;
      nvgBeginPath(vg);
      nvgMoveTo(vg, screen.x - x, screen.y - y);
      nvgLineTo(vg, screen.x + x, screen.y + y);
      float opacity = std::max(0.f, 1.f - bit.groundedTime / 5.f);
      nvgStrokeColor(vg, bit.leaf ? nvgRGBA(116, 125, 68, int(145 * opacity))
                                  : nvgRGBA(113, 99, 65, int(165 * opacity)));
      nvgStrokeWidth(vg, std::max(.35f, (bit.leaf ? 1.5f : .9f) * scale));
      nvgStroke(vg);
    };
    int nextDebris = 0;
    for (int ti = 0; ti < count; ++ti) {
      const auto& p = placements[ti];
      while (nextDebris < debrisCount) {
        const auto& bit = debris[debrisOrder[nextDebris]];
        if (!depthBefore(stageNear(bit.position, width, height), bit.position.y,
                         p.near, p.y * height))
          break;
        drawDebris(debrisOrder[nextDebris++]);
      }
      const auto& t = trees[p.index];
      float size =
          height * (.075f + 2.4f * p.near * p.near) * (.85f + .3f * t.shape);
      float root = p.y * height;
      if (p.near < .5f)
        size = std::min(size, std::max(8.f, (root - horizon - 4.f) * .88f));
      float trunk =
          width * (.003f + .075f * p.near * p.near) * (.5f + 1.3f * t.girth);
      bool deciduous = t.species < reverbSize;
      if (deciduous) trunk = std::min(trunk, size * (.016f + .014f * t.girth));
      Vec wind = flowAt(p.x, p.y);
      float strength = std::min(1.5f, wind.norm());
      const Motion* motion = nullptr;
      float nearest = 10.f;
      for (const auto& m : motions) {
        if (!m.enabled) continue;
        float distance = m.position.minus(Vec(p.x, p.y)).square();
        if (distance < nearest) {
          nearest = distance;
          motion = &m;
        }
      }
      float flexibility = .65f + .7f * t.shape;
      if (motion) flexibility *= .5f + motion->flexibility;
      // Weather drives trees independently of chime strikes. Chime settings
      // still supply local stiffness, but their pendulums never push trees.
      Vec inertia = windSway(t, wind, time);
      Vec twig = windTwig(t, wind, time);
      float bend = size * flexibility *
                   (.026f * wind.x + .17f * inertia.x + .08f * twig.x);
      float flutter = flexibility * (twig.x - inertia.x) * (.4f + turbulence);
      float depthBend = size * (.008f * wind.y + .05f * inertia.y);
      float load = std::max(strength, windStrength) * (.5f + .5f * turbulence);
      if (load > .72f && damage[p.index] < 3) {
        stress[p.index] += dt * (load - .72f) * (1.f + std::abs(twig.x) * 4.f) *
                           (1.f + t.shape);
        if (stress[p.index] > .24f + .25f * t.species) {
          stress[p.index] = 0.f;
          ++damage[p.index];
          for (int fragment = 0; fragment < 4; ++fragment) {
            auto& bit = debris[debrisHead++ % debris.size()];
            float side = (fragment % 2 ? -1.f : 1.f);
            // Position is on the local ground plane; altitude is separate.
            bit.groundedTime = 0.f;
            bit.position = Vec(p.x * width + side * size * .08f, root);
            bit.birthScale = stageScale(bit.position, width, height);
            bit.altitude = size * (.5f + .12f * fragment) / bit.birthScale;
            bit.verticalSpeed = 2.f + fragment;
            bit.gravity = std::max(20.f, size * 1.8f) / bit.birthScale;
            bit.velocity = Vec(wind.x * 12.f + side * 3.f, wind.y * 3.f);
            bit.size = std::max(1.f, std::min(5.f, size * .025f));
            bit.spin = side * (.5f + t.shape);
            bit.angle = t.phase;
            bit.leaf = fragment != 0;
          }
        }
      } else
        stress[p.index] = std::max(0.f, stress[p.index] - dt * .05f);
      // Whole trees fail only after sustained extreme weather. Once down,
      // they stay rooted on the local ground plane until the forest regrows.
      auto& topple = fall[p.index];
      if (topple == 0.f) {
        if (load > .94f) {
          uprootStress[p.index] +=
              dt * (load - .94f) * (.7f + t.shape + .5f * damage[p.index]);
          if (uprootStress[p.index] > .65f + 1.5f * t.girth) {
            topple = .001f;
            Vec direction =
                wind + Vec(.12f * std::cos(t.phase), .12f * std::sin(t.phase));
            float norm = std::max(.001f, direction.norm());
            fallDirection[p.index] = direction / norm;
          }
        } else {
          uprootStress[p.index] =
              std::max(0.f, uprootStress[p.index] - dt * .08f);
        }
      } else if (topple < 1.f) {
        topple = std::min(
            1.f, topple + dt * (.12f + 1.2f * topple) / (.8f + .7f * t.girth));
      }
      float breakHeight = topple > 0.f && t.branching > .45f
                              ? size * (.22f + .28f * t.taper)
                              : 0.f;
      auto point = [&](float x, float up) {
        float h = up / size;
        float moving = up >= breakHeight ? topple : 0.f;
        float hinge = up >= breakHeight ? breakHeight : 0.f;
        float above = up - hinge;
        float angle = moving * 1.5707963f;
        float groundLength = std::min(size, width * (.18f + .2f * p.near));
        Vec groundOffset = fallDirection[p.index] *
                           (groundLength * (above / size) * std::sin(angle));
        groundOffset.y *= .4f;
        float standing = std::cos(angle);
        return Vec(
            p.x * width + x + groundOffset.x +
                standing * ((bend + size * .065f * (t.lean - .5f)) * h * h +
                            flutter * size * .006f * h * h * h),
            root + groundOffset.y + x * .4f * std::sin(angle) -
                hinge * (1.f - moving) - above * standing +
                depthBend * h * h * standing);
      };
      auto vertex = [&](float x, float up, bool first = false) {
        Vec q = point(x, up);
        if (first)
          nvgMoveTo(vg, q.x, q.y);
        else
          nvgLineTo(vg, q.x, q.y);
      };
      float warmth = sat(.25f + .45f * delayTime + .2f * t.shape);
      NVGcolor bark =
          t.barkHue < .6f
              ? nvgLerpRGBA(nvgRGB(29, 49, 47), nvgRGB(88, 61, 43),
                            t.barkHue / .6f)
              : nvgLerpRGBA(nvgRGB(48, 48, 39), nvgRGB(103, 111, 99),
                            (t.barkHue - .6f) / .4f);
      bark = nvgLerpRGBA(ground, bark, .55f + .4f * light);
      bark = nvgLerpRGBA(bark, nvgRGB(74, 59, 40), .15f * warmth);
      float fog = sat((.20f - p.y) * 4.f);
      bark = nvgLerpRGBA(bark, sky, fog * .45f);
      for (int piece = 0; piece < (breakHeight > 0.f ? 2 : 1); ++piece) {
        float low = piece == 1 ? breakHeight / size : 0.f;
        float high =
            breakHeight > 0.f && piece == 0 ? breakHeight / size - .0001f : 1.f;
        nvgBeginPath(vg);
        for (int segment = 0; segment <= 8; ++segment) {
          float h = low + (high - low) * segment / 8.f;
          float profile = deciduous
                              ? (.7f - .35f * h + .12f * std::pow(1.f - h, 6.f))
                              : (.8f - (.43f + .25f * t.taper) * h);
          vertex(-trunk * profile * (1.f + .07f * std::sin(h * 9.f + t.phase)),
                 h * size, segment == 0);
        }
        for (int segment = 8; segment >= 0; --segment) {
          float h = low + (high - low) * segment / 8.f;
          float profile = deciduous
                              ? (.64f - .32f * h + .1f * std::pow(1.f - h, 6.f))
                              : (.72f - (.35f + .24f * t.taper) * h);
          vertex(trunk * profile, h * size);
        }
        nvgClosePath(vg);
        nvgFillColor(vg, bark);
        nvgFill(vg);
      }
      int branches = 2 + int(feedback * 4.f + t.branching * 4.f);
      for (int b = 0; b < std::max(0, branches - damage[p.index]); ++b) {
        float h =
            .35f + b * .55f / branches + .025f * std::sin(t.phase + b * 2.1f);
        float side = (b + int(t.branching * 5.f)) % 2 ? -1.f : 1.f;
        nvgBeginPath(vg);
        vertex(0, h * size, true);
        float reach =
            side * size * (.065f + .13f * t.branching) * (1.f - h * .4f);
        vertex(reach * .5f, (h + .04f + .025f * t.lean) * size);
        vertex(reach, (h + .08f + .06f * t.taper) * size);
        nvgStrokeColor(
            vg, nvgLerpRGBA(bark, nvgRGB(102, 99, 70), .12f * t.branching));
        nvgStrokeWidth(vg, std::max(.5f, trunk * (.13f + .18f * t.taper)));
        nvgStroke(vg);
      }
      // Nearby trees continue out of view; distant crowns stay whole.
      if (p.near < .5f) {
        NVGcolor leaves = deciduous
                              ? nvgLerpRGBA(nvgRGB(31, 65, 53),
                                            nvgRGB(102, 86, 42), t.leafHue)
                              : nvgLerpRGBA(nvgRGB(22, 54, 56),
                                            nvgRGB(58, 76, 38), t.leafHue);
        leaves = nvgLerpRGBA(ground, leaves, .62f + .32f * light);
        leaves = nvgLerpRGBA(leaves, sky, fog * .45f);
        float spread =
            (.13f + .10f * reverbMix) * size * (.65f + .7f * t.leafHue);
        if (deciduous && damage[p.index] > 0) {
          for (int lobe = damage[p.index] * 2; lobe < 5; ++lobe) {
            nvgBeginPath(vg);
            for (int v = 0; v < 12; ++v) {
              float angle = v * 6.2831853f / 12.f;
              float local = (lobe - 2) * .32f * spread;
              vertex(local + std::cos(angle) * spread * .43f,
                     size * (.71f + .065f * (lobe % 3)) +
                         std::sin(angle) * size * .13f,
                     v == 0);
            }
            nvgClosePath(vg);
            nvgFillColor(vg, leaves);
            nvgFill(vg);
          }
        } else if (deciduous) {
          nvgBeginPath(vg);
          for (int v = 0; v < 24; ++v) {
            float a = v * 6.2831853f / 24.f;
            float r = 1.f + .13f * std::sin(v * 2.3f + t.phase) +
                      .09f * std::sin(v * .9f + t.phase);
            vertex(std::cos(a) * spread * r,
                   size * (.76f + (.20f + .13f * t.taper) * std::sin(a) * r),
                   v == 0);
          }
          nvgClosePath(vg);
          nvgFillColor(vg, leaves);
          nvgFill(vg);
        } else if (!deciduous) {
          int tiers = 4 + int(t.shape * 3.f);
          for (int tier = damage[p.index]; tier < tiers; ++tier) {
            float h = .38f + tier * .48f / tiers;
            float reach = spread * (1.f - (h - .38f) * 1.25f) *
                          (.8f + .25f * std::sin(t.phase + tier * 1.8f));
            nvgBeginPath(vg);
            vertex(-reach, h * size, true);
            vertex(-reach * .65f, (h + .07f) * size);
            vertex(size * .015f * std::sin(t.phase + tier), (h + .25f) * size);
            vertex(reach * .72f, (h + .06f) * size);
            vertex(reach, (h - .015f) * size);
            nvgClosePath(vg);
            nvgFillColor(vg, leaves);
            nvgFill(vg);
          }
        }
      }
      int furrows = breakHeight > 0.f ? 0
                    : p.near > .4f    ? 2 + int(t.girth * 3.f)
                                      : 1;
      for (int stripe = 0; stripe < furrows; ++stripe) {
        float offset = (float(stripe + 1) / (furrows + 1) - .5f) * trunk;
        nvgBeginPath(vg);
        vertex(offset, size * .025f, true);
        for (int segment = 1; segment <= 8; ++segment) {
          float h = segment * .11f;
          vertex(offset * (1.f - h * .5f) +
                     trunk * .035f * std::sin(h * 13.f + t.phase + stripe),
                 size * h);
        }
        nvgStrokeColor(vg, nvgLerpRGBA(bark,
                                       stripe % 2 ? nvgRGB(12, 27, 25)
                                                  : nvgRGB(136, 141, 110),
                                       .24f));
        nvgStrokeWidth(vg, std::max(.35f, trunk * (.025f + .035f * t.taper)));
        nvgStroke(vg);
      }
    }
    while (nextDebris < debrisCount) drawDebris(debrisOrder[nextDebris++]);
    nvgRestore(vg);
  }
};
}  // namespace windchimes
