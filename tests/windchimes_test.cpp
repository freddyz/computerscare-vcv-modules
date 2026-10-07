#include "Windchimes/Engine.hpp"
#include "Windchimes/Presets.hpp"
#include "Windchimes/Selection.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>

void require(bool condition, const char* message) {
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
  namespace wc = windchimes;
  // The spatial field stays continuous, varies more with turbulence, and
  // applies no force once weather is off. Identical sets must drift apart.
  {
    wc::Wind calmField, roughField;
    double calmDifference = 0, roughDifference = 0;
    for (int i = 0; i < 4000; ++i) {
      calmField.step(0.0025f, 0.7f, 0.5f, 0.f);
      roughField.step(0.0025f, 0.7f, 0.5f, 1.f);
      auto a = calmField.flowAt(0.2f, 0.3f, 0);
      auto b = calmField.flowAt(0.8f, 0.7f, 1);
      auto c = roughField.flowAt(0.2f, 0.3f, 0);
      auto d = roughField.flowAt(0.8f, 0.7f, 1);
      calmDifference += (a.left-b.left)*(a.left-b.left) + (a.right-b.right)*(a.right-b.right);
      roughDifference += (c.left-d.left)*(c.left-d.left) + (c.right-d.right)*(c.right-d.right);
      auto neighbor = roughField.flowAt(0.2001f, 0.3001f, 0);
      require(std::fabs(c.left-neighbor.left) + std::fabs(c.right-neighbor.right) < 0.01f,
        "dragging through the wind field is continuous");
    }
    require(calmDifference > 0.01 && roughDifference > calmDifference * 1.5,
      "turbulence increases variation between stage positions");
    calmField.step(0.0025f, 0.f, 0.5f, 0.5f);
    auto off = calmField.flowAt(0.2f, 0.3f, 0);
    require(off.left == 0.f && off.right == 0.f, "local eddies stop driving when wind is off");
    std::unique_ptr<wc::Engine> scene(new wc::Engine()); scene->setSampleRate(48000.f);
    wc::SetConfig same; same.enabled = true;
    scene->configure(0, same, 0.f); scene->configure(1, same, 0.f);
    double separation = 0;
    for (int i = 0; i < 480000; ++i) {
      scene->process(0.7f, 0.5f, 0.6f, 0.f);
      if (i % 120 == 0) {
        float dx = scene->strikerAngle(0) - scene->strikerAngle(1);
        float dz = scene->strikerDepth(0) - scene->strikerDepth(1);
        separation += dx*dx + dz*dz;
      }
    }
    require(separation / 4000 > 0.001, "identical overlapping sets develop independent striker motion");
  }
  for(float pan:{0.f,.25f,.5f,.75f,1.f}) for(float near:{0.f,.5f,1.f}) {
    auto p=wc::migrateStagePosition(pan,near);
    require(std::fabs(wc::stageDistance(p.x,p.y)-(1.f-near))<1e-5f,
      "legacy migration preserves distance");
    require(p.y<=.50001f && (pan<=.5f ? p.x<=.50001f : p.x>=.49999f),
      "legacy migration preserves horizontal side in the front half");
  }
  {
    std::unique_ptr<wc::Reverb> room(new wc::Reverb);room->configure(48000,.6f);
    double differences[3]={};
    for(int i=0;i<48000;++i) {
      auto wet=room->processWetQuad({i==0?1.f:0.f,i==0?.7f:0.f});
      for(int ch=1;ch<4;++ch) {
        float d=wet.channel[0]-wet.channel[ch];differences[ch-1]+=d*d;
      }
    }
    for(double d:differences) require(d>1e-5,"quad room returns have distinct diffusion patterns");
  }
  // Every connected layout preserves source power and includes missing
  // directions. A center source stays balanced; radial distance is symmetric.
  for (unsigned mask = 1; mask < 16; ++mask) {
    for (float x : {0.f, 0.5f, 1.f}) for (float y : {0.f, 0.5f, 1.f}) {
      auto g = wc::spatialGains(x,y,mask); float power=0.f;
      for(int ch=0;ch<4;++ch) {
        require(std::isfinite(g[ch]) && g[ch]>=0.f,"quad gains are finite and nonnegative");
        require((mask&(1u<<ch)) || g[ch]==0.f,"unused jacks receive no signal");
        power+=g[ch]*g[ch];
      }
      require(std::fabs(power-1.f)<1e-5f,"all patch combinations preserve source power");
    }
  }
  for (int ch=0;ch<4;++ch) {
    auto g=wc::spatialGains(ch%2, ch/2, 15);
    require(g[ch]==1.f,"speaker corner routes to its corresponding quad output");
  }
  auto front=wc::spatialGains(.25f,.9f,3), rear=wc::spatialGains(.25f,.1f,12);
  require(std::fabs(front[0]-rear[2])<1e-6f && std::fabs(front[1]-rear[3])<1e-6f,
    "front and rear stereo mixdowns preserve left/right orientation");
  require(wc::stageDistance(.5f,.5f)==0.f && wc::stageDistance(.5f,0.f)==1.f &&
          wc::stageDistance(.5f,1.f)==1.f && wc::stageDistance(0.f,.5f)==1.f &&
          wc::stageDistance(1.f,.5f)==1.f,"every stage direction becomes far toward its edge");
  {
    std::unique_ptr<wc::Engine> scene(new wc::Engine); scene->setSampleRate(48000);
    wc::SetConfig c;c.enabled=true;c.x=.25f;c.y=.75f;scene->configure(0,c,0);
    scene->configureEffects(.8f,.6f,.5f,.4f);scene->strike(0);
    double energy[4]={};
    for(unsigned mask=1;mask<16;++mask) {
      scene->setOutputMask(mask);
      for(int i=0;i<2400;++i) {
        auto out=scene->processQuad(0,0,0,0);
        for(int ch=0;ch<4;++ch) {
          require(std::isfinite(out.channel[ch]),"routing changes with an active room remain finite");
          require((mask&(1u<<ch)) || out.channel[ch]==0.f,"engine skips unused outputs");
          energy[ch]+=out.channel[ch]*out.channel[ch];
        }
      }
    }
    for(float e:energy) require(e>1e-7f,"all four room/audio outputs are audible");
  }
  {
    wc::Selection selected;
    selected.toggle(2);selected.toggle(5);
    require(selected.mask==37u && selected.selected==5,"modifier click adds sets and updates the primary");
    selected.toggle(5);
    require(selected.mask==5u && selected.contains(selected.selected),"toggling off the primary retains a selected primary");
    selected.retain(4u);require(selected.mask==4u && selected.selected==2,"deleted sets leave the selection cleanly");
    selected.toggle(2);require(selected.mask==0,"last selected set can be deselected");
    selected.selectOnly(7);require(selected.mask==128u && selected.selected==7,"plain click replaces the selection");
    selected.clear();require(selected.mask==0,"empty stage click clears selection");
  }
  wc::SetConfig config; config.enabled = true;
  require(std::fabs(wc::tubeFrequency(config, 5, 0.f) / wc::tubeFrequency(config, 0, 0.f) - 2.f) < 0.001f, "scale repeats at the octave");
  require(std::fabs(wc::tubeFrequency(config, 0, 1.f) / wc::tubeFrequency(config, 0, 0.f) - 2.f) < 0.001f, "1V/oct transposition");
  config.divisions = 19;
  require(std::fabs(wc::tubeFrequency(config, 1, 0.f) / wc::tubeFrequency(config, 0, 0.f) - std::exp2(3.f / 19.f)) < 0.001f, "scale maps onto selected EDO");
  // Spread skips scale degrees, carrying across octaves rather than adding
  // semitone offsets. Existing five scale IDs retain their tuning.
  for (int scale = 0; scale < wc::scaleCount; ++scale) {
    config = wc::SetConfig(); config.scale = scale;
    for (int divisions : {12, 19, 24}) {
      config.divisions = divisions;
      for (int spread = 1; spread <= 4; ++spread) {
        config.spread = spread;
        float previous = 0.f;
        for (int t = 0; t < 4; ++t) {
          float frequency = wc::tubeFrequency(config, t, 0.f);
          require(std::isfinite(frequency) && frequency > previous,
            "new scales and degree spreads produce ordered finite pitches");
          previous = frequency;
        }
      }
    }
  }
  config = wc::SetConfig(); config.spread = 2;
  require(std::fabs(wc::tubeFrequency(config, 1, 0.f) /
      wc::tubeFrequency(config, 0, 0.f) - std::exp2(4.f / 12.f)) < 0.001f,
    "spread two skips the second major-pentatonic scale note");
  require(std::fabs(wc::tubeFrequency(config, 5, 0.f) /
      wc::tubeFrequency(config, 0, 0.f) - 4.f) < 0.001f,
    "scale-degree spread carries correctly through multiple octaves");
  config = wc::SetConfig(); config.scale = 5;
  require(std::fabs(wc::tubeFrequency(config, 1, 0.f) /
      wc::tubeFrequency(config, 0, 0.f) - std::exp2(1.f / 12.f)) < 0.001f,
    "chromatic scale includes every semitone");
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
    require(presets.count == 8, "every material offers eight intentional sound presets");
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
    for (int mode = 0; material != 0 && mode < 6; ++mode)
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
  // Muting preserves a short pitched object response across the Shape range.
  // Increasing Body must extend that response, rather than mix a separate click.
  for (float rate : {44100.f, 48000.f, 96000.f}) {
    for (float shape : {0.f, 0.33f, 0.67f, 1.f}) {
      double previousTailRatio = 0.f;
      for (float body : {0.f, 0.25f, 0.5f}) {
        config = wc::SetConfig(); config.material = 1; config.shape = shape;
        config.body = body; config.decay = 0.65f; config.hardness = 0.7f;
        wc::Resonator muted; muted.configure(261.625565f, rate, config);
        muted.strike(0.8f, wc::ContactKind::Striker, 0.4f);
        double early = 0.f, tail = 0.f;
        for (int i = 0; i < static_cast<int>(rate * 0.06f); ++i) {
          float sample = muted.process();
          if (i < rate * 0.02f) early += sample * sample;
          else tail += sample * sample;
        }
        double ratio = tail / early;
        require(early > 0.01 && ratio > 0.03,
          "low Body retains audible wood resonance after the initial impact");
        require(ratio > previousTailRatio,
          "Body continuously extends the same wood object's response");
        previousTailRatio = ratio;
      }
    }
  }
  for (float rate : {44100.f, 48000.f, 96000.f}) {
    config = wc::SetConfig(); config.material = 0; config.shape = 1.f; config.decay = 0.7f;
    wc::Resonator natural, shimmer;
    config.inharmonicity = 0.5f; natural.configure(261.625565f, rate, config);
    config.inharmonicity = 0.78f; shimmer.configure(261.625565f, rate, config);
    float naturalSplit = natural.modeFrequency(6) - natural.modeFrequency(0);
    float shimmerSplit = shimmer.modeFrequency(6) - shimmer.modeFrequency(0);
    require(naturalSplit > 0.1f && naturalSplit < 0.3f && shimmerSplit > 0.3f && shimmerSplit < 0.5f,
      "metal paired bending modes give subtle natural beating and stronger shimmer");
    // Measured bass-tube ratios from Lukkari/Valimaki Table 2 are
    // approximately 1/2.685/5.075/8.036/11.437, not ideal thin-beam spacing.
    const float observed[] = {1.f, 2.685f, 5.075f, 8.036f, 11.437f};
    for (int mode = 0; mode < 5; ++mode)
      require(std::fabs(natural.modeFrequency(mode)/261.625565f-observed[mode]) < .18f,
        "natural metal tube spacing follows measured compressed bending modes");
    config.decay = 1.f; config.body = 1.f;
    auto low = wc::MetalModel::mode(0, 220.f, config);
    auto upper = wc::MetalModel::mode(4, 220.f, config);
    require(low.seconds > 35.f && upper.seconds > .8f && upper.seconds < 2.5f,
      "metal upper modes retain metallic sustain beneath the long principal ring");
    config = wc::SetConfig(); config.material = 0; config.body = 0.f; config.shape = 0.5f;
    config.decay = 0.7f;
    wc::Resonator muted, full, soft, hard;
    muted.configure(261.625565f, rate, config);
    config.body = 1.f; full.configure(261.625565f, rate, config);
    config.body = 0.f; config.hardness = 0.f; soft.configure(261.625565f, rate, config);
    config.hardness = 1.f; hard.configure(261.625565f, rate, config);
    muted.strike(0.8f); full.strike(0.8f); soft.strike(0.8f); hard.strike(0.8f);
    double early = 0.f, mutedTail = 0.f, fullTail = 0.f;
    double softEnergy = 0.f, hardEnergy = 0.f, softSlope = 0.f, hardSlope = 0.f;
    float previousSoft = 0.f, previousHard = 0.f;
    for (int i = 0; i < rate * 0.2f; ++i) {
      float a = muted.process(), b = full.process(), c = soft.process(), d = hard.process();
      if (i < rate * 0.02f) early += a*a;
      else if (i < rate * 0.06f) mutedTail += a*a;
      if (i >= rate * 0.1f) fullTail += b*b;
      softEnergy += c*c; hardEnergy += d*d;
      softSlope += (c-previousSoft)*(c-previousSoft); hardSlope += (d-previousHard)*(d-previousHard);
      previousSoft = c; previousHard = d;
    }
    require(mutedTail > early * 0.03 && fullTail > mutedTail,
      "low Body retains brief metallic resonance while full Body sustains it");
    require(softEnergy < hardEnergy && softSlope/softEnergy < hardSlope/hardEnergy,
      "soft metal strikes reduce attack energy and bandwidth without a noise burst");
    config.body = 1.f; config.shape = 1.f; config.hardness = 0.8f;
    wc::Resonator node, center;
    node.configure(261.625565f,rate,config); center.configure(261.625565f,rate,config);
    node.strike(0.8f,wc::ContactKind::Striker,0.224f);
    center.strike(0.8f,wc::ContactKind::Striker,0.5f);
    double nr = 0.f, ni = 0.f, cr = 0.f, ci = 0.f;
    for (int i = 0; i < rate*0.15f; ++i) {
      float a=node.process(), b=center.process();
      double phase = 2.f*wc::pi*261.625565f*i/rate;
      nr+=a*std::cos(phase); ni+=a*std::sin(phase);
      cr+=b*std::cos(phase); ci+=b*std::sin(phase);
    }
    require(nr*nr+ni*ni < (cr*cr+ci*ci)*0.1,
      "a bending-node strike suppresses the lowest metal mode relative to a central strike");
  }
  // Every corner of the six sound controls, at low/high pitch and all rates:
  // preserve ordered bending spectra and finite energy under both contacts.
  for (int material : {0,2}) {
  for (float rate : {44100.f, 48000.f, 96000.f}) {
    for (float hz : {110.f, 880.f}) {
      for (int corner = 0; corner < 64; ++corner) {
        config = wc::SetConfig(); config.material = material;
        config.decay = (corner & 1) ? 1.f : 0.f;
        config.brightness = (corner & 2) ? 1.f : 0.f;
        config.hardness = (corner & 4) ? 1.f : 0.f;
        config.shape = (corner & 8) ? 1.f : 0.f;
        config.body = (corner & 16) ? 1.f : 0.f;
        config.inharmonicity = (corner & 32) ? 1.f : 0.f;
        auto second = wc::MetalModel::mode(1,hz,config);
        require(material != 0 || (second.frequency/hz > 2.4f && second.frequency/hz < 2.76f),
          "extreme metal controls retain a plausible bent-tube spectrum");
        for (auto kind : {wc::ContactKind::Striker,wc::ContactKind::Tube}) {
          wc::Resonator voice; voice.configure(hz,rate,config);
          voice.strike(.8f,kind,.4f);
          double energy = 0.f;
          for (int i=0;i<rate*.08f;++i) {
            float sample=voice.process();
            require(std::isfinite(sample) && std::fabs(sample)<2.f,
              "metal/plastic control corners and collision types remain bounded at every rate");
            energy+=sample*sample;
          }
          require(energy>1e-5,"metal/plastic extreme settings retain an audible body response");
        }
      }
    }
  }
  }
  // Perceptual regressions: metal needs persistent upper ring, while plastic
  // uses viscoelastic loss and geometry-specific short wall/cavity responses.
  config = wc::SetConfig(); config.material = 0; config.body = .9f;
  config.decay = .7f; config.shape = 1.f;
  auto metalUpper = wc::MetalModel::mode(2,220.f,config);
  config.material = 2;
  auto plasticUpper = wc::PlasticModel::mode(2,220.f,config);
  require(metalUpper.seconds > plasticUpper.seconds*2.f,
    "metal retains substantially more upper-mode sustain than plastic");
  for(float rate : {44100.f,48000.f,96000.f}) {
    config=wc::SetConfig(); config.material=2; config.body=.8f;
    config.shape=.7f; config.decay=.7f; config.brightness=.8f;
    wc::Resonator soft,hard,contact;
    config.hardness=0.f; soft.configure(220.f,rate,config);
    contact.configure(220.f,rate,config);
    config.hardness=1.f; hard.configure(220.f,rate,config);
    soft.strike(.8f,wc::ContactKind::Striker,.4f);
    hard.strike(.8f,wc::ContactKind::Striker,.4f);
    contact.strike(.8f,wc::ContactKind::Tube,.4f);
    double se=0,he=0,ss=0,hs=0,difference=0;float lastS=0,lastH=0;
    for(int i=0;i<rate*.1f;++i) {
      float a=soft.process(),b=hard.process(),c=contact.process();
      se+=a*a; he+=b*b; ss+=(a-lastS)*(a-lastS);hs+=(b-lastH)*(b-lastH);
      difference+=(a-c)*(a-c);lastS=a;lastH=b;
    }
    require(se<he && ss/se<hs/he,"plastic softness reduces attack energy and bandwidth");
    require(difference>.001,"plastic wall contact differs from a soft striker");
    config.body=0.f;
    wc::Resonator first,second;
    first.configure(220.f,rate,config);second.configure(220.f,rate,config);
    first.strike(.8f,wc::ContactKind::Striker,.4f);
    second.strike(.8f,wc::ContactKind::Striker,.4f);
    double early=0,tail=0;
    for(int i=0;i<rate*.06f;++i) {
      float a=first.process(),b=second.process();
      require(a==b,"plastic low Body remains deterministic with no mixed random noise");
      if(i<rate*.02f)early+=a*a;else tail+=a*a;
    }
    require(tail>early*.03,"plastic low Body preserves resonance beyond its attack");
    auto muted=wc::PlasticModel::mode(0,220.f,config);
    config.body=1.f;auto full=wc::PlasticModel::mode(0,220.f,config);
    auto high=wc::PlasticModel::mode(0,880.f,config);
    require(full.seconds>muted.seconds && high.seconds<full.seconds,
      "plastic loss increases with muting and absolute frequency");
  }
  // Sleeping SIMD groups must wake on a fresh contact, with no residual state
  // resurrected when a material changes its occupied modal slots.
  for (float rate : {44100.f,48000.f,96000.f}) {
    for (int material=0;material<3;++material) {
      config=wc::SetConfig();config.material=material;config.decay=0.f;
      wc::Resonator used,fresh;
      used.configure(220.f,rate,config);used.strike(.8f);
      for(int i=0;i<rate*3.f;++i) used.process();
      require(!used.active(),"quiet modal groups reach whole-voice sleep");
      config.material=(material+1)%3;config.decay=.8f;config.body=.95f;
      used.configure(220.f,rate,config);fresh.configure(220.f,rate,config);
      used.strike(.8f,wc::ContactKind::Tube,.4f);
      fresh.strike(.8f,wc::ContactKind::Tube,.4f);
      double energy=0.f;
      for(int i=0;i<rate*.1f;++i) {
        float a=used.process(),b=fresh.process();energy+=a*a;
        require(a==b,"sleeping and newly enabled modal lanes restart without stale state");
      }
      require(energy>.01,"fresh contacts wake sleeping modal banks");
    }
  }
  auto stopped = std::unique_ptr<wc::Engine>(new wc::Engine);
  auto unaffected = std::unique_ptr<wc::Engine>(new wc::Engine);
  stopped->setSampleRate(48000); unaffected->setSampleRate(48000);
  config = wc::SetConfig(); config.enabled = true; config.decay = 1.f;
  config.x = 1.f; config.y = 1.f;
  stopped->configure(1, config, 0.f); unaffected->configure(1, config, 0.f);
  stopped->strike(1); unaffected->strike(1);
  config.x = 0.f; stopped->configure(0, config, 0.f); stopped->strike(0);
  double beforeStop = 0.f;
  for (int i = 0; i < 24000; ++i) {
    auto out = stopped->process(0, 0, 0, 0); unaffected->process(0, 0, 0, 0);
    beforeStop += out.left * out.left;
  }
  require(beforeStop > 0.01, "stop test starts with an audible physical strike");
  stopped->stop(0);
  double rightError = 0.f;
  for (int i = 0; i < 4800; ++i) {
    auto out = stopped->process(0, 0, 0, 0), reference = unaffected->process(0, 0, 0, 0);
    rightError += std::pow(out.right - reference.right, 2);
    if (i >= 240) require(std::fabs(out.left) < 1e-6f,
      "stop fades the selected set to silence within five milliseconds");
  }
  require(rightError < 1e-8, "stopping one set leaves other sets' audio unchanged");
  for (int i = 0; i < 24000; ++i) stopped->process(1, 1, 1, 0);
  require(stopped->stopped(0) && stopped->strikerAngle(0) == 0.f && stopped->strikerDepth(0) == 0.f,
    "stopped chime remains at rest under continuing wind");
  stopped->strike(0);
  require(!stopped->stopped(0), "strike resumes a stopped set");
  double resumed = 0.f;
  for (int i = 0; i < 24000; ++i) { auto out = stopped->process(0, 0, 0, 0); resumed += out.left*out.left; }
  require(resumed > 0.01, "resumed striker produces physical contact audio");
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
  config.y = 0.5f; nearby->configure(0,config,0.f);
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
    config=wc::SetConfig(); config.enabled=true; config.y=0.5f - 0.5f * (1.f - nearness);
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
  // A single push creates a swinging phrase; alternating impacts can retain
  // similar spacing without a strike scheduler or additional wind impulses.
  wc::Motion phrase; config = wc::SetConfig(); config.tubes = 6; config.swing = 0.65f;
  phrase.configure(6); phrase.kick(0.f);
  float lastTime = -1.f, lastInterval = -1.f; int lastTube = -1, phraseHits = 0;
  bool steadyRun = false;
  for (int i = 0; i < 8000; ++i)
    phrase.step(0.0025f, 0.f, 0.f, config, 0, [&](int tube, float) {
      float now = i * 0.0025f, interval = now - lastTime;
      if (tube != lastTube && lastInterval > 0.7f && interval > 0.7f &&
          interval < 1.5f && std::fabs(interval - lastInterval) < 0.2f * lastInterval)
        steadyRun = true;
      lastInterval = tube != lastTube ? interval : -1.f;
      lastTime = now; lastTube = tube; ++phraseHits;
    }, [](int, int, float) {});
  require(phraseHits >= 4 && steadyRun,
    "one push sustains a phrase with consecutive nearly periodic alternating strikes");
  float referencePeriod = 0.f, referencePosition = 0.f;
  for (float dt : {0.00125f, 0.0025f, 0.005f}) {
    wc::Suspension assembly; assembly.kick({1.8f, 0.f, 0.f});
    float initialEnergy = assembly.energy(), previous = 0.f, first = 0.f, last = 0.f;
    int crossings = 0;
    for (int i = 0; i < static_cast<int>(10.f / dt); ++i) {
      assembly.step(dt, {}, 0.65f);
      float current = assembly.axis().x;
      if (previous < 0.f && current >= 0.f) {
        if (++crossings == 1) first = i * dt;
        last = i * dt;
      }
      previous = current;
      require(assembly.energy() <= initialEnergy + 1e-4f,
        "unforced coupled pendulum does not generate energy");
      auto cord = assembly.sailPosition() - assembly.position();
      require(std::fabs(cord.dot(cord) - wc::sailLength * wc::sailLength) < 1e-5f,
        "independent sail remains connected by a fixed-length cord");
    }
    require(crossings >= 4, "coupled pendulum maintains several free swing cycles");
    float period = (last - first) / (crossings - 1);
    if (referencePeriod > 0.f) {
      require(std::fabs(period - referencePeriod) < referencePeriod * 0.01f &&
          std::fabs(assembly.position().x - referencePosition) < 0.002f,
        "pendulum cadence and trajectory converge across physics step sizes");
    } else { referencePeriod = period; referencePosition = assembly.position().x; }
  }
  wc::Suspension impactAssembly; impactAssembly.kick({1.8f, 0.f, 0.4f});
  for (int i = 0; i < 80; ++i) impactAssembly.step(0.0025f, {}, 0.65f);
  auto velocity = impactAssembly.velocity();
  auto normal = velocity * (1.f / std::sqrt(velocity.dot(velocity)));
  float beforeImpact = impactAssembly.kineticEnergy();
  float impulse = 1.48f * velocity.dot(normal) / impactAssembly.response(normal).dot(normal);
  impactAssembly.applyImpulse(normal * -impulse);
  require(impactAssembly.kineticEnergy() < beforeImpact,
    "coupled effective-mass collision dissipates energy");
  auto sailVelocity = impactAssembly.sailSpeed();
  require(sailVelocity.dot(sailVelocity) > 0.01f,
    "the sail retains momentum after the striker impact");
  wc::Wind coherentWeather; wc::Motion smoothBreeze;
  config = wc::SetConfig(); config.tubes = 6; config.swing = 0.5f;
  int smoothHits = 0;
  for (int i = 0; i < 24000; ++i) {
    coherentWeather.step(0.0025f, 0.5f, 0.55f, 0.f);
    smoothBreeze.step(0.0025f, coherentWeather.x, coherentWeather.y, config, 0,
      [&](int, float) { ++smoothHits; }, [](int, int, float) {});
  }
  require(smoothHits >= 4,
    "coherent gusts drive repeated strikes with turbulence disabled");
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
