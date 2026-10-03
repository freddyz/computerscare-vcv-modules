#pragma once
#include "Types.hpp"
namespace windchimes {
class Wind {
  struct Channel {
    float bass = 0.f, body = 0.f, air = 0.f;
    float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
    float process(float noise, float bassCoeff, float bodyCoeff, float airCoeff,
                  float b0, float b2, float a1, float a2, float tone,
                  float texture) {
      bass += (noise - bass) * bassCoeff;
      body += (noise - body) * bodyCoeff;
      air += (noise - air) * airCoeff;
      float howl = b0 * noise + b2 * x2 - a1 * y1 - a2 * y2;
      x2 = x1;
      x1 = noise;
      y2 = y1;
      y1 = howl;
      float pink = bass * 2.8f + body * 0.65f + air * 0.22f;
      float rustle = air - body;
      return pink * (1.f - tone * 0.55f) +
             rustle * (0.15f + tone * 0.85f) * (0.4f + texture * 0.6f) +
             howl * texture * 0.85f;
    }
  };
  Random weather{0x1948a3}, noiseLeft{0x7616ab}, noiseRight{0x915bcc};
  float elapsed = 0.f, targetX = 0.f, targetY = 0.f;
  float breezeX = 0.f, breezeY = 0.f, gustEnergy = 0.f;
  float direction = 0.f, eddyX = 0.f, eddyY = 0.f;
  float flutter = 0.f, flutterTarget = 0.f, envelope = 0.f;
  Channel left, right;
  float rate = 0.f, tone = 0.5f, texture = 0.4f;
  float bassCoeff = 0.f, bodyCoeff = 0.f, airCoeff = 0.f, envelopeCoeff = 0.f;
  float b0 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
  void updateHowl() {
    float frequency = 180.f + tone * 1000.f + strength * 210.f;
    float angle = 2.f * pi * std::min(frequency, rate * 0.2f) / rate;
    float alpha = std::sin(angle) / (2.f * (1.2f + texture * 3.5f));
    float norm = 1.f / (1.f + alpha);
    b0 = alpha * norm;
    b2 = -b0;
    a1 = -2.f * std::cos(angle) * norm;
    a2 = (1.f - alpha) * norm;
  }

 public:
  float x = 0.f, y = 0.f, strength = 0.f;
  void configure(float sampleRate, float newTone, float newTexture) {
    if (sampleRate == rate && newTone == tone && newTexture == texture) return;
    rate = sampleRate;
    tone = clamp(newTone, 0.f, 1.f);
    texture = clamp(newTexture, 0.f, 1.f);
    bassCoeff = 1.f - std::exp(-2.f * pi * 65.f / rate);
    bodyCoeff = 1.f - std::exp(-2.f * pi * (280.f + tone * 600.f) / rate);
    airCoeff = 1.f - std::exp(-2.f * pi * (1600.f + tone * 8500.f) / rate);
    envelopeCoeff = 1.f - std::exp(-1.f / (rate * 0.04f));
    updateHowl();
  }
  void gust() {
    gustEnergy = 1.f;
    elapsed = 0.f;
  }
  void step(float dt, float amount, float gustiness, float turbulence) {
    elapsed -= dt;
    if (elapsed <= 0.f) {
      elapsed = 4.f + weather.uniform() * (10.f - gustiness * 6.f);
      direction += weather.bipolar() * (0.3f + turbulence * 1.2f);
      float angle = direction;
      float magnitude = 0.15f + weather.uniform() * (0.35f + gustiness * 0.8f);
      targetX = std::cos(angle) * magnitude;
      targetY = std::sin(angle) * magnitude;
      flutterTarget = weather.bipolar();
    }
    breezeX +=
        (targetX - breezeX) * std::min(dt * (0.65f + gustiness * 0.6f), 1.f);
    breezeY +=
        (targetY - breezeY) * std::min(dt * (0.65f + gustiness * 0.6f), 1.f);
    flutter += (flutterTarget - flutter) * std::min(dt * 6.f, 1.f);
    gustEnergy *= std::exp(-dt * 0.8f);
    // Correlated gusts retain energy near pendulum time scales without a clock
    // or phase-aware drive. Even a smooth breeze has some speed fluctuation.
    float eddyRate = 1.2f + turbulence * 4.f;
    float coefficient = 1.f - std::exp(-dt * eddyRate);
    float variance = std::sqrt(3.f * (2.f - coefficient) / coefficient);
    eddyX += (weather.bipolar() * variance - eddyX) * coefficient;
    eddyY += (weather.bipolar() * variance - eddyY) * coefficient;
    float gustScale = 0.18f + gustiness * 0.4f;
    float force = amount + gustEnergy;
    float along = eddyX * gustScale;
    float across = eddyY * gustScale * (0.12f + turbulence * 0.75f);
    x = force *
        (breezeX + std::cos(direction) * along - std::sin(direction) * across);
    y = force *
        (breezeY + std::sin(direction) * along + std::cos(direction) * across);
    strength = clamp(std::sqrt(x * x + y * y), 0.f, 1.5f);
    if (rate > 0.f) updateHowl();
  }
  Stereo process(float sampleRate) {
    if (rate != sampleRate) configure(sampleRate, tone, texture);
    float target = strength * (1.f + flutter * texture * 0.4f);
    envelope += (target - envelope) * envelopeCoeff;
    float l = noiseLeft.bipolar(), r = noiseRight.bipolar();
    // Shared low-frequency air movement plus independent stereo rustling.
    float common = (l + r) * 0.3f;
    float outLeft = left.process(l * 0.7f + common, bassCoeff, bodyCoeff,
                                 airCoeff, b0, b2, a1, a2, tone, texture);
    float outRight = right.process(r * 0.7f + common, bassCoeff, bodyCoeff,
                                   airCoeff, b0, b2, a1, a2, tone, texture);
    return {outLeft * envelope * 0.45f, outRight * envelope * 0.45f};
  }
};
}  // namespace windchimes
