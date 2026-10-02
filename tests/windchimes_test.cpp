#include "Windchimes/Engine.hpp"
#include "Windchimes/Presets.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>

void require(bool condition, const char* message) {
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
  namespace wc = windchimes;
  wc::SetConfig config; config.enabled = true;
  require(std::fabs(wc::tubeFrequency(config, 5, 0.f) / wc::tubeFrequency(config, 0, 0.f) - 2.f) < 0.001f, "scale repeats at the octave");
  require(std::fabs(wc::tubeFrequency(config, 0, 1.f) / wc::tubeFrequency(config, 0, 0.f) - 2.f) < 0.001f, "1V/oct transposition");
  config.divisions = 19;
  require(std::fabs(wc::tubeFrequency(config, 1, 0.f) / wc::tubeFrequency(config, 0, 0.f) - std::exp2(3.f / 19.f)) < 0.001f, "scale maps onto selected EDO");
  for (float rate : {44100.f, 48000.f, 96000.f}) {
    for (int material = 0; material < 3; ++material) {
      wc::Engine engine; engine.setSampleRate(rate);
      config = wc::SetConfig(); config.enabled = true; config.material = material; config.decay = 0.f;
      engine.configure(0, config, 0.f);
      auto silent = engine.process(0, 0, 0, 0);
      require(silent.left == 0.f && silent.right == 0.f, "unstruck tube starts silent");
      engine.strike(0);
      double energy = 0;
      for (int i = 0; i < static_cast<int>(rate * 3.f); ++i) {
        auto out = engine.process(0, 0, 0, 0);
        require(std::isfinite(out.left) && std::isfinite(out.right), "finite output at all sample rates");
        energy += out.left * out.left + out.right * out.right;
      }
      require(energy > 0.01, "each material produces ringing");
      auto decayed = engine.process(0, 0, 0, 0);
      require(decayed.left == 0.f && decayed.right == 0.f, "short voices sleep after decay");
      engine.setSampleRate(rate == 96000 ? 44100 : 96000);
      engine.configure(0, config, 0.f); engine.strike(0);
      require(std::isfinite(engine.process(0, 0, 0, 0).left), "sample rate changes remain stable");
    }
  }
  // Morphing must be audible in every material, continuous at profile joins,
  // and stable while controls change during a ringing voice.
  for (int material = 0; material < 3; ++material) {
    config = wc::SetConfig(); config.material = material;
    auto difference = [&](float sa, float sb, float ba, float bb) {
      wc::Resonator a, b;
      config.shape = sa; config.body = ba; a.configure(220.f, 48000.f, config);
      config.shape = sb; config.body = bb; b.configure(220.f, 48000.f, config);
      a.strike(0.8f); b.strike(0.8f);
      double error = 0.f;
      for (int i = 0; i < 24000; ++i) {
        double delta = a.process() - b.process(); error += delta * delta;
      }
      return error;
    };
    require(difference(0.f, 1.f, 0.65f, 0.65f) > 0.01, "shape changes every material's modal response");
    require(difference(0.5f, 0.5f, 0.f, 1.f) > 0.01, "body changes impact/resonance balance for every material");
    for (float boundary : {1.f / 3.f, 2.f / 3.f})
      require(difference(boundary - 1e-6f, boundary + 1e-6f, 0.65f, 0.65f) < 1e-4,
        "shape profiles morph continuously across interpolation boundaries");
    wc::Resonator sweep; config.decay = 1.f; sweep.configure(220.f, 48000.f, config); sweep.strike(1.f);
    for (int i = 0; i < 96000; ++i) {
      if (i % 240 == 0) { config.shape = i / 96000.f; config.body = 1.f - config.shape; sweep.configure(220.f, 48000.f, config); }
      float sample = sweep.process();
      require(std::isfinite(sample) && std::fabs(sample) < 4.f, "live timbre morph remains bounded");
    }
  }
  wc::Resonator longWood; config = wc::SetConfig(); config.material = 1;
  config.decay = 1.f; config.body = 1.f; longWood.configure(220.f, 48000.f, config); longWood.strike(1.f);
  double woodTail = 0.f;
  for (int i = 0; i < 480000; ++i) { float sample = longWood.process(); if (i >= 432000) woodTail += sample * sample; }
  require(longWood.active() && woodTail > 1e-4, "maximum wood decay retains an audible late tail without timeout");
  for (int i = 0; i < 960000 && longWood.active(); ++i) longWood.process();
  require(!longWood.active(), "extended wood tail eventually sleeps at the energy threshold");
  for (int material = 0; material < 3; ++material) {
    auto presets = wc::soundPresets(material);
    require(material != 1 || presets.count == 8, "wood offers eight sound presets");
    wc::Resonator previous; bool hasPrevious = false;
    for (int index = 0; index < presets.count; ++index) {
      const auto& p = presets.items[index];
      config = wc::SetConfig(); config.material = material;
      config.decay = p.decay; config.brightness = p.brightness; config.hardness = p.hardness;
      config.shape = p.shape; config.body = p.body; config.inharmonicity = p.inharmonicity;
      wc::Resonator current; current.configure(220.f, 48000.f, config); current.strike(0.8f);
      wc::Resonator saved = current;
      double energy = 0.f, difference = 0.f;
      for (int i = 0; i < 12000; ++i) {
        float sample = current.process();
        require(std::isfinite(sample) && std::fabs(sample) < 4.f, "material presets produce bounded audio");
        energy += sample * sample;
        if (hasPrevious) { double delta = sample - previous.process(); difference += delta * delta; }
      }
      require(energy > 0.01, "every preset is audible");
      require(!hasPrevious || difference > 0.01, "neighboring presets produce distinct sound responses");
      previous = saved; hasPrevious = true;
    }
  }
  for (int material = 0; material < 3; ++material) {
    config = wc::SetConfig(); config.material = material;
    wc::Resonator harmonic, natural, stretched;
    config.inharmonicity = 0.f; harmonic.configure(220.f, 48000.f, config);
    config.inharmonicity = 0.5f; natural.configure(220.f, 48000.f, config);
    config.inharmonicity = 1.f; stretched.configure(220.f, 48000.f, config);
    for (int mode = 0; mode < 6; ++mode)
      require(std::fabs(harmonic.modeFrequency(mode) - 220.f*(mode+1)) < 0.01f,
        "inharmonicity zero gives harmonic structural spacing");
    require(std::fabs(natural.modeFrequency(0)-220.f) < 0.01f && std::fabs(stretched.modeFrequency(0)-220.f) < 0.01f,
      "inharmonicity preserves the tuned fundamental");
    require(std::fabs(natural.modeFrequency(2)-stretched.modeFrequency(2)) > 1.f,
      "inharmonicity changes higher structural modes in every material");
    wc::Resonator striker, pair; striker.configure(220.f, 48000.f, config); pair.configure(220.f, 48000.f, config);
    striker.strike(0.8f, wc::ContactKind::Striker, 0.3f); pair.strike(0.8f, wc::ContactKind::Tube, 0.3f);
    double difference = 0.f;
    for (int i = 0; i < 4800; ++i) { double d = striker.process()-pair.process(); difference += d*d; }
    require(difference > 0.001, "tube contacts have a distinct acoustic impulse from striker contacts");
  }
  for (float rate : {44100.f, 48000.f, 96000.f}) {
    config = wc::SetConfig(); config.material = 1; config.body = 0.f; config.decay = 0.5f;
    wc::Resonator soft, hard;
    config.hardness = 0.f; soft.configure(261.625565f, rate, config);
    config.hardness = 1.f; hard.configure(261.625565f, rate, config);
    soft.strike(1.f); hard.strike(1.f);
    double softDerivative = 0.f, hardDerivative = 0.f, softEnergy = 0.f, hardEnergy = 0.f;
    float lastSoft = 0.f, lastHard = 0.f, softPeak = 0.f, hardPeak = 0.f;
    for (int i = 0; i < static_cast<int>(rate * 0.02f); ++i) {
      float a = soft.process(), b = hard.process();
      softPeak = std::max(softPeak,std::fabs(a)); hardPeak = std::max(hardPeak,std::fabs(b));
      softEnergy += a*a; hardEnergy += b*b;
      softDerivative += (a-lastSoft)*(a-lastSoft); hardDerivative += (b-lastHard)*(b-lastHard);
      lastSoft=a; lastHard=b;
    }
    require(softPeak < hardPeak && softEnergy < hardEnergy,
      "low hardness rounds off the dry wood attack rather than producing a louder burst");
    require(softDerivative/softEnergy < hardDerivative/hardEnergy,
      "soft wood contact has less high-frequency attack energy");
  }
  wc::Engine engine; engine.setSampleRate(48000);
  config = wc::SetConfig(); config.enabled = true; config.tubes = wc::maxTubes; config.level = 1.f;
  for (int i = 0; i < wc::maxSets; ++i) engine.configure(i, config, 0.f);
  double energy = 0;
  for (int i = 0; i < 480000; ++i) {
    if (i % 2400 == 0) { engine.gust(); for (int s = 0; s < wc::maxSets; ++s) engine.strike(s); }
    auto out = engine.process(1, 1, 1, 1);
    require(std::isfinite(out.left) && std::fabs(out.left) < 20.f && std::isfinite(out.right) && std::fabs(out.right) < 20.f, "dense repeated collisions remain bounded");
    energy += out.left * out.left;
  }
  require(energy > 1.f, "dense scene produces audio");
  config.enabled = false;
  for (int i = 0; i < wc::maxSets; ++i) engine.configure(i, config, 0.f);
  wc::Stereo removed;
  for (int i = 0; i < 48000; ++i) removed = engine.process(0, 0, 0, 0);
  require(std::fabs(removed.left) < 1e-6f && std::fabs(removed.right) < 1e-6f, "removing sets clears ringing and wind mix fades silently");
  wc::Engine breeze; breeze.setSampleRate(48000);
  config.enabled = true; config.decay = 0.f; breeze.configure(0, config, 0.f);
  energy = 0;
  for (int i = 0; i < 480000; ++i) { auto out = breeze.process(0.8f, 0.7f, 0.4f, 0.f); energy += out.left * out.left; }
  require(energy > 0.01, "internal wind causes chime collisions with audible wind muted");
  // Independently verify pair impacts, rather than inferring them from mixed audio.
  for (int count : {1, 6, 12}) {
    wc::Motion motion; config.tubes = count; config.swing = 0.8f;
    int collisions = 0;
    for (int i = 0; i < 16000; ++i) {
      if (i % 400 == 0) motion.kick(i * 0.37f);
      motion.step(0.0025f, std::sin(i * 0.013f), std::cos(i * 0.009f), config, 0,
        [](int, float) {}, [&](int a, int b, float velocity) {
          require(a != b && a >= 0 && b < count && velocity > 0.f, "pair impact excites two distinct valid tubes");
          ++collisions;
        });
    }
    require(count == 1 ? collisions == 0 : collisions > 0, "moving tubes collide only within multi-tube sets");
  }
  // Strong wind alone must excite tube pairs, including sparse two-tube sets.
  for (int count = 2; count <= wc::maxTubes; ++count) {
    wc::Motion windyMotion, calmMotion; wc::Wind weather;
    config.tubes = count; config.swing = 0.5f;
    int windyPairs = 0, calmPairs = 0;
    for (int i = 0; i < 48000; ++i) {
      weather.step(0.0025f, 0.9f, 0.7f, 0.5f);
      windyMotion.step(0.0025f, weather.x, weather.y, config, 0,
        [](int, float) {}, [&](int, int, float) { ++windyPairs; });
      calmMotion.step(0.0025f, 0.f, 0.f, config, 0,
        [](int, float) {}, [&](int, int, float) { ++calmPairs; });
    }
    require(windyPairs > 0, "strong wind drives tube-pair collisions without manual impulses");
    require(calmPairs == 0, "stationary tubes do not generate phantom pair strikes");
  }
  wc::Motion steadyMotion; config.tubes = 6; config.swing = 0.5f;
  for (int i = 0; i < 16000; ++i)
    steadyMotion.step(0.0025f, 0.8f, 0.f, config, 0,
      [](int, float) {}, [](int, int, float) {});
  for (int t = 0; t < config.tubes; ++t)
    require(std::fabs(steadyMotion.tubeAngle(t)) < 0.2f,
      "sustained wind settles tubes near vertical instead of holding them up");
  wc::Motion normalMotion; wc::Wind normalWeather;
  normalWeather.configure(48000.f, 0.45f, 0.4f); config.tubes = 6; config.swing = 0.5f;
  int normalPairs = 0;
  for (int i = 0; i < 48000; ++i) {
    normalWeather.step(0.0025f, 0.5f, 0.55f, 0.35f);
    normalMotion.step(0.0025f, normalWeather.x, normalWeather.y, config, 0,
      [](int, float) {}, [&](int, int, float) { ++normalPairs; });
  }
  require(normalPairs > 0, "default breeze creates tube-pair impacts without manual gusts");
  for (float rate : {44100.f, 48000.f, 96000.f}) {
    auto small = std::unique_ptr<wc::Reverb>(new wc::Reverb);
    auto large = std::unique_ptr<wc::Reverb>(new wc::Reverb);
    small->configure(rate, 0.f); large->configure(rate, 1.f);
    auto dryProbe = small->process(wc::Stereo(0.3f, -0.4f), 0.f);
    require(dryProbe.left == 0.3f && dryProbe.right == -0.4f, "reverb dry endpoint is exact");
    small->configure(rate + 1.f, 0.f); small->configure(rate, 0.f);
    double smallTail = 0, largeTail = 0, stereoDifference = 0;
    for (int i = 0; i < static_cast<int>(rate * 3.f); ++i) {
      wc::Stereo input(i == 0 ? 1.f : 0.f, 0.f);
      auto near = small->process(input, 1.f);
      auto wet = large->process(input, 1.f);
      require(std::isfinite(wet.left) && std::isfinite(wet.right) && std::fabs(wet.left) < 2.f && std::fabs(wet.right) < 2.f, "reverb impulse response remains bounded");
      if (i > rate) { smallTail += near.left * near.left + near.right * near.right; largeTail += wet.left * wet.left + wet.right * wet.right; }
      stereoDifference += (wet.left - wet.right) * (wet.left - wet.right);
    }
    require(largeTail > 1e-5 && stereoDifference > 0.001, "large room has a sustained stereo tail");
    require(largeTail > smallTail * 20, "size increases reverberation decay");
    // Size modulation and sample-rate changes must remain finite.
    for (int i = 0; i < 48000; ++i) {
      if (i % 120 == 0) large->configure(rate, (i / 120) % 2);
      auto out = large->process(wc::Stereo(0.1f, -0.1f), 0.5f);
      require(std::isfinite(out.left) && std::fabs(out.left) < 2.f, "changing reverb size stays stable");
    }
    large->configure(rate == 96000.f ? 44100.f : 96000.f, 0.5f);
    require(large->process(wc::Stereo(), 1.f).left == 0.f, "rate changes clear incompatible delay state");
  }
  wc::Wind deep, airy, textured;
  deep.configure(48000, 0, 0); airy.configure(48000, 1, 0); textured.configure(48000, 0, 1);
  double toneDifference = 0, textureDifference = 0;
  for (int i = 0; i < 96000; ++i) {
    if (i % 120 == 0) { deep.step(0.0025f, 0.8f, 0.6f, 0.5f); airy.step(0.0025f, 0.8f, 0.6f, 0.5f); textured.step(0.0025f, 0.8f, 0.6f, 0.5f); }
    auto a = deep.process(48000), b = airy.process(48000), c = textured.process(48000);
    require(std::isfinite(c.left) && std::fabs(c.left) < 2.f, "layered wind remains finite");
    require(deep.strength == airy.strength && deep.strength == textured.strength, "wind sound controls preserve physical weather");
    toneDifference += (a.left - b.left) * (a.left - b.left);
    textureDifference += (a.left - c.left) * (a.left - c.left);
  }
  require(toneDifference > 0.01 && textureDifference > 0.01, "both wind controls audibly change synthesis");
  for (float position : {0.f, 0.5f, 1.f}) {
    auto positioned = std::unique_ptr<wc::Engine>(new wc::Engine);
    positioned->setSampleRate(48000);
    config = wc::SetConfig(); config.enabled = true; config.x = position;
    positioned->configure(0, config, 0.f); positioned->strike(0);
    double leftEnergy = 0, rightEnergy = 0, difference = 0;
    for (int i = 0; i < 24000; ++i) {
      auto out = positioned->process(0, 0, 0, 0);
      leftEnergy += out.left * out.left; rightEnergy += out.right * out.right;
      difference += (out.left - out.right) * (out.left - out.right);
    }
    require(leftEnergy + rightEnergy > 0.01, "positioned sets produce audio");
    if (position == 0.f) require(rightEnergy < leftEnergy * 1e-9, "left placement pans all tubes left");
    if (position == 1.f) require(leftEnergy < rightEnergy * 1e-9, "right placement pans all tubes right");
    if (position == 0.5f) require(difference < 1e-9, "center placement pans all tubes equally");
  }
  auto distant = std::unique_ptr<wc::Engine>(new wc::Engine);
  auto nearby = std::unique_ptr<wc::Engine>(new wc::Engine);
  distant->setSampleRate(48000.f); nearby->setSampleRate(48000.f);
  config = wc::SetConfig(); config.enabled = true; config.brightness = 1.f; config.hardness = 1.f;
  config.y = 0.f; distant->configure(0,config,0.f);
  config.y = 1.f; nearby->configure(0,config,0.f);
  distant->strike(0); nearby->strike(0);
  double farEnergy=0.f,nearEnergy=0.f,farDifference=0.f,nearDifference=0.f;
  float lastFar=0.f,lastNear=0.f;
  for(int i=0;i<48000;++i) {
    float a=distant->process(0.f,0.f,0.f,0.f).left;
    float b=nearby->process(0.f,0.f,0.f,0.f).left;
    farEnergy+=a*a; nearEnergy+=b*b;
    farDifference+=(a-lastFar)*(a-lastFar); nearDifference+=(b-lastNear)*(b-lastNear);
    lastFar=a;lastNear=b;
  }
  require(farEnergy>0.f && farEnergy<nearEnergy*0.3f,"Far is quieter than Near for the same physical strike");
  require(farDifference/farEnergy<nearDifference/nearEnergy,"Far attenuates upper-frequency detail as well as level");
  auto depthEnergy = [&](float nearness,float wet) {
    auto scene=std::unique_ptr<wc::Engine>(new wc::Engine);
    scene->setSampleRate(48000.f); scene->configureEffects(wet,0.6f,0.5f,0.5f);
    config=wc::SetConfig(); config.enabled=true; config.y=nearness;
    scene->configure(0,config,0.f); scene->strike(0);
    double result=0.f;
    for(int i=0;i<144000;++i) {
      auto out=scene->process(0.f,0.f,0.f,0.f); result+=out.left*out.left+out.right*out.right;
    }
    return result;
  };
  double farDry=depthEnergy(0.f,0.f),nearDry=depthEnergy(1.f,0.f);
  double farRoom=depthEnergy(0.f,1.f),nearRoom=depthEnergy(1.f,1.f);
  require(farDry<nearDry*0.02,"far-edge direct sound falls by at least 17 dB");
  require(farRoom>0.f && nearRoom>farRoom,"distant reflections remain audible but lower in absolute level");
  require(farRoom/farDry>3.f*nearRoom/nearDry,"distance increases reflected-to-direct energy rather than reverberating the already attenuated dry mix");
  for (int count : {1, 6, 12}) {
    wc::Motion suspended; config.tubes = count; config.swing = 1.f;
    suspended.configure(count);
    int strikerHits = 0, pairHits = 0;
    auto strikerHit = [&](int t, float) {
      auto body = suspended.tubeShape(t);
      wc::Point striker = suspended.strikerPosition();
      auto near = wc::closestPoint(striker, body.top(), body.bottom());
      auto delta = near - striker;
      require(delta.dot(delta) <= std::pow(wc::strikerRadius + body.radius + 1e-5f, 2), "striker audio events require visible geometry contact");
      ++strikerHits;
    };
    auto pairHit = [&](int a, int b, float) {
      auto first = suspended.tubeShape(a);
      auto second = suspended.tubeShape(b);
      require(wc::capsuleContact(first, second).distance <= first.radius + second.radius + 1e-5f, "pair audio events require rendered tubes to touch");
      ++pairHits;
    };
    for (int i = 0; i < 12000; ++i) {
      if (i % 600 == 0) suspended.kick(i * 0.19f);
      suspended.step(0.0025f, std::sin(i * 0.02f) * 3.f, std::cos(i * 0.015f), config, 0, strikerHit, pairHit);
      require(std::fabs(suspended.strikerAngle()) <= wc::strikerSwingLimit + 1e-6f, "striker displacement is constrained by its suspension");
      auto striker = suspended.strikerPosition();
      auto cord = striker - wc::Point(0.f, wc::pivotY);
      require(std::fabs(cord.dot(cord) - wc::strikerLength * wc::strikerLength) < 1e-5f, "striker string length remains fixed under gusts");
      for (int t = 0; t < count; ++t) {
        require(std::fabs(suspended.tubeAngle(t)) <= wc::tubeSwingLimit + 1e-6f, "tube swing remains bounded after collisions");
        auto body = suspended.tubeShape(t);
        auto string = body.top() - wc::tubeAnchorPoint(t, count);
        float length = wc::tubeLength(t) - wc::tubeHalfLength(t);
        require(std::fabs(string.dot(string) - length * length) < 1e-5f, "tube strings never stretch or disconnect");
      }
    }
    require(strikerHits > 0, "physical striker reaches tubes for every tested set size");
    require(count == 1 ? pairHits == 0 : pairHits > 0, "bounded pendulums preserve tube-pair collisions");
    suspended.reset(); suspended.configure(count);
    auto before = wc::Point(suspended.x, suspended.y);
    suspended.strike(0);
    require(suspended.x == before.x && suspended.y == before.y, "manual strike pushes without teleporting the striker");
  }
  wc::Motion settling; config.tubes = 8; config.swing = 1.f;
  settling.configure(config.tubes); settling.kick(wc::pi * 0.25f);
  int dissipativeHits = 0; bool movedInDepth = false;
  for (int i = 0; i < 24000; ++i) {
    float impactEnergy = -1.f;
    auto impact = [&]() { impactEnergy = settling.kineticEnergy(); ++dissipativeHits; };
    settling.step(0.0025f, 0.f, 0.f, config, 0,
      [&](int, float) { impact(); }, [&](int, int, float) { impact(); });
    if (impactEnergy >= 0.f)
      require(settling.kineticEnergy() <= impactEnergy + 1e-5f,
        "contact impulses remove kinetic energy");
    movedInDepth = movedInDepth || std::fabs(settling.strikerDepth()) > 0.02f;
  }
  require(movedInDepth && dissipativeHits > 0, "striker swings in depth and impacts the circular tube array");
  require(settling.kineticEnergy() < 1e-7f, "motion settles after a gust even at maximum swing");
  require(std::hypot(settling.strikerAngle(), settling.strikerDepth()) < 0.002f,
    "striker returns to vertical in both axes");
  for (int t = 0; t < config.tubes; ++t)
    require(std::hypot(settling.tubeAngle(t), settling.tubeDepth(t)) < 0.002f,
      "every tube returns to vertical in both axes");
  auto physicalStrike = std::unique_ptr<wc::Engine>(new wc::Engine);
  physicalStrike->setSampleRate(48000.f); config = wc::SetConfig(); config.enabled = true;
  physicalStrike->configure(0, config, 0.f); physicalStrike->strike(0);
  auto beforeContact = physicalStrike->process(0, 0, 0, 0);
  require(beforeContact.left == 0.f && beforeContact.right == 0.f, "manual audition waits for a real contact before emitting audio");
  bool sounded = false, highlighted = false;
  for (int i = 0; i < 48000; ++i) {
    auto out = physicalStrike->process(0, 0, 0, 0);
    sounded = sounded || std::fabs(out.left) > 1e-6f;
    for (int t = 0; t < config.tubes; ++t) highlighted = highlighted || physicalStrike->tubeFlash(0, t) > 0.f;
    if (std::fabs(out.left) > 1e-6f) require(highlighted, "audible strikes have matching animation events");
  }
  require(sounded && highlighted, "manual audition produces both physical contact and sound");
  std::puts("windchimes DSP tests passed");
}
