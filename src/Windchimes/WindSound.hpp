#pragma once
#include "Spatial.hpp"
namespace windchimes {
class WindSound {
  struct Band {
    float b = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;
    void configure(float rate, float frequency, float q) {
      float angle = 2.f * pi * std::min(frequency, rate * .22f);
      angle /= rate;
      float alpha = std::sin(angle) / (2.f * q), norm = 1.f / (1.f + alpha);
      b = alpha * norm;
      a1 = -2.f * std::cos(angle) * norm;
      a2 = (1.f - alpha) * norm;
    }
    float process(float x) {
      float y = b * x + z1;
      z1 = -a1 * y + z2;
      z2 = -b * x - a2 * y;
      if (std::fabs(z1) < 1e-15f) z1 = 0.f;
      if (std::fabs(z2) < 1e-15f) z2 = 0.f;
      return y;
    }
  };
  struct Patch {
    Band breath, rustle, whistle;
    float drift = 0.f, grain = 0.f, grainSmooth = 0.f, grainDecay = .999f;
    float catchLevel = 0.f, catchTarget = 0.f;
    std::array<float, 4> gains{}, currentGains{};
  };
  struct Profile {
    float low, breath, rustle, whistle, flutter, dry;
  };
  Random control{0x42c716}, noise{0xab7125};
  std::array<Patch, 3> patches{};
  float rate = 48000.f, low1 = 0.f, low2 = 0.f, lowCoeff = .01f;
  float speed = 0.f, targetSpeed = 0.f, smoothing = .001f, grainAttack = .01f;
  float currentTone = .45f, currentTexture = .4f;
  float targetTone = .45f, targetTexture = .4f, targetPresence = .5f,
        presence = .5f;
  float turbulence = 0.f, heading = 0.f;
  Profile profile{1.f, 1.f, .15f, .1f, .1f, 0.f};
  unsigned mask = 0;
  std::array<float, 4> centerGains{};
  std::array<std::array<float, 4>, 4> toneFilter{};
  float toneCoeff = .9f, attackCoeff = .001f, previousStrength = 0.f,
        lift = 0.f;

