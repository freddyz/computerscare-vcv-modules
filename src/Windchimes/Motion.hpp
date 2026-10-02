#pragma once
#include "Geometry.hpp"
namespace windchimes {
class Motion {
  struct Pendulum {
    Point axis{0.f, 1.f, 0.f}, velocity;
    float flash = 0.f;
  };
  std::array<Pendulum, maxTubes> tubes{};
  Pendulum striker;
  std::array<bool, maxTubes> strikerContacts{};
  std::array<bool, maxTubes * maxTubes> pairContacts{};
  int count = 0;
  Point meanWind;
  static Point tangent(Point v, Point axis) { return v - axis * v.dot(axis); }
  static void constrain(Pendulum& p, float bound) {
    float norm = std::sqrt(p.axis.dot(p.axis));
    p.axis = p.axis * (1.f / std::max(norm, 1e-6f));
    float horizontal = std::sqrt(p.axis.x * p.axis.x + p.axis.z * p.axis.z);
    if (p.axis.y < std::cos(bound)) {
      float scale = std::sin(bound) / std::max(horizontal, 1e-6f);
      p.axis = {p.axis.x * scale, std::cos(bound), p.axis.z * scale};
      Point outward = tangent({p.axis.x, 0.f, p.axis.z}, p.axis);
      float squared = outward.dot(outward);
      float speed = p.velocity.dot(outward);
      if (speed > 0.f && squared > 1e-6f)
        p.velocity = p.velocity - outward * (speed / squared);
    }
    p.velocity = tangent(p.velocity, p.axis);
    float speed = std::sqrt(p.velocity.dot(p.velocity));
    if (speed > 4.f) p.velocity = p.velocity * (4.f / speed);
  }
  static void integrate(Pendulum& p, float dt, float length, Point force,
                        float damping, float bound) {
    Point acceleration = tangent(Point(0.f, 9.81f, 0.f) + force, p.axis);
    p.velocity = (p.velocity + acceleration * dt) * std::exp(-damping * dt);
    p.axis = p.axis + p.velocity * (dt / length);
    constrain(p, bound);
    p.flash = std::max(0.f, p.flash - dt * 5.f);
  }
  static float angleComponent(const Pendulum& p, bool depth) {
    float horizontal = std::sqrt(p.axis.x * p.axis.x + p.axis.z * p.axis.z);
    return horizontal > 1e-6f ? std::atan2(horizontal, p.axis.y) *
                                    (depth ? p.axis.z : p.axis.x) / horizontal
                              : 0.f;
  }
  // Impulses act in each body's permitted tangent plane. Low restitution
  // dissipates impact energy rather than driving an endless pendulum.
  static void resolve(Pendulum& a, Pendulum& b, Point normal, float penetration,
                      float la, float lb, float massA, float boundA) {
    Point ta = tangent(normal, a.axis), tb = tangent(normal, b.axis);
    float inverse = massA * ta.dot(ta) + tb.dot(tb);
    if (inverse < 1e-6f) return;
    float speed = (a.velocity - b.velocity).dot(normal);
    if (speed > 0.f) {
      float impulse = speed * 1.25f / inverse;
      a.velocity = a.velocity - ta * (impulse * massA);
      b.velocity = b.velocity + tb * impulse;
    }
    float correction = std::min(penetration, 0.02f) * 0.8f / inverse;
    a.axis = a.axis - ta * (correction * massA / la);
    b.axis = b.axis + tb * (correction / lb);
    constrain(a, boundA);
    constrain(b, tubeSwingLimit);
  }
  void updateStriker() {
    Point p = strikerPosition();
    x = p.x;
    y = p.y;
    z = p.z;
  }

