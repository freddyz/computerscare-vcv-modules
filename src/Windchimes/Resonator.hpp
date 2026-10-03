#pragma once
#include "MetalModel.hpp"
#include "PlasticModel.hpp"
#include "Types.hpp"
namespace windchimes {
// Fixed modal bank: all coefficient calculations happen at control rate.
enum class ContactKind { Striker, Tube };
class Resonator {
  struct Mode {
    float a = 0.f, b = 0.f, gain = 0.f, driveGain = 0.f;
  };
  std::array<Mode, modesPerTube> modes{};
  // GCC/Clang vector arithmetic maps to SSE on x64 and NEON on ARM64.
  // Fixed aligned storage; coefficients are packed only at control/contact
  // rate.
  using Four = float __attribute__((vector_size(16)));
  struct Bank {
    Four real{}, imag{}, a{}, b{}, drive{};
    bool limited = false;
  };
  static_assert(modesPerTube % 4 == 0, "modal bank uses four-lane groups");
  std::array<Bank, modesPerTube / 4> banks{};
  unsigned activeBanks = 0, configuredBanks = 0;

  bool awake = false;
  unsigned sleepCounter = 0;
  float frequency = -1.f, sampleRate = 0.f, decay = -1.f, brightness = -1.f,
        hardness = -1.f, shape = -1.f, body = -1.f, inharmonicity = -1.f;
  int material = -1, pulseSamples = 1, pulseLeft = 0;
  float pulseReal = 1.f, pulseImag = 0.f, pulseA = 1.f, pulseB = 0.f,
        pulseNorm = 1.f;
  float pulseBudget = 0.f;
  float pulseLevel = 0.f, ringGain = 1.f;
  // Continuous interpolation between body profiles, with no mode switching.
  static float interpolate(const float* profile, float position) {
    float p = clamp(position, 0.f, 1.f) * 3.f;
    int segment = std::min(static_cast<int>(p), 2);
    float fraction = p - segment;
    return profile[segment] +
           (profile[segment + 1] - profile[segment]) * fraction;
  }

  static float beamModeShape(int mode, float location) {
    // Free-free bending eigenfunctions, shared by wood bars and metal tubes.
    static const double beta[] = {4.73004074,  7.85320462,  10.99560784,
                                  14.13716549, 17.27875966, 20.42035225};
    double b = beta[mode], x = clamp(location, 0.f, 1.f);
    static const double sigmas[] = {0.98250221457623799, 1.0007773119072687,
                                    0.99996645012540886, 1.0000014498976566,
                                    0.99999993734438342, 1.000000002707595};
    double sigma = sigmas[mode];
    double beam = 0.5 * (std::cosh(b * x) + std::cos(b * x) -
                         sigma * (std::sinh(b * x) + std::sin(b * x)));
    return static_cast<float>(beam);
  }
  static float woodModeShape(int mode, float location, float shape) {
    float beam = beamModeShape(mode, location);
    float compact = std::cos(pi * (mode + 2.f) * clamp(location, 0.f, 1.f));
    float blend = std::min(shape * 2.f, 1.f);
    return clamp(static_cast<float>(beam) * (1.f - blend) + compact * blend,
                 -1.f, 1.f);
  }

