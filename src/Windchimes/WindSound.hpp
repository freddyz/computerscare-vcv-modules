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
    Band breath, airDetail, whistle;
    float drift = 0.f, grain = 0.f, grainSmooth = 0.f, grainDecay = .999f;
    float catchLevel = 0.f, catchTarget = 0.f;
    std::array<float, 4> gains{}, currentGains{};
  };
  struct Profile {
    float low, breath, airDetail, whistle, flutter;
  };
  Random control{0x42c716}, noise{0xab7125};
  std::array<Patch, 3> patches{};
  float rate = 48000.f, low1 = 0.f, low2 = 0.f, lowCoeff = .01f;
  float speed = 0.f, targetSpeed = 0.f, smoothing = .001f, grainAttack = .01f;
  float currentTone = .45f, currentTexture = .4f;
  float targetTone = .45f, targetTexture = .4f, targetPresence = .5f,
        presence = .5f;
  float turbulence = 0.f, heading = 0.f;
  float targetActivity = 0.f, activity = 0.f;
  Profile profile{1.f, 1.f, .15f, .1f, .1f};
  unsigned mask = 0;
  std::array<float, 4> centerGains{};
  std::array<std::array<float, 4>, 4> toneFilter{};
  float toneCoeff = .9f, attackCoeff = .001f, releaseCoeff = .00003f,
        previousStrength = 0.f, lift = 0.f;

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
    grainAttack = 1.f - std::exp(-1.f / (.09f * rate));
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
    // Texture adds overlapping, softly shaped air swells, never dry grains.
    profile.low = 1.f + 1.1f * (1.f - currentTone) + .3f * presence;
    profile.breath = .85f;
    profile.airDetail = .7f * currentTone * currentTone;
    profile.whistle = .65f * currentTone;
    profile.flutter = .35f * currentTexture;
    // Release follows the air mass rather than the instantaneous weather.
    // Texture contributes gently; even smooth air has a substantial tail.
    float releaseTime = .7f + .45f * currentTexture + .25f * lift;
    releaseCoeff = 1.f - std::exp(-1.f / (releaseTime * rate));
    // Acoustic energy rises much faster than flow speed. Gentle breezes
    // expose sparse soft air; buffeting and resonances emerge with speed.
    targetActivity = clamp(strength / .65f, 0.f, 1.f);
    targetSpeed = std::pow(clamp(strength, 0.f, 1.5f), 2.1f);
    // Low Mix reveals passing breezes; raising it fills in quieter airflow
    // and increases close buffeting, alongside the downstream level control.
    float opening =
        clamp((strength - .18f * (1.f - presence)) / .32f, 0.f, 1.f);
    targetSpeed *= presence + (1.f - presence) * opening * opening;
    // A fixed upper roll-off keeps brightness in the moving layers, rather
    // than making Tone a filter sweep across the entire wind signal.
    toneCoeff =
        1.f - std::exp(-2.f * pi * std::min(rate * .35f, 6500.f) / rate);
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
      p.breath.configure(
          rate,
          (140.f + currentTone * (220.f + 1230.f * targetActivity)) *
              (1.f + .35f * strength + lift * .65f) * swell,
          .6f);
      p.airDetail.configure(
          rate, (900.f + currentTone * 3200.f) * (1.f + lift * .35f), .65f);
      p.whistle.configure(
          rate,
          (190.f + i * 173.f + currentTone * 750.f) *
              (1.f + .55f * strength + .2f * p.drift + lift * .4f),
          3.f + currentTexture * 8.f);
      // Slow overlapping envelopes let brighter air pass intermittently.
      if (control.uniform() <
          dt * strength * (.25f + currentTone * 1.8f + currentTexture * 1.2f) *
              (1.f + roughness * .4f)) {
        p.grain = std::max(p.grain, .2f + control.uniform() * .8f);
        float duration =
            .25f + control.uniform() * (.65f + .5f * currentTexture);
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
    speed += (targetSpeed - speed) *
             (targetSpeed > speed ? attackCoeff : releaseCoeff);
    if (speed < 1e-12f) {
      speed = 0.f;
      if (targetSpeed == 0.f) {
        toneFilter = {};
        return out;
      }
    }
    activity += (targetActivity - activity) *
                (targetActivity > activity ? attackCoeff : releaseCoeff * .2f);
    float common = noise.bipolar();
    low1 += (common - low1) * lowCoeff;
    low2 += (low1 - low2) * lowCoeff;
    if (std::fabs(low1) < 1e-15f) low1 = 0.f;
    if (std::fabs(low2) < 1e-15f) low2 = 0.f;
    float rumble = low2 * 7.f * profile.low * (1.f - .2f * currentTone) *
                   (.08f + .92f * activity * activity) * speed * .32f;
    for (int ch = 0; ch < 4; ++ch)
      if (mask & (1u << ch)) out.channel[ch] = rumble * centerGains[ch];
    for (auto& p : patches) {
      float n = noise.bipolar();
      p.grain *= p.grainDecay;
      if (p.grain < 1e-12f) p.grain = 0.f;
      p.grainSmooth += (p.grain - p.grainSmooth) * grainAttack;
      if (p.grainSmooth < 1e-12f) p.grainSmooth = 0.f;
      float detail = p.grainSmooth * activity * activity;
      float moving = 1.f + p.drift * (.4f + turbulence * .6f);
      float air =
          p.breath.process(n) * profile.breath * (.55f + .25f * currentTone) *
          moving * (.15f + .85f * activity) *
          (activity +
           (1.f - activity) * clamp(.25f + p.drift + p.grainSmooth, .03f, 1.f));
      float airDetail = p.airDetail.process(n) * profile.airDetail * detail *
                        (.35f + currentTexture * .65f);
      float whistle = p.whistle.process(n) * profile.whistle *
                      (.25f + .75f * currentTexture) * detail * p.catchLevel *
                      clamp((speed - .12f) * 2.f, 0.f, 1.f);
      // Broad swells add movement without sharp flutter or a cyclic LFO.
      air *= 1.f + profile.flutter * currentTexture * (p.grainSmooth - .15f);
      float source = (air + airDetail + whistle) * speed * .28f;
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
