#pragma once
#include "Geometry.hpp"
namespace windchimes {
// Two spherical links, with absolute velocities and constraint impulses.
// The sail keeps its own momentum when the clapper hits a tube.
class Suspension {
  Point upper{0.f, 1.f, 0.f}, lower{0.f, 1.f, 0.f};
  Point primaryVelocity, sailVelocity;
  float primaryMass = 1.2f, primaryInverseMass = 1.f / 1.2f, targetMass = 1.2f,
        configuredWeight = .5f;
  float sailArea = 1.f, targetArea = 1.f, configuredSize = .5f;
  static Point tangent(Point v, Point axis) { return v - axis * v.dot(axis); }
  static Point unit(Point v) {
    return v * (1.f / std::max(std::sqrt(v.dot(v)), 1e-6f));
  }
  static Point bounded(Point axis, float limit) {
    axis = unit(axis);
    if (axis.y >= std::cos(limit)) return axis;
    float horizontal = std::hypot(axis.x, axis.z);
    float scale = std::sin(limit) / std::max(horizontal, 1e-6f);
    return {axis.x * scale, std::cos(limit), axis.z * scale};
  }
  static Point windDrive(Point force, Point velocity) {
    // Wind can add momentum or steer it, but cannot do negative work.
    float work = force.dot(velocity), speed2 = velocity.dot(velocity);
    if (work < 0.f && speed2 > 1e-8f)
      force = force - velocity * (work / speed2);
    return force;
  }
  void projectVelocities() {
    primaryVelocity = tangent(primaryVelocity, upper);
    Point q = tangent(lower, upper);
    float denominator = primaryInverseMass * q.dot(q) + sailInverseMass;
    float impulse = (sailVelocity - primaryVelocity).dot(lower) / denominator;
    primaryVelocity = primaryVelocity + q * (primaryInverseMass * impulse);
    sailVelocity = sailVelocity - lower * (sailInverseMass * impulse);
    // Unilateral angular stops remove only outward motion, with mass-weighted
    // impulses; both links remain free to swing back toward vertical.
    for (int iteration = 0; iteration < 2; ++iteration) {
      if (upper.y <= std::cos(strikerSwingLimit) + 1e-5f) {
        Point out = tangent({upper.x, 0.f, upper.z}, upper);
        if (primaryVelocity.dot(out) > 0.f)
          applyImpulse(out * (-primaryVelocity.dot(out) /
                              std::max(response(out).dot(out), 1e-6f)));
      }
      if (lower.y <= std::cos(1.1f) + 1e-5f) {
        Point out = tangent({lower.x, 0.f, lower.z}, lower);
        float speed = (sailVelocity - primaryVelocity).dot(out);
        if (speed > 0.f) {
          Point t = tangent(out, upper);
          float coupling = primaryInverseMass * q.dot(t) / denominator;
          Point a = (q * coupling - t) * primaryInverseMass;
          Point b = (out - lower * coupling) * sailInverseMass;
          float impulse = -speed / std::max((b - a).dot(out), 1e-6f);
          primaryVelocity = primaryVelocity + a * impulse;
          sailVelocity = sailVelocity + b * impulse;
        }
      }
    }
  }