 public:
  void reset() {
    for (auto& bank : banks) {
      bank.real = bank.imag = Four{};
      bank.limited = false;
    }
    activeBanks = 0;
    awake = false;
    pulseLeft = 0;
    pulseBudget = pulseLevel = 0.f;
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
    // Wood structural and cavity modes occupy separate slots; no high
    // structural mode morphs into a low cavity.
    static const float woodProfiles[6][4] = {{1.f, 1.f, 1.f, 1.f},
                                             {2.756f, 1.78f, 2.28f, 2.63f},
                                             {5.404f, 3.12f, 4.61f, 5.17f},
                                             {8.933f, 4.83f, 6.94f, 8.29f},
                                             {13.344f, 6.91f, 9.57f, 11.82f},
                                             {18.64f, 9.36f, 12.67f, 16.1f}};
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
      t60 *= ((1.f - extension) * interpolate(shapeLoss, c.shape) + extension);
    }
    pulseSamples = std::max(
        2, static_cast<int>(rate * (0.00018f + 0.0024f * (1.f - c.hardness) *
                                                   (1.f - c.hardness))));
    float pulseAngle = pi / (pulseSamples + 1.f);
    pulseA = std::cos(pulseAngle);
    pulseB = std::sin(pulseAngle);
    pulseNorm = std::tan(pulseAngle * 0.5f);
    ringGain = c.material == 1 ? 1.5f : (c.material == 0 ? 1.2f : 1.35f);
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
          // A common loss curve keeps the same object audible when muted.
          // Absolute frequency supplies the wood loss; extended decay relaxes
          // the lowest mode's intrinsic loss while upper modes remain damped.
          float extension = clamp((c.decay - 0.85f) / 0.15f, 0.f, 1.f);
          float muteLoss = 35.f * (1.f - c.body) * (1.f - c.body);
          float frequencyLoss = std::pow(f / 261.625565f, 1.7f);
          float intrinsic = 2.f * (i == 0 ? 1.f - extension : 1.f);
          seconds = 6.907755f /
                    (6.907755f / t60 + (intrinsic + muteLoss) * frequencyLoss);
          static const float weights[6][4] = {
              {0.65f, 0.58f, 0.6f, 0.55f},     {0.3f, 0.34f, 0.27f, 0.31f},
              {0.16f, 0.22f, 0.18f, 0.16f},    {0.1f, 0.12f, 0.12f, 0.08f},
              {0.055f, 0.065f, 0.07f, 0.045f}, {0.025f, 0.03f, 0.035f, 0.02f}};
          gain = interpolate(weights[i], c.shape) *
                 std::pow(std::max(ratio, 1.f), -(1.f - c.brightness) * 0.65f) *
                 (0.85f + 0.15f * c.body);
        } else if (i < 8) {
          // Broad, brief cavity color rather than prominent musical partials.
          f = hz * (i == 6 ? 1.43f : 2.71f);
          seconds = (0.065f + 0.055f * c.body) * (i == 6 ? 1.f : 0.6f);
          gain = hollow * (i == 6 ? 0.22f : 0.1f) * (0.55f + 0.45f * c.body);
        } else {
          // No independent click bank. The short sound is the same body's
          // structural/cavity response under increased damping.
          gain = 0.f;
        }
      } else if (c.material == 0) {
        auto modal = MetalModel::mode(i, hz, c);
        f = modal.frequency;
        seconds = modal.seconds;
        gain = modal.gain;
      } else {
        auto modal = PlasticModel::mode(i, hz, c);
        f = modal.frequency;
        seconds = modal.seconds;
        gain = modal.gain;
      }
      // Fade at the upper band limit before removing a mode during morphs.
      float bandFade = clamp((rate * 0.45f - f) / (rate * 0.05f), 0.f, 1.f);
      m.gain = gain * bandFade;
      float radius = std::exp(-6.907755f / (std::max(seconds, 0.002f) * rate));
      float angle = 2.f * pi * std::min(f, rate * 0.45f) / rate;
      m.a = radius * std::cos(angle);
      m.b = radius * std::sin(angle);
      auto& bank = banks[i / 4];
      int lane = i % 4;
      bank.a[lane] = m.a;
      bank.b[lane] = m.b;
      bank.drive[lane] = m.gain != 0.f ? m.driveGain : 0.f;
      if (m.gain == 0.f) {
        bank.real[lane] = bank.imag[lane] = bank.drive[lane] = 0.f;
      }
      normalization += m.gain;
    }
    configuredBanks = 0;
    for (int i = 0; i < modesPerTube; ++i) {
      modes[i].gain /= std::max(normalization, 1.f);
      if (modes[i].gain != 0.f) configuredBanks |= 1u << (i / 4);
    }
    // Newly enabled lanes may receive the remaining current contact pulse.
    if (pulseLeft > 0) activeBanks |= configuredBanks;
  }
  void strike(float velocity, ContactKind kind = ContactKind::Striker,
              float location = 0.5f) {
    // Broad side-to-side tube contact differs from the central striker's tap.
    int samples = kind == ContactKind::Tube
                      ? std::max(2, static_cast<int>(pulseSamples * 1.7f))
                      : pulseSamples;
    if (material == 1) {
      float contactHardness =
          kind == ContactKind::Tube ? 0.72f - 0.12f * shape : hardness;
      float duration = 0.00018f + 0.0024f * (1.f - contactHardness) *
                                      (1.f - contactHardness);
      duration *= std::pow(clamp(velocity, 0.05f, 1.f), -0.2f);
      samples = std::max(2, static_cast<int>(duration * sampleRate));
    }
    if (material == 0) {
      // Smooth finite force pulse, scaled with tube size and impact speed.
      float duration = MetalModel::contactDuration(
          frequency, hardness, velocity, kind == ContactKind::Tube);
      samples = std::max(2, static_cast<int>(duration * sampleRate));
    }
    if (material == 2) {
      samples =
          std::max(2, static_cast<int>(sampleRate *
                                       PlasticModel::contactDuration(
                                           frequency, hardness, shape, velocity,
                                           kind == ContactKind::Tube)));
    }
    float angle = (material != 1 ? 2.f : 1.f) * pi / (samples + 1.f);
    pulseA = std::cos(angle);
    pulseB = std::sin(angle);
    pulseNorm = material != 1 ? 2.f / (samples + 1.f) : std::tan(angle * 0.5f);
    pulseLevel = clamp(
        pulseBudget + velocity * (kind == ContactKind::Tube ? 0.85f : 1.f), 0.f,
        2.f);
    pulseBudget = pulseLevel;
    pulseLeft = samples;
    pulseReal = 1.f;
    pulseImag = 0.f;
    std::array<float, 6> metalResponse{};
    if (material == 0) {
      float footprint = kind == ContactKind::Tube
                            ? .025f + .025f * shape
                            : .012f + .025f * (1.f - hardness);
      for (int i = 0; i < 6; ++i)
        metalResponse[i] = (beamModeShape(i, location - footprint) +
                            2.f * beamModeShape(i, location) +
                            beamModeShape(i, location + footprint)) *
                           .25f;
    }
    for (int i = 0; i < modesPerTube; ++i) {
      if (modes[i].gain == 0.f) {
        banks[i / 4].drive[i % 4] = 0.f;
        continue;
      }
      float response = 1.f;
      if (material == 1 && i < 6) {
        float footprint = kind == ContactKind::Tube
                              ? 0.08f + 0.04f * shape
                              : 0.015f + 0.025f * (1.f - hardness);
        float coupling = (woodModeShape(i, location - footprint, shape) +
                          2.f * woodModeShape(i, location, shape) +
                          woodModeShape(i, location + footprint, shape)) *
                         0.25f;
        float radiation = woodModeShape(i, 0.f, shape);
        response = coupling * radiation;
      } else if (material == 0) {
        response = metalResponse[i % 6];
      } else if (material == 2) {
        float footprint = kind == ContactKind::Tube
                              ? .06f + .04f * shape
                              : .015f + .035f * (1.f - hardness);
        if (i < 6) {
          response = (woodModeShape(i, location - footprint, shape) +
                      2.f * woodModeShape(i, location, shape) +
                      woodModeShape(i, location + footprint, shape)) *
                     .25f;
        } else {
          response = (PlasticModel::localShape(i, location - footprint) +
                      2.f * PlasticModel::localShape(i, location) +
                      PlasticModel::localShape(i, location + footprint)) *
                     .25f;
        }
      }
      modes[i].driveGain = modes[i].gain * response;
      banks[i / 4].drive[i % 4] = modes[i].driveGain;
    }
    activeBanks = configuredBanks;
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
      excitation = pulseLevel * pulseNorm *
                   (material != 1 ? .5f * (1.f - pulseReal) : pulseImag);
      pulseBudget = std::max(0.f, pulseBudget - excitation);
      if (--pulseLeft == 0) pulseBudget = pulseLevel = 0.f;
    }
    float out = 0.f;
    for (int i = 0; i < modesPerTube / 4; ++i) {
      if (!(activeBanks & (1u << i))) continue;
      auto& bank = banks[i];
      if (excitation != 0.f || bank.limited) {
        bank.real += bank.drive * excitation;
        // Preserve the existing saturation under exceptionally dense strikes.
        // Ordinary passive ringing needs no repeated clamping.
        bank.limited = false;
        for (int lane = 0; lane < 4; ++lane) {
          bank.real[lane] = clamp(bank.real[lane], -2.f, 2.f);
          bank.limited |= bank.real[lane] * bank.real[lane] +
                              bank.imag[lane] * bank.imag[lane] >
                          3.9f;
        }
      }
      Four real = bank.a * bank.real - bank.b * bank.imag;
      bank.imag = bank.b * bank.real + bank.a * bank.imag;
      bank.real = real;
      for (int lane = 0; lane < 4; ++lane) out += bank.imag[lane];
    }
    out *= ringGain;
    if ((++sleepCounter & 255u) == 0 && pulseLeft == 0) {
      float energy = 0.f;
      activeBanks = 0;
      for (int i = 0; i < modesPerTube / 4; ++i) {
        auto& bank = banks[i];
        for (int lane = 0; lane < 4; ++lane) {
          float e = bank.real[lane] * bank.real[lane] +
                    bank.imag[lane] * bank.imag[lane];
          // Far below the whole-voice threshold: max discarded amplitude over
          // twelve modes is 1.2e-7 before ring gain. Also prevents denormals.
          if (e < 1e-16f)
            bank.real[lane] = bank.imag[lane] = 0.f;
          else {
            energy += e;
            activeBanks |= 1u << i;
          }
        }
      }
      if (energy < 1e-12f) reset();
    }
    return out;
  }
};
}  // namespace windchimes
