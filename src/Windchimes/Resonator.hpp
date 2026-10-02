#pragma once
#include "Types.hpp"
namespace windchimes {
// Fixed modal bank: all coefficient calculations happen at control rate.
enum class ContactKind { Striker, Tube };
class Resonator {
  struct Mode {
    float real = 0.f, imag = 0.f, a = 0.f, b = 0.f, gain = 0.f, driveGain = 0.f;
  };
  std::array<Mode, modesPerTube> modes{};
  Random noise{0x71ab39};
  bool awake = false;
  unsigned sleepCounter = 0;
  float frequency = -1.f, sampleRate = 0.f, decay = -1.f, brightness = -1.f,
        hardness = -1.f, shape = -1.f, body = -1.f, inharmonicity = -1.f;
  int material = -1, pulseSamples = 1, pulseLeft = 0;
  float pulseReal = 1.f, pulseImag = 0.f, pulseA = 1.f, pulseB = 0.f,
        pulseNorm = 1.f;
  float pulseBudget = 0.f;
  float pulseLevel = 0.f, transient = 0.f, noiseLow = 0.f, noiseCoeff = 0.f;
  float transientDecay = 0.f, ringGain = 1.f, impactGain = 0.f;
  // Continuous interpolation between body profiles, with no mode switching.
  static float interpolate(const float* profile, float position) {
    float p = clamp(position, 0.f, 1.f) * 3.f;
    int segment = std::min(static_cast<int>(p), 2);
    float fraction = p - segment;
    return profile[segment] +
           (profile[segment + 1] - profile[segment]) * fraction;
  }