 public:
  static constexpr float sailMass = 0.6f;
  static constexpr float sailInverseMass = 1.f / sailMass;
  void setSailSize(float size) {
    if (!std::isfinite(size)) size = .5f;
    size = clamp(size, 0.f, 1.f);
    if (size == configuredSize) return;
    configuredSize = size;
    targetArea = .25f * std::pow(16.f, size);
  }
  void setWeight(float weight) {
    if (!std::isfinite(weight)) weight = .5f;
    weight = clamp(weight, 0.f, 1.f);
    if (weight == configuredWeight) return;
    configuredWeight = weight;
    targetMass = .3f * std::pow(16.f, weight);
  }
  float mass() const { return primaryMass; }
  // Tube mass is normalized to one. Reduced mass scales impact excitation;
  // 50% preserves the original contact level exactly.
  float impactScale() const {
    return std::sqrt((primaryMass / (primaryMass + 1.f)) * (2.2f / 1.2f));
  }
  Point axis() const { return upper; }
  Point sailAxis() const { return lower; }
  Point velocity() const { return primaryVelocity; }
  Point sailSpeed() const { return sailVelocity; }
  Point position() const {
    return Point(0.f, pivotY, 0.f) + upper * strikerLength;
  }
  Point sailPosition() const { return position() + lower * sailLength; }
  float kineticEnergy() const {
    return 0.5f * (primaryMass * primaryVelocity.dot(primaryVelocity) +
                   sailMass * sailVelocity.dot(sailVelocity));
  }
  float energy() const {
    return kineticEnergy() +
           9.81f * (primaryMass * strikerLength * (1.f - upper.y) +
                    sailMass * (strikerLength * (1.f - upper.y) +
                                sailLength * (1.f - lower.y)));
  }
  void reset() {
    sailArea = targetArea;
    primaryMass = targetMass;
    primaryInverseMass = 1.f / primaryMass;
    upper = lower = {0.f, 1.f, 0.f};
    primaryVelocity = sailVelocity = {};
  }
  // Inverse effective mass at the clapper, accounting for the lower cord.
  Point response(Point normal) const {
    Point t = tangent(normal, upper), q = tangent(lower, upper);
    float impulse = primaryInverseMass * q.dot(t) /
                    (primaryInverseMass * q.dot(q) + sailInverseMass);
    return (t - q * impulse) * primaryInverseMass;
  }
  void applyImpulse(Point impulse) {
    Point t = tangent(impulse, upper), q = tangent(lower, upper);
    float coupling = primaryInverseMass * q.dot(t) /
                     (primaryInverseMass * q.dot(q) + sailInverseMass);
    primaryVelocity = primaryVelocity + (t - q * coupling) * primaryInverseMass;
    sailVelocity = sailVelocity + lower * (sailInverseMass * coupling);
  }
  void kick(Point speed) {
    primaryVelocity = primaryVelocity + tangent(speed, upper);
    sailVelocity = sailVelocity + speed * 1.3f;
    projectVelocities();
  }
  void shift(Point displacement) {
    upper = bounded(upper + displacement * (1.f / strikerLength),
                    strikerSwingLimit);
    projectVelocities();
  }
  void step(float dt, Point air, float swing, float dragScale = 1.f) {
    primaryMass += (targetMass - primaryMass) * std::min(dt * 25.f, 1.f);
    primaryInverseMass = 1.f / primaryMass;
    sailArea += (targetArea - sailArea) * std::min(dt * 25.f, 1.f);
    // Wind-only coupling: calm air supplies no drag and no sustaining force.
    Point flow = air * 6.f;
    float speed = std::sqrt(flow.dot(flow));
    // Side-area approximation for the hanging paddle: less exposed when the
    // flow points along its cord. The residual area represents its thickness.
    float axial = flow.dot(lower) / std::max(speed, 1e-6f);
    float exposure = 0.35f + 0.65f * (1.f - axial * axial);
    Point sailForce = windDrive(
        flow * (0.06f * dragScale * speed * exposure * sailArea), sailVelocity);
    Point primaryForce = windDrive(flow * .035f, primaryVelocity);
    Point a = Point(0.f, 9.81f, 0.f) + primaryForce * primaryInverseMass;
    Point b = Point(0.f, 9.81f, 0.f) + sailForce * sailInverseMass;
    float dot = upper.dot(lower);
    Point relativeSpeed = sailVelocity - primaryVelocity;
    // Solve the two length-constraint accelerations, including centripetal
    // terms.
    float rhsTop =
        -primaryVelocity.dot(primaryVelocity) / strikerLength - upper.dot(a);
    float rhsLower =
        -relativeSpeed.dot(relativeSpeed) / sailLength - lower.dot(b - a);
    float determinant =
        primaryInverseMass *
        (primaryInverseMass + sailInverseMass - primaryInverseMass * dot * dot);
    float topForce = ((primaryInverseMass + sailInverseMass) * rhsTop +
                      primaryInverseMass * dot * rhsLower) /
                     determinant;
    float linkForce =
        (primaryInverseMass * dot * rhsTop + primaryInverseMass * rhsLower) /
        determinant;
    a = a + (upper * topForce - lower * linkForce) * primaryInverseMass;
    b = b + lower * (linkForce * sailInverseMass);
    float damping = 0.24f + (1.f - swing) * 0.3f;
    primaryVelocity = (primaryVelocity + a * dt) * std::exp(-damping * dt);
    sailVelocity = (sailVelocity + b * dt) * std::exp(-damping * dt);
    Point first = upper * strikerLength + primaryVelocity * dt;
    Point second =
        upper * strikerLength + lower * sailLength + sailVelocity * dt;
    // Fixed bounded work, no allocation. Finish with exact link lengths.
    for (int iteration = 0; iteration < 6; ++iteration) {
      first = bounded(first, strikerSwingLimit) * strikerLength;
      Point delta = second - first;
      float length = std::sqrt(delta.dot(delta));
      Point correction =
          delta *
          ((length - sailLength) /
           (std::max(length, 1e-6f) * (primaryInverseMass + sailInverseMass)));
      first = first + correction * primaryInverseMass;
      second = second - correction * sailInverseMass;
    }
    upper = bounded(first, strikerSwingLimit);
    lower = bounded(second - upper * strikerLength, 1.1f);
    projectVelocities();
  }
};
}  // namespace windchimes