 public:
  void configure(float sampleRate, float tone, float texture, float mix) {
    if (rate != sampleRate) {
      rate = std::max(sampleRate, 8000.f);
      low1 = low2 = 0.f;
      toneFilter = {};
      patches = {};
      speed = 0.f;
    }
    smoothing = 1.f - std::exp(-1.f / (.12f * rate));
    grainAttack = 1.f - std::exp(-1.f / (.002f * rate));
    targetTone = clamp(tone, 0.f, 1.f);
    targetTexture = clamp(texture, 0.f, 1.f);
    targetPresence = clamp(mix, 0.f, 1.f);
  }
  // Weather/control-rate work: independent random detail never advances the
  // physical-weather RNG. Every band breathes differently beneath shared gusts.
  void step(float dt, float strength, float windX, float windY,
            float roughness) {
    turbulence = roughness;
    float rising =
        std::max(0.f, (strength - previousStrength) / std::max(dt, .0001f));
    previousStrength = strength;
    float liftTarget =
        clamp(rising * .08f + roughness * strength * .22f, 0.f, .3f);
    lift += (liftTarget - lift) *
            std::min(1.f, dt * (liftTarget > lift ? 5.f : 1.7f));
    float attackTime = .65f - .55f * clamp(strength, 0.f, 1.f);
    attackCoeff = 1.f - std::exp(-1.f / (attackTime * rate));
    currentTone += (targetTone - currentTone) * std::min(1.f, dt * 8.f);
    currentTexture +=
        (targetTexture - currentTexture) * std::min(1.f, dt * 8.f);
    presence += (targetPresence - presence) * std::min(1.f, dt * 8.f);
    // Continuous surface character: smooth exposed air -> catches/whistles ->
    // layered leaf rustle. Tone sets depth versus dry, bright surface detail.
    profile.low = .5f +
                  1.2f * (1.f - currentTone) * (1.f - .4f * currentTexture) +
                  .4f * presence;
    profile.breath = 1.2f - .6f * currentTexture;
    profile.rustle =
        2.f * currentTexture * currentTexture * (.25f + .75f * currentTone);
    profile.whistle = 5.6f * currentTexture * (1.f - currentTexture) *
                      (1.f - .5f * currentTone);
    profile.flutter = .6f * currentTexture;
    profile.dry = currentTexture * currentTexture * currentTone;
    targetSpeed = std::pow(clamp(strength, 0.f, 1.5f), 1.35f);
    // Low Mix reveals passing breezes; raising it fills in quieter airflow
    // and increases close buffeting, alongside the downstream level control.
    float opening =
        clamp((strength - .18f * (1.f - presence)) / .32f, 0.f, 1.f);
    targetSpeed *= presence + (1.f - presence) * opening * opening;
    // Four low-pass stages remove high-frequency surface detail at low Tone,
    // opening exponentially into the airy range instead of only tilting layers.
    float openingTone =
        std::pow(clamp((currentTone + lift * .3f) / .65f, 0.f, 1.f), .8f);
    float cutoff = 60.f * std::pow(300.f, openingTone);
    // Keep the deep end intact, then gently limit the upper range to ~8 kHz.
    if (cutoff > 3500.f)
      cutoff = 3500.f + 4500.f * (1.f - std::exp(-(cutoff - 3500.f) / 4500.f));
    cutoff = std::min(rate * .35f, cutoff);
    toneCoeff = 1.f - std::exp(-2.f * pi * cutoff / rate);
    if (strength > .02f) heading = std::atan2(windY, windX);
    lowCoeff =
        1.f - std::exp(-2.f * pi *
                       (55.f + 85.f * currentTone + 40.f * strength) / rate);
    for (int i = 0; i < 3; ++i) {
      auto& p = patches[i];
      float driftStep = std::min(1.f, dt * (.8f + roughness * 3.f));
      float variance =
          std::sqrt(3.f * (2.f - driftStep) / std::max(driftStep, 1e-6f));
      p.drift += (control.bipolar() * variance * .35f - p.drift) * driftStep;
      p.drift = clamp(p.drift, -.8f, .8f);
      float swell = clamp(1.f + p.drift * (.4f + roughness), .25f, 1.8f);
      p.breath.configure(rate,
                         (180.f + currentTone * 1450.f) *
                             (1.f + .35f * strength + lift * .65f) * swell,
                         .6f);
      p.rustle.configure(rate,
                         (1200.f + currentTone * 3600.f + profile.dry * 800.f) *
                             (1.f + lift * .35f),
                         .65f);
      p.whistle.configure(
          rate,
          (190.f + i * 173.f + currentTone * 750.f) *
              (1.f + .55f * strength + .2f * p.drift + lift * .4f),
          3.f + currentTexture * 8.f);
      // Random clusters of leaf/fabric activity, not a permanently open hiss.
      if (control.uniform() <
          dt * strength * (2.f + currentTexture * 28.f) * (1.f + roughness)) {
        p.grain = std::max(p.grain, .2f + control.uniform() * .8f);
        float duration = .018f + control.uniform() * (.16f - .1f * profile.dry);
        p.grainDecay = std::exp(-1.f / (rate * duration));
      }
      if (control.uniform() < dt * (.4f + roughness * 2.f))
        p.catchTarget =
            control.uniform() < .65f ? 0.f : .3f + .7f * control.uniform();
      p.catchLevel += (p.catchTarget - p.catchLevel) *
                      std::min(1.f, dt * (.8f + roughness * 3.f));
      float angle = heading + (i - 1) * 1.4f + p.drift * (.4f + roughness);
      p.gains = spatialGains(.5f + .4f * std::cos(angle),
                             .5f + .4f * std::sin(angle), mask);
    }
  }
  Quad process(unsigned outputs) {
    if (outputs != mask) {
      mask = outputs;
      toneFilter = {};
      centerGains = spatialGains(.5f, .5f, mask);
      for (auto& p : patches)
        p.currentGains = p.gains = spatialGains(.5f, .5f, mask);
    }
    Quad out;
    if (!mask) return out;
    speed +=
        (targetSpeed - speed) * (targetSpeed > speed ? attackCoeff : smoothing);
    if (speed < 1e-12f) {
      speed = 0.f;
      if (targetSpeed == 0.f) {
        toneFilter = {};
        return out;
      }
    }
    float common = noise.bipolar();
    low1 += (common - low1) * lowCoeff;
    low2 += (low1 - low2) * lowCoeff;
    if (std::fabs(low1) < 1e-15f) low1 = 0.f;
    if (std::fabs(low2) < 1e-15f) low2 = 0.f;
    float rumble =
        low2 * 7.f * profile.low * (1.f - .6f * currentTone) * speed * .32f;
    for (int ch = 0; ch < 4; ++ch)
      if (mask & (1u << ch)) out.channel[ch] = rumble * centerGains[ch];
    for (auto& p : patches) {
      float n = noise.bipolar();
      p.grain *= p.grainDecay;
      if (p.grain < 1e-12f) p.grain = 0.f;
      p.grainSmooth += (p.grain - p.grainSmooth) * grainAttack;
      if (p.grainSmooth < 1e-12f) p.grainSmooth = 0.f;
      float detail = (.06f + .94f * currentTexture) * p.grainSmooth;
      float moving = 1.f + p.drift * (.4f + turbulence * .6f);
      float air = p.breath.process(n) * profile.breath *
                  (.4f + .6f * currentTone) * moving;
      float rustle = p.rustle.process(n) * profile.rustle * detail *
                     (.35f + currentTone) * 2.f;
      float whistle = p.whistle.process(n) * profile.whistle * currentTexture *
                      p.catchLevel * clamp((speed - .12f) * 2.f, 0.f, 1.f);
      // Rigging/fabric catches modulate the broad band without a cyclic LFO.
      air *= 1.f + profile.flutter * currentTexture * (p.grainSmooth - .15f);
      float source = (air + rustle + whistle) * speed * .28f;
      for (int ch = 0; ch < 4; ++ch)
        if (mask & (1u << ch)) {
          p.currentGains[ch] += (p.gains[ch] - p.currentGains[ch]) * smoothing;
          out.channel[ch] += source * p.currentGains[ch];
        }
    }
    for (int ch = 0; ch < 4; ++ch)
      if (mask & (1u << ch)) {
        float sample = out.channel[ch];
        for (float& state : toneFilter[ch]) {
          state += (sample - state) * toneCoeff;
          if (std::fabs(state) < 1e-15f) state = 0.f;
          sample = state;
        }
        out.channel[ch] = sample;
      }
    return out;
  }
};
}  // namespace windchimes