 public:
  void reset() {
    for (auto& m : modes) m.real = m.imag = 0.f;
    awake = false;
    pulseLeft = 0;
    pulseBudget = pulseLevel = transient = noiseLow = 0.f;
  }
  bool active() const { return awake; }
  float modeFrequency(int index) const {
    return std::atan2(modes[index].b, modes[index].a) * sampleRate / (2.f * pi);
  }
  void configure(float hz, float rate, const SetConfig& c) {
    if (frequency == hz && sampleRate == rate && material == c.material &&
        decay == c.decay && brightness == c.brightness &&
        hardness == c.hardness && shape == c.shape && body == c.body &&
        inharmonicity == c.inharmonicity)
      return;
    frequency = hz;
    sampleRate = rate;
    material = c.material;
    decay = c.decay;
    brightness = c.brightness;
    hardness = c.hardness;
    shape = c.shape;
    body = c.body;
    inharmonicity = c.inharmonicity;
    // Wood geometry has its own structural modes. Cavity and contact modes
    // occupy separate slots; no high structural mode morphs into a low cavity.
    static const float woodProfiles[6][4] = {{1.f, 1.f, 1.f, 1.f},
                                             {2.756f, 1.78f, 2.28f, 2.63f},
                                             {5.404f, 3.12f, 4.61f, 5.17f},
                                             {8.933f, 4.83f, 6.94f, 8.29f},
                                             {13.344f, 6.91f, 9.57f, 11.82f},
                                             {18.64f, 9.36f, 12.67f, 16.1f}};
    static const float profiles[8][4] = {
        {1.f, 1.f, 1.f, 1.f},           {2.756f, 1.62f, 2.12f, 2.42f},
        {5.404f, 2.38f, 3.74f, 4.16f},  {8.933f, 3.31f, 5.28f, 6.38f},
        {13.344f, 4.72f, 7.43f, 9.12f}, {18.64f, 6.13f, 9.86f, 12.3f},
        {24.82f, 7.86f, 1.37f, 1.48f},  {31.88f, 10.2f, 2.68f, 2.96f}};
    float hollow = clamp((c.shape - 0.45f) / 0.55f, 0.f, 1.f);
    float minDecay =
        c.material == 0 ? 0.12f : (c.material == 1 ? 0.05f : 0.07f);
    float maxDecay = c.material == 0 ? 24.f : (c.material == 1 ? 12.f : 16.f);
    float t60 = minDecay * std::pow(maxDecay / minDecay, c.decay);
    if (c.material == 1) {
      // Most travel covers natural wood sustain; only the last 15% extends it.
      t60 = c.decay <= 0.85f
                ? 0.035f * std::pow(1.25f / 0.035f, c.decay / 0.85f)
                : 1.25f * std::pow(12.f / 1.25f, (c.decay - 0.85f) / 0.15f);
      const float shapeLoss[4] = {1.f, 0.65f, 0.42f, 0.72f};
      float extension = clamp((c.decay - 0.85f) / 0.15f, 0.f, 1.f);
      t60 *= ((1.f - extension) * interpolate(shapeLoss, c.shape) + extension) *
             (0.18f + 0.82f * c.body);
    }
    pulseSamples = std::max(
        2, static_cast<int>(rate * (0.00018f + 0.0024f * (1.f - c.hardness) *
                                                   (1.f - c.hardness))));
    float pulseAngle = pi / (pulseSamples + 1.f);
    pulseA = std::cos(pulseAngle);
    pulseB = std::sin(pulseAngle);
    pulseNorm = std::tan(pulseAngle * 0.5f);
    noiseCoeff =
        1.f - std::exp(-2.f * pi * (650.f + c.brightness * 6500.f) / rate);
    transientDecay =
        std::exp(-1.f / (rate * (0.002f + (1.f - c.hardness) * 0.009f)));
    ringGain = 0.12f + 0.88f * c.body;
    impactGain = (1.f - c.body) *
                 (c.material == 0 ? 0.15f : (c.material == 1 ? 0.7f : 0.45f));
    float normalization = 0.f;
    for (int i = 0; i < modesPerTube; ++i) {
      auto& m = modes[i];
      float f = hz, seconds = t60, gain = 0.f;
      if (c.material == 1) {
        if (i < 6) {
          float natural = interpolate(woodProfiles[i], c.shape);
          // Harmonic -> natural geometry -> exaggerated spacing, continuously.
          float morph = c.inharmonicity <= 0.5f
                            ? c.inharmonicity * 2.f
                            : 1.f + (c.inharmonicity - 0.5f) * 0.5f;
          float ratio = (i + 1.f) * std::pow(natural / (i + 1.f), morph);
          f = hz * ratio;
          // Losses depend on actual frequency, not slot number. The long-decay
          // extension sustains the fundamental without sustaining bright bells.
          seconds =
              t60 / (1.f + 0.65f * std::pow(std::max(ratio - 1.f, 0.f), 1.3f));
          if (i > 0) seconds = std::min(seconds, 0.28f / (1.f + 0.08f * ratio));
          const float weights[] = {0.48f, 0.3f, 0.2f, 0.12f, 0.07f, 0.04f};
          gain = weights[i] *
                 std::pow(std::max(ratio, 1.f), -(1.f - c.brightness) * 0.65f) *
                 (0.25f + 0.75f * c.body);
        } else if (i < 8) {
          // Broad, brief cavity color rather than prominent musical partials.
          f = hz * (i == 6 ? 1.43f : 2.71f);
          seconds = (0.025f + 0.09f * c.body) * (i == 6 ? 1.f : 0.6f);
          gain = hollow * (i == 6 ? 0.22f : 0.1f) * (0.55f + 0.45f * c.body);
        } else {
          const float contactHz[] = {430.f, 970.f, 1830.f, 3270.f};
          float shapeSize = 0.8f + 0.4f * c.shape;
          f = contactHz[i - 8] * shapeSize *
              (0.75f + 0.25f * std::sqrt(hz / 261.625565f));
          seconds =
              (0.014f + 0.012f * (1.f - c.hardness)) / (1.f + (i - 8) * 0.6f);
          gain = (0.6f - 0.42f * c.body) *
                 std::exp(-(i - 8) * (0.8f + (1.f - c.brightness) * 0.5f));
        }
      } else if (i < 8) {
        float natural = interpolate(profiles[i], c.shape) *
                        (c.material == 2 ? 1.f + 0.022f * i : 1.f);
        float morph = c.inharmonicity <= 0.5f
                          ? c.inharmonicity * 2.f
                          : 1.f + (c.inharmonicity - 0.5f) * 0.5f;
        float ratio = (i + 1.f) * std::pow(natural / (i + 1.f), morph);
        f = hz * ratio;
        seconds = t60 / (1.f + i * (c.material == 0 ? 0.18f : 0.8f));
        if (i >= 6) seconds = t60 * (0.12f + hollow * 0.2f);
        gain = std::exp(
            -i * ((c.material == 0 ? 1.05f : 1.4f) - c.brightness * 0.8f));
        if (i >= 6)
          gain = gain * (1.f - hollow) + hollow * (i == 6 ? 0.32f : 0.16f);
      }
      float softness =
          1.f / (1.f + f * f * std::pow((1.f - c.hardness) * 0.0006f, 2.f));
      // Fade at the upper band limit before removing a mode during morphs.
      float bandFade = clamp((rate * 0.45f - f) / (rate * 0.05f), 0.f, 1.f);
      m.gain = gain * softness * bandFade;
      float radius = std::exp(-6.907755f / (std::max(seconds, 0.002f) * rate));
      float angle = 2.f * pi * std::min(f, rate * 0.45f) / rate;
      m.a = radius * std::cos(angle);
      m.b = radius * std::sin(angle);
      if (m.gain == 0.f) m.real = m.imag = 0.f;
      normalization += m.gain;
    }
    for (auto& m : modes) m.gain /= std::max(normalization, 1.f);
    if (c.material == 1) {
      ringGain = 1.f;
      impactGain = 0.f;
    }
  }
  void strike(float velocity, ContactKind kind = ContactKind::Striker,
              float location = 0.5f) {
    // Broad side-to-side tube contact differs from the central striker's tap.
    int samples = kind == ContactKind::Tube
                      ? std::max(2, static_cast<int>(pulseSamples * 1.7f))
                      : pulseSamples;
    float angle = pi / (samples + 1.f);
    pulseA = std::cos(angle);
    pulseB = std::sin(angle);
    pulseNorm = std::tan(angle * 0.5f);
    pulseLevel = clamp(
        pulseBudget + velocity * (kind == ContactKind::Tube ? 0.85f : 1.f), 0.f,
        2.f);
    pulseBudget = pulseLevel;
    pulseLeft = samples;
    pulseReal = 1.f;
    pulseImag = 0.f;
    for (int i = 0; i < modesPerTube; ++i) {
      float response = 1.f;
      if (i > 0 && (material != 1 || i < 6)) {
        float position = std::sin(pi * (i + 1.f) * clamp(location, 0.f, 1.f));
        response = 0.7f + 0.3f * position * position;
      }
      if (kind == ContactKind::Tube && material == 1 && i >= 8)
        response *= 0.65f;
      modes[i].driveGain = modes[i].gain * response;
    }
    transient =
        material == 1
            ? 0.f
            : clamp(transient +
                        velocity * (kind == ContactKind::Tube ? 0.55f : 1.f),
                    0.f, 2.f);
    awake = true;
    sleepCounter = 0;
  }
  float process() {
    if (!awake) return 0.f;
    float excitation = 0.f;
    if (pulseLeft > 0) {
      float real = pulseA * pulseReal - pulseB * pulseImag;
      pulseImag = pulseB * pulseReal + pulseA * pulseImag;
      pulseReal = real;
      excitation = pulseLevel * pulseNorm * pulseImag;
      pulseBudget = std::max(0.f, pulseBudget - excitation);
      if (--pulseLeft == 0) pulseBudget = pulseLevel = 0.f;
    }
    float out = 0.f;
    for (auto& m : modes) {
      if (m.gain == 0.f) continue;
      m.real = clamp(m.real + excitation * m.driveGain, -2.f, 2.f);
      float real = m.a * m.real - m.b * m.imag;
      m.imag = m.b * m.real + m.a * m.imag;
      m.real = real;
      out += m.imag;
    }
    transient *= transientDecay;
    if (material != 1) noiseLow += (noise.bipolar() - noiseLow) * noiseCoeff;
    out = out * ringGain + noiseLow * transient * impactGain;
    if ((++sleepCounter & 255u) == 0 && pulseLeft == 0) {
      float energy = transient * transient;
      for (const auto& m : modes) energy += m.real * m.real + m.imag * m.imag;
      if (energy < 1e-12f) reset();
    }
    return out;
  }
};
}  // namespace windchimes