 public:
  float x = 0.f, y = pivotY + strikerLength, z = 0.f;
  void configure(int n) {
    if (count == n) return;
    count = n;
    tubes.fill(Pendulum());
    strikerContacts.fill(false);
    pairContacts.fill(false);
  }
  float tubeAngle(int t) const { return angleComponent(tubes[t], false); }
  float tubeDepth(int t) const { return angleComponent(tubes[t], true); }
  float strikerAngle() const { return angleComponent(striker, false); }
  float strikerDepth() const { return angleComponent(striker, true); }
  float tubeFlash(int t) const { return tubes[t].flash; }
  float strikerFlash() const { return striker.flash; }
  Capsule tubeShape(int t) const {
    return {
        tubeAnchorPoint(t, std::max(count, 1)) + tubes[t].axis * tubeLength(t),
        tubes[t].axis, tubeHalfLength(t), tubeRadius(std::max(count, 1))};
  }
  Point strikerPosition() const {
    return Point(0.f, pivotY, 0.f) + striker.axis * strikerLength;
  }
  float tubeX(int t) const { return tubeShape(t).center.x; }
  float tubeY(int t) const { return tubeShape(t).center.y; }
  float kineticEnergy() const {
    float energy = striker.velocity.dot(striker.velocity) * 0.2f;
    for (int t = 0; t < count; ++t)
      energy += tubes[t].velocity.dot(tubes[t].velocity) * 0.5f;
    return energy;
  }
  void reset() {
    tubes.fill(Pendulum());
    striker = Pendulum();
    count = 0;
    meanWind = Point();
    strikerContacts.fill(false);
    pairContacts.fill(false);
    updateStriker();
  }
  void kick(float direction) {
    Point impulse(std::cos(direction), 0.f, std::sin(direction));
    striker.velocity = striker.velocity + tangent(impulse * 1.8f, striker.axis);
    for (int t = 0; t < count; ++t)
      tubes[t].velocity =
          tubes[t].velocity +
          tangent(impulse * (0.08f + (t % 3) * 0.04f), tubes[t].axis);
  }
  void strike(int t) {
    if (count <= 0) return;
    Point target = tubeShape(t).center - strikerPosition();
    target.y = 0.f;
    float length = std::sqrt(target.dot(target));
    striker.velocity =
        tangent(target * (2.8f / std::max(length, 1e-6f)), striker.axis);
  }
  template <typename Hit, typename PairHit>
  void step(float dt, float windX, float windY, const SetConfig& c, int slot,
            Hit hit, PairHit pairHit) {
    configure(c.tubes);
    Point air(windX, 0.f, windY);
    meanWind = meanWind + (air - meanWind) * std::min(dt * 2.f, 1.f);
    // Gust changes impart momentum; steady wind cannot pin bodies up.
    Point gust = air - meanWind;
    float damping = 0.22f + (1.f - c.swing) * 0.65f;
    integrate(striker, dt, strikerLength, gust * 12.f, damping,
              strikerSwingLimit);
    for (int t = 0; t < count; ++t) {
      float exposure = 5.f + ((t + slot) % 3) * 3.f;
      integrate(tubes[t], dt, tubeLength(t),
                Point(gust.x + gust.z * (t % 2 ? 0.8f : -0.8f), 0.f,
                      gust.z + gust.x * (t % 2 ? -0.8f : 0.8f)) *
                    exposure,
                damping + 0.08f, tubeSwingLimit);
    }
    updateStriker();
    for (int t = 0; t < count; ++t) {
      Capsule shape = tubeShape(t);
      Point center = strikerPosition(),
            near = closestPoint(center, shape.top(), shape.bottom());
      Point delta = near - center;
      float distance = std::sqrt(delta.dot(delta)),
            radius = strikerRadius + shape.radius;
      if (distance <= radius && distance > 1e-6f) {
        Point normal = delta * (1.f / distance);
        float speed = (striker.velocity - tubes[t].velocity).dot(normal);
        if (!strikerContacts[t] && speed > 0.025f) {
          hit(t, clamp(speed * 0.8f, 0.035f, 1.f));
          striker.flash = tubes[t].flash = 1.f;
        }
        resolve(striker, tubes[t], normal, radius - distance, strikerLength,
                tubeLength(t), 2.5f, strikerSwingLimit);
        strikerContacts[t] = true;
        updateStriker();
      } else if (distance > radius * 1.08f)
        strikerContacts[t] = false;
    }
    for (int a = 0; a < count; ++a)
      for (int b = a + 1; b < count; ++b) {
        Capsule ca = tubeShape(a), cb = tubeShape(b);
        Contact contact = capsuleContact(ca, cb);
        float radius = ca.radius + cb.radius;
        int index = a * maxTubes + b;
        if (contact.distance <= radius) {
          Point delta = contact.distance > 1e-6f ? contact.b - contact.a
                                                 : cb.center - ca.center;
          float length = std::sqrt(delta.dot(delta));
          Point normal =
              length > 1e-6f ? delta * (1.f / length) : Point(1.f, 0.f, 0.f);
          float speed = (tubes[a].velocity - tubes[b].velocity).dot(normal);
          if (!pairContacts[index] && speed > 0.015f) {
            pairHit(a, b, clamp(speed * 0.7f, 0.025f, 0.8f));
            tubes[a].flash = tubes[b].flash = 1.f;
          }
          resolve(tubes[a], tubes[b], normal, radius - contact.distance,
                  tubeLength(a), tubeLength(b), 1.f, tubeSwingLimit);
          pairContacts[index] = true;
        } else if (contact.distance > radius * 1.08f)
          pairContacts[index] = false;
      }
    updateStriker();
  }
};
}  // namespace windchimes
