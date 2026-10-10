#include <osdialog.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>

#include "Computerscare.hpp"
#include "ComputerscareMeter.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
#include "Phlooper/Engine.hpp"
#include "Phlooper/Wav.hpp"
#include "Phlooper/Waveform.hpp"
#include "Phlooper/Zoom.hpp"

struct PhlooperRightOutputPort : ComputerscareSvgPort {
  PhlooperRightOutputPort() {
    setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance,
        "res/components/computerscare-pentagon-jack-1-outline.svg")));
  }
};

struct PhlooperGainQuantity : ParamQuantity {
  float getDisplayValue() override {
    return 20.f * std::log10(std::max(.0001f, getValue()));
  }
  void setDisplayValue(float db) override {
    setValue(db <= -80.f ? 0.f : std::pow(10.f, db / 20.f));
  }
  std::string getDisplayValueString() override {
    return getValue() == 0.f ? "−∞" : string::f("%+.1f", getDisplayValue());
  }
};

struct PhlooperLengthQuantity : ParamQuantity {
  std::string getDescription() override;
};

struct PhlooperOffsetQuantity : ParamQuantity {
  float getDisplayValue() override;
  void setDisplayValue(float value) override;
  std::string getDisplayValueString() override {
    return string::f("%+.3f", getDisplayValue());
  }
  std::string getUnit() override;
};

struct ComputerscarePhlooper : Module {
  enum Param {
    START,
    LENGTH,
    OFFSET,
    REC_MIX,
    MIX,
    COUNT,
    MODE,
    OVERDUB,
    RECORD,
    ERASE,
    RESTART,
    SPEED,
    STOP,
    HOLD,
    LOOP_MUTE,
    LOOP_SOLO = LOOP_MUTE + 16,
    LOOP_PAUSE = LOOP_SOLO + 16,
    INPUT_GAIN = LOOP_PAUSE + 16,
    OUTPUT_GAIN,
    LOOP_DIRECTION,
    VISUAL_MODE = LOOP_DIRECTION + 16,
    SHOW_TRANSPORT,
    ZOOM,
    OUTPUT_MODE,
    UNUSED_INPUT_VCA,
    MUTE,
    PARAMS
  };
  enum Input {
    LEFT,
    RIGHT,
    REC_GATE,
    ERASE_GATE,
    RESTART_TRIG,
    MUTE_GATE,
    STOP_GATE,
    START_CV,
    LENGTH_CV,
    OFFSET_CV,
    HOLD_GATE,
    SPEED_CV,
    REC_MIX_CV,
    OUT_MIX_CV,
    VCA_CV,
    INPUTS
  };
  enum Output { OUT_L, OUT_R, EOC, OUTPUTS };
  enum Light { RECORD_LIGHT, LIGHTS };
  phlooper::Engine loop;
  ComputerscareMeterSignal inputMeter, outputMeter;
  float inputGain = 1.f, outputGain = 1.f;
  std::mutex storage;
  std::atomic<int> frames{0};
  std::array<std::atomic<float>, 16> positions{}, starts{}, ends{};
  struct WaveformVisual {
    std::array<std::atomic<float>, phlooper::waveformBins> low{}, high{};
    std::array<std::atomic<uint64_t>, phlooper::waveformBins> range{};
  };
  std::array<WaveformVisual, 16> waveform{};
  unsigned waveformDivider = 0;
  int waveformCursor = 0;
  std::atomic<uint64_t> waveformRange{0};
  uint64_t waveformSamplingRange = 0;
  static uint64_t packRange(phlooper::TimeRange range) {
    static_assert(sizeof(range) == sizeof(uint64_t), "range packing size");
    uint64_t bits;
    std::memcpy(&bits, &range, sizeof(bits));
    return bits;
  }
  static phlooper::TimeRange unpackRange(uint64_t bits) {
    phlooper::TimeRange range;
    std::memcpy(&range, &bits, sizeof(bits));
    return range;
  }

  std::atomic<bool> recording{false}, speedHoldVisual{false};
  std::atomic<unsigned> recordVisual{0}, eraseVisual{0};
  struct WriteVisual {
    std::atomic<float> from{0}, extent{0}, base{0}, period{1};
    std::atomic<int> kind{0};
  };
  std::array<WriteVisual, 16> writeVisual{};
  std::atomic<bool> resetRequest{false};
  unsigned visualDivider = 0;
  float loadRegionSettleTime = 0.f;
  bool firstGate = false;
  dsp::SchmittTrigger restartButton;
  std::array<dsp::SchmittTrigger, 16> retrigger;
  std::array<dsp::PulseGenerator, 16> endPulse;
  std::string error;
  ComputerscarePhlooper() {
    config(PARAMS, INPUTS, OUTPUTS, LIGHTS);
    waveformRange = packRange({});
    configSwitch(OUTPUT_MODE, 0, 1, 0, "Output mode", {"Mix", "Polyphonic"});
    configSwitch(MUTE, 0.f, 1.f, 0.f, "Mute all loops", {"Playing", "Muted"});
    configSwitch(ZOOM, 0, 1, 0, "Zoom", {"Full recording", "Fit loops"});
    configParam<PhlooperGainQuantity>(INPUT_GAIN, 0.f, 4.f, 1.f, "Input gain",
                                      " dB");
    configParam<PhlooperGainQuantity>(OUTPUT_GAIN, 0.f, 4.f, 1.f, "Output gain",
                                      " dB");
    configParam(START, 0, 1, 0, "Start", "%", 0, 100);
    configParam<PhlooperLengthQuantity>(LENGTH, .001, 1, 1, "Length", "%", 0,
                                        100);
    configParam(SPEED, -2.f, 2.f, 0.f, "Overall playback speed", "×", 2.f);
    configParam<PhlooperOffsetQuantity>(OFFSET, -20, 20, 1, "Phase offset");
    configParam(REC_MIX, 0, 1, .5, "Record mix", "%", 0, 100);
    configParam(MIX, 0, 1, 1, "Output mix", "%", 0, 100);
    configParam(COUNT, 1, 16, 2, "Active loops")->snapEnabled = true;
    configSwitch(MODE, 0, 2, 0, "Phase mode",
                 {"Time (ms)", "Length (%)", "Speed (%)"});
    configSwitch(VISUAL_MODE, 0, 1, 0, "Visualization", {"Line", "Waveform"});
    configSwitch(SHOW_TRANSPORT, 0, 1, 1, "Per-channel controls",
                 {"Hidden", "Visible"});
    configSwitch(OVERDUB, 0, 1, 0, "Overdub", {"Blend", "Add"});
    configSwitch(RECORD, 0.f, 1.f, 0.f, "Record all", {"Off", "Recording"});
    configLight(RECORD_LIGHT, "Recording");
    configButton(ERASE, "Erase all while held");
    configButton(RESTART, "Restart all");
    configSwitch(HOLD, 0.f, 1.f, 0.f, "Hold phase", {"Drifting", "Held"});
    for (int i = 0; i < 16; ++i) {
      configSwitch(LOOP_DIRECTION + i, 0.f, 2.f, 0.f,
                   string::f("Loop %d direction", i + 1),
                   {"Forward", "Reverse", "Forward then reverse"});
      configSwitch(LOOP_MUTE + i, 0.f, 1.f, 0.f,
                   string::f("Loop %d mute", i + 1), {"Off", "Muted"});
      configSwitch(LOOP_SOLO + i, 0.f, 1.f, 0.f,
                   string::f("Loop %d solo", i + 1), {"Off", "Solo"});
      configSwitch(LOOP_PAUSE + i, 0.f, 1.f, 0.f,
                   string::f("Loop %d pause", i + 1), {"Playing", "Paused"});
    }
    configSwitch(STOP, 0.f, 1.f, 0.f, "Stop all loops", {"Playing", "Stopped"});
    const int shortcutParams[] = {STOP,  MUTE,    HOLD, RECORD,
                                  ERASE, RESTART, ZOOM, SHOW_TRANSPORT};
    const char* shortcutHints[] = {"[space]",    "[m]", "[h]", "[r]",
                                   "[e] (hold)", "[t]", "[z]", "[c]"};
    for (int i = 0; i < 8; ++i)
      getParamQuantity(shortcutParams[i])->description = shortcutHints[i];
    const char* names[] = {
        "Audio left / mono", "Audio right",      "Record gates",
        "Erase gates",       "Restart triggers", "Mute gates",
        "Stop gates",        "Start CV",         "Length CV",
        "Offset CV",         "Hold phase gate",  "Overall speed CV",
        "Record mix CV",     "Output mix CV",    "Input VCA CV"};
    for (int i = 0; i < INPUTS; ++i) configInput(i, names[i]);
    configOutput(EOC, "End of loop triggers (polyphonic)");
    configOutput(OUT_L, "Left");
    configOutput(OUT_R, "Right");
    configBypass(LEFT, OUT_L);
    configBypass(RIGHT, OUT_R);
  }
  unsigned gates(int port) {
    unsigned mask = 0;
    for (int i = 0; i < 16; ++i) {
      // A one-channel control addresses all loops; poly channels address
      // individually.
      float v = inputs[port].getChannels() == 1 ? inputs[port].getVoltage()
                                                : inputs[port].getVoltage(i);
      if (v >= 1.f) mask |= 1u << i;
    }
    return mask;
  }
  void process(const ProcessArgs& args) override {
    float gainSlew = std::min(1.f, args.sampleTime / .005f);
    inputGain += (params[INPUT_GAIN].getValue() - inputGain) * gainSlew;
    outputGain += (params[OUTPUT_GAIN].getValue() - outputGain) * gainSlew;
    std::array<phlooper::Frame, phlooper::voices> channelInputs{};
    phlooper::Frame input, inputMeterFrame;
    int inputCount = clamp(int(params[COUNT].getValue()), 1, 16);
    for (int i = 0; i < phlooper::voices; ++i) {
      auto voltage = [&](int port) {
        return inputs[port].getVoltage(inputs[port].getChannels() == 1 ? 0 : i);
      };
      float vca = inputs[VCA_CV].isConnected()
                      ? clamp(voltage(VCA_CV) / 10.f, 0.f, 1.f)
                      : 1.f;
      channelInputs[i].l = voltage(LEFT) * inputGain * vca;
      channelInputs[i].r = inputs[RIGHT].isConnected()
                               ? voltage(RIGHT) * inputGain * vca
                               : channelInputs[i].l;
      if (i < inputCount) {
        inputMeterFrame.l =
            std::max(inputMeterFrame.l, std::fabs(channelInputs[i].l));
        inputMeterFrame.r =
            std::max(inputMeterFrame.r, std::fabs(channelInputs[i].r));
        input.l += channelInputs[i].l / inputCount;
        input.r += channelInputs[i].r / inputCount;
      }
    }
    inputMeter.process(inputMeterFrame.l, inputMeterFrame.r, args.sampleTime);
    std::unique_lock<std::mutex> lock(storage, std::try_to_lock);
    if (!lock.owns_lock()) {
      bool poly = params[OUTPUT_MODE].getValue() > .5f;
      outputs[OUT_L].setChannels(poly ? inputCount : 1);
      outputs[OUT_R].setChannels(poly ? inputCount : 1);
      for (int i = 0; i < (poly ? inputCount : 1); ++i) {
        outputs[OUT_L].setVoltage(
            (poly ? channelInputs[i].l : input.l) * outputGain, i);
        outputs[OUT_R].setVoltage(
            (poly ? channelInputs[i].r : input.r) * outputGain, i);
      }
      outputMeter.process((poly ? inputMeterFrame.l : input.l) * outputGain,
                          (poly ? inputMeterFrame.r : input.r) * outputGain,
                          args.sampleTime);
      recordVisual = 0;
      eraseVisual = 0;
      return;
    }
    if (resetRequest.exchange(false)) {
      params[START].setValue(0.f);
      params[LENGTH].setValue(1.f);
      loop.resetHeads();
      loop.size = 0;
      loop.initial = false;
      params[RECORD].setValue(0.f);
      frames = 0;
    }
    unsigned rec =
        gates(REC_GATE) | (params[RECORD].getValue() > .5f ? 65535u : 0u);
    if (!loop.size && !loop.initial && rec) {
      loop.begin(inputs[RIGHT].isConnected());
      firstGate = true;
    }
    if (loop.initial && !rec && firstGate) {
      loop.finish();
      firstGate = false;
    }
    unsigned restart = 0;
    bool all = restartButton.process(params[RESTART].getValue());
    for (int i = 0; i < 16; ++i) {
      float v = inputs[RESTART_TRIG].getChannels() == 1
                    ? inputs[RESTART_TRIG].getVoltage()
                    : inputs[RESTART_TRIG].getVoltage(i);
      if (retrigger[i].process(v) || all) restart |= 1u << i;
    }
    phlooper::Settings s;
    s.count = int(params[COUNT].getValue());
    s.mode = int(params[MODE].getValue());
    s.overdub = int(params[OVERDUB].getValue());
    s.start = params[START].getValue();
    s.length = params[LENGTH].getValue();
    for (int i = 0; i < phlooper::voices; ++i) {
      auto cv = [&](int port) {
        return inputs[port].getChannels() == 1 ? inputs[port].getVoltage()
                                               : inputs[port].getVoltage(i);
      };
      if (inputs[START_CV].getChannels() == 1 ||
          i < inputs[START_CV].getChannels()) {
        s.startMask |= 1u << i;
        s.starts[i] = clamp(s.start + cv(START_CV) / 10.f, 0.f, 1.f);
      }
      if (inputs[LENGTH_CV].getChannels() == 1 ||
          i < inputs[LENGTH_CV].getChannels()) {
        s.lengthMask |= 1u << i;
        s.lengths[i] = clamp(s.length + cv(LENGTH_CV) / 10.f, .001f, 1.f);
      }
    }
    s.offset = params[OFFSET].getValue() * (s.mode == 0 ? 5.f : 1.f);
    for (int i = 0; i < std::min(16, inputs[OFFSET_CV].getChannels()); ++i) {
      s.offsetMask |= 1u << i;
      float range = s.mode == 0 ? 100.f : 20.f;
      s.offsets[i] =
          clamp(inputs[OFFSET_CV].getVoltage(i) / 10.f, -1.f, 1.f) * range;
    }
    s.recordMix = clamp(
        params[REC_MIX].getValue() + inputs[REC_MIX_CV].getVoltage() / 10.f,
        0.f, 1.f);
    s.mix =
        clamp(params[MIX].getValue() + inputs[OUT_MIX_CV].getVoltage() / 10.f,
              0.f, 1.f);
    s.speed = std::pow(
        2.f, clamp(params[SPEED].getValue() + inputs[SPEED_CV].getVoltage(),
                   -2.f, 2.f));
    s.hold =
        params[HOLD].getValue() > .5f || inputs[HOLD_GATE].getVoltage() >= 1.f;
    if (loadRegionSettleTime > 0.f && loop.speedHold.active && s.mode == 2)
      s.hold = true;
    unsigned manualMute = params[MUTE].getValue() > .5f ? 65535u : 0u;
    unsigned solo = 0, paused = 0;
    for (int i = 0; i < s.count; ++i) {
      s.directions[i] = int(params[LOOP_DIRECTION + i].getValue());
      if (params[LOOP_MUTE + i].getValue() > .5f) manualMute |= 1u << i;
      if (params[LOOP_SOLO + i].getValue() > .5f) solo |= 1u << i;
      if (params[LOOP_PAUSE + i].getValue() > .5f) paused |= 1u << i;
    }
    if (solo) manualMute |= ((1u << s.count) - 1u) & ~solo;
    unsigned eraseMask =
        gates(ERASE_GATE) | (params[ERASE].getValue() > .5f ? 65535u : 0u);
    unsigned stopMask = gates(STOP_GATE) | paused |
                        (params[STOP].getValue() > .5f ? 65535u : 0u);
    // Patch cables can take several samples to propagate restored CV values.
    // Adopt the loaded region immediately without resetting saved playheads.
    if (loadRegionSettleTime > 0.f && loop.size) {
      loop.activeStart.fill(-1);
      loadRegionSettleTime =
          std::max(0.f, loadRegionSettleTime - args.sampleTime);
    }
    auto out = loop.process(input, args.sampleRate, s, rec, eraseMask, restart,
                            gates(MUTE_GATE) | manualMute, stopMask,
                            inputs[RIGHT].isConnected(), &channelInputs);
    speedHoldVisual.store(loop.speedHold.active, std::memory_order_relaxed);
    recordVisual.store(loop.initial ? 65535u : rec & ~eraseMask & ~stopMask,
                       std::memory_order_relaxed);
    eraseVisual.store(loop.initial ? 0u : eraseMask & ~stopMask,
                      std::memory_order_relaxed);
    out.l *= outputGain;
    out.r *= outputGain;
    bool polyOutput = params[OUTPUT_MODE].getValue() > .5f;
    outputs[OUT_L].setChannels(polyOutput ? s.count : 1);
    outputs[OUT_R].setChannels(polyOutput ? s.count : 1);
    phlooper::Frame outputMeterFrame;
    for (int i = 0; i < (polyOutput ? s.count : 1); ++i) {
      outputs[OUT_L].setVoltage(
          polyOutput ? loop.outputs[i].l * outputGain : out.l, i);
      outputs[OUT_R].setVoltage(
          polyOutput ? loop.outputs[i].r * outputGain : out.r, i);
    }
    if (polyOutput) {
      for (int i = 0; i < s.count; ++i) {
        outputMeterFrame.l = std::max(
            outputMeterFrame.l, std::fabs(loop.outputs[i].l * outputGain));
        outputMeterFrame.r = std::max(
            outputMeterFrame.r, std::fabs(loop.outputs[i].r * outputGain));
      }
    } else
      outputMeterFrame = out;
    outputMeter.process(outputMeterFrame.l, outputMeterFrame.r,
                        args.sampleTime);
    outputs[EOC].setChannels(s.count);
    for (int i = 0; i < 16; ++i) {
      if (loop.completed & (1u << i)) endPulse[i].trigger(.001f);
      bool high = endPulse[i].process(args.sampleTime);
      if (i < s.count) outputs[EOC].setVoltage(high ? 10.f : 0.f, i);
    }
    if (int(params[VISUAL_MODE].getValue()) == 1 && ++waveformDivider >= 64) {
      waveformDivider = 0;
      uint64_t requestedRange = waveformRange.load(std::memory_order_acquire);
      if (requestedRange != waveformSamplingRange) {
        waveformSamplingRange = requestedRange;
        waveformCursor = 0;
      }
      auto range = unpackRange(requestedRange);
      int size = loop.initial ? loop.captured : loop.size;
      for (int work = 0; work < 8; ++work) {
        waveformCursor %= s.count * phlooper::waveformBins;
        int voice = waveformCursor / phlooper::waveformBins;
        int bin = waveformCursor++ % phlooper::waveformBins;
        auto value = phlooper::sampleWaveformBin(loop.audio[voice].get(), size,
                                                 loop.stereo[voice], bin,
                                                 range.start, range.end);
        waveform[voice].low[bin].store(value.low, std::memory_order_relaxed);
        waveform[voice].high[bin].store(value.high, std::memory_order_relaxed);
        waveform[voice].range[bin].store(requestedRange,
                                         std::memory_order_release);
      }
    }
    frames.store(loop.initial ? loop.captured : loop.size,
                 std::memory_order_relaxed);
    recording.store(loop.initial || rec, std::memory_order_relaxed);
    lights[RECORD_LIGHT].setBrightness(loop.initial || rec ? 1.f : 0.f);
    // Publish display snapshots at 750 Hz, independently of audio/EOC timing.
    if (++visualDivider < 64) return;
    visualDivider = 0;
    for (int i = 0; i < 16; ++i) {
      auto& visual = writeVisual[i];
      auto& span = loop.writeSpans[i];
      float total = float(std::max(1, loop.size));
      visual.from.store(loop.initial ? 0.f : (span.base + span.from) / total,
                        std::memory_order_relaxed);
      visual.extent.store(loop.initial ? 1.f : span.extent / total,
                          std::memory_order_relaxed);
      visual.base.store(loop.initial ? 0.f : span.base / total,
                        std::memory_order_relaxed);
      visual.period.store(loop.initial ? 1.f : span.period / total,
                          std::memory_order_relaxed);
      visual.kind.store(loop.initial ? 1 : span.kind,
                        std::memory_order_relaxed);
      if (loop.initial) {
        positions[i].store(1.f, std::memory_order_relaxed);
        starts[i].store(0.f, std::memory_order_relaxed);
        ends[i].store(1.f, std::memory_order_relaxed);
        continue;
      }
      float start = float(loop.sourceStart(i));
      positions[i].store(
          loop.size
              ? float((start +
                       loop.playbackPosition(
                           i, int(params[LOOP_DIRECTION + i].getValue()))) /
                      loop.size)
              : 0.f,
          std::memory_order_relaxed);
      starts[i].store(loop.size ? start / loop.size : 0.f,
                      std::memory_order_relaxed);
      ends[i].store(
          loop.size ? float((start + loop.periods[i]) / loop.size) : 1.f,
          std::memory_order_relaxed);
    }
  }
  void load(const std::string& path, int voice = -1) {
    try {
      auto wav = phlooper::loadWav(path);
      int available = int(wav.samples.size() / wav.channels);
      int n = int(std::min(double(phlooper::capacity),
                           double(available) * phlooper::rate / wav.rate));
      if (n < 1) throw std::runtime_error("Empty WAV");
      std::vector<float> converted(n * 2);
      for (int i = 0; i < n; ++i) {
        double p = double(i) * wav.rate / phlooper::rate;
        int a = std::min(available - 1, int(p)),
            b = std::min(available - 1, a + 1);
        float f = float(p - a);
        for (int ch = 0; ch < 2; ++ch) {
          int c = std::min(ch, wav.channels - 1);
          converted[2 * i + ch] =
              5.f * (wav.samples[a * wav.channels + c] * (1 - f) +
                     wav.samples[b * wav.channels + c] * f);
        }
      }
      std::lock_guard<std::mutex> lock(storage);
      if (voice >= 0) {
        if (voice >= phlooper::voices) throw std::runtime_error("Invalid loop");
        if (loop.initial)
          throw std::runtime_error(
              "Finish initial recording before importing a loop");
        if (!loop.size) {
          loop.size = n;
          for (int i = 0; i < phlooper::voices; ++i) {
            std::fill(loop.audio[i].get(), loop.audio[i].get() + n * 2, 0.f);
            loop.stereo[i] = false;
          }
          loop.resetHeads();
          loadRegionSettleTime = .002f;
        }
        int copied = std::min(n, loop.size);
        std::copy(converted.begin(), converted.begin() + copied * 2,
                  loop.audio[voice].get());
        std::fill(loop.audio[voice].get() + copied * 2,
                  loop.audio[voice].get() + loop.size * 2, 0.f);
        loop.stereo[voice] = wav.channels > 1;
        frames = loop.size;
        error.clear();
        return;
      }
      for (int i = 0; i < 16; ++i) {
        std::copy(converted.begin(), converted.end(), loop.audio[i].get());
        loop.stereo[i] = wav.channels > 1;
      }
      loop.size = n;
      loop.initial = false;
      loop.resetHeads();
      loadRegionSettleTime = .002f;
      params[RECORD].setValue(0.f);
      frames = n;
      error.clear();
    } catch (const std::exception& e) {
      error = e.what();
    }
  }
  json_t* dataToJson() override {
    std::lock_guard<std::mutex> lock(storage);
    auto* root = json_object();
    auto* heads = json_array();
    auto* starts = json_array();
    auto* returning = json_array();
    for (int i = 0; i < 16; ++i) {
      json_array_append_new(returning, json_boolean(loop.returning[i]));
      json_array_append_new(heads, json_real(loop.head[i]));
      json_array_append_new(starts, json_integer(loop.activeStart[i]));
    }
    json_object_set_new(root, "returning", returning);
    json_object_set_new(root, "heads", heads);
    json_object_set_new(root, "starts", starts);
    if (loop.speedHold.active) {
      auto& state = loop.speedHold;
      auto* held = json_object();
      json_object_set_new(held, "count", json_integer(state.count));
      json_object_set_new(held, "size", json_integer(state.size));
      json_object_set_new(held, "clock", json_real(state.clock));
      json_object_set_new(held, "duration", json_real(state.duration));
      json_object_set_new(held, "globalSpeed", json_real(state.globalSpeed));
      auto* channels = json_array();
      for (int i = 0; i < state.count; ++i) {
        auto* channel = json_array();
        for (double value : {state.starts[i], state.periods[i], state.speeds[i],
                             state.phases[i]})
          json_array_append_new(channel, json_real(value));
        json_array_append_new(channels, channel);
      }
      json_object_set_new(held, "channels", channels);
      json_object_set_new(root, "speedHold", held);
    }
    return root;
  }
  void dataFromJson(json_t* root) override {
    std::lock_guard<std::mutex> lock(storage);
    auto* heads = json_object_get(root, "heads");
    auto* starts = json_object_get(root, "starts");
    auto* returning = json_object_get(root, "returning");
    for (int i = 0; i < 16; ++i) {
      loop.returning[i] = json_is_true(json_array_get(returning, i));
      loop.previousDirection[i] = int(params[LOOP_DIRECTION + i].getValue());
      auto* h = json_array_get(heads, i);
      double value = json_is_number(h) ? json_number_value(h) : 0.;
      loop.head[i] = std::isfinite(value)
                         ? std::max(0., std::min(double(loop.limit), value))
                         : 0.;
      auto* start = json_array_get(starts, i);
      loop.activeStart[i] =
          json_is_integer(start)
              ? int(std::max(json_int_t(-1),
                             std::min(json_int_t(loop.limit - 1),
                                      json_integer_value(start))))
              : -1;
    }
    loop.speedHold = {};
    auto* held = json_object_get(root, "speedHold");
    if (json_is_object(held)) {
      phlooper::Engine::SpeedHoldState state;
      state.count = int(json_integer_value(json_object_get(held, "count")));
      state.size = int(json_integer_value(json_object_get(held, "size")));
      state.clock = json_number_value(json_object_get(held, "clock"));
      state.duration = json_number_value(json_object_get(held, "duration"));
      state.globalSpeed =
          json_number_value(json_object_get(held, "globalSpeed"));
      bool valid = state.count >= 1 && state.count <= 16 && state.size > 0 &&
                   state.size <= loop.limit && std::isfinite(state.clock) &&
                   state.clock >= 0 && state.clock < 1 &&
                   std::isfinite(state.duration) && state.duration > 0 &&
                   state.duration <= loop.limit * 80. &&
                   std::isfinite(state.globalSpeed) &&
                   state.globalSpeed >= .25 && state.globalSpeed <= 4;
      auto* channels = json_object_get(held, "channels");
      valid &= json_array_size(channels) == size_t(std::max(0, state.count));
      for (int i = 0; valid && i < state.count; ++i) {
        auto* channel = json_array_get(channels, i);
        state.starts[i] = json_number_value(json_array_get(channel, 0));
        state.periods[i] = json_number_value(json_array_get(channel, 1));
        state.speeds[i] = json_number_value(json_array_get(channel, 2));
        state.phases[i] = json_number_value(json_array_get(channel, 3));
        valid = json_array_size(channel) == 4 &&
                std::isfinite(state.starts[i]) && state.starts[i] >= 0 &&
                state.starts[i] < state.size &&
                std::isfinite(state.periods[i]) && state.periods[i] > 0 &&
                state.periods[i] <= loop.limit + 1e-6 &&
                std::isfinite(state.speeds[i]) && state.speeds[i] >= .0125 &&
                state.speeds[i] <= 32 && std::isfinite(state.phases[i]) &&
                state.phases[i] >= 0 && state.phases[i] < 1 &&
                std::fabs(state.periods[i] / state.speeds[i] - state.duration) <
                    1e-6;
      }
      if (valid) {
        state.active = true;
        loop.speedHold = state;
      }
    }
  }
  void exportWavs(const std::string& path, int voice) {
    try {
      int n;
      {
        std::lock_guard<std::mutex> lock(storage);
        n = loop.initial ? loop.captured : loop.size;
      }
      if (!n) throw std::runtime_error("No recorded audio to export");
      std::string base = path;
      if (base.size() >= 4 && base.substr(base.size() - 4) == ".wav")
        base.resize(base.size() - 4);
      std::vector<std::string> paths;
      int first = voice < 0 ? 0 : voice, last = voice < 0 ? 16 : voice + 1;
      bool overwrite = false;
      for (int i = first; i < last; ++i) {
        paths.push_back(voice < 0
                            ? base + "-loop-" + std::to_string(i + 1) + ".wav"
                            : base + ".wav");
        overwrite |= system::exists(paths.back());
      }
      if (overwrite && !osdialog_message(OSDIALOG_WARNING, OSDIALOG_YES_NO,
                                         "Replace existing export WAV files?"))
        return;
      std::vector<float> snapshot(n * 2);
      for (int i = first; i < last; ++i) {
        bool stereo;
        for (int start = 0; start < n * 2; start += 4096) {
          std::lock_guard<std::mutex> lock(storage);
          if ((loop.initial ? loop.captured : loop.size) < n)
            throw std::runtime_error(
                "Recording changed during export; try again");
          int end = std::min(n * 2, start + 4096);
          std::copy(loop.audio[i].get() + start, loop.audio[i].get() + end,
                    snapshot.begin() + start);
        }
        {
          std::lock_guard<std::mutex> lock(storage);
          stereo = loop.stereo[i];
        }
        phlooper::saveWav(paths[i - first], snapshot.data(), n, stereo);
      }
    } catch (const std::exception& e) {
      osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, e.what());
    }
  }
  void onSave(const SaveEvent&) override {
    int n;
    {
      std::lock_guard<std::mutex> lock(storage);
      n = loop.initial ? loop.captured : loop.size;
    }
    try {
      std::string dir = createPatchStorageDirectory();
      if (!n) {
        for (int i = 0; i < 16; ++i)
          std::remove(
              (dir + "/loop-" + std::to_string(i + 1) + ".wav").c_str());
        return;
      }
      std::vector<float> snapshot(n * 2);
      for (int i = 0; i < 16; ++i) {
        bool stereo = false;
        for (int start = 0; start < n * 2; start += 4096) {
          std::lock_guard<std::mutex> lock(storage);
          int end = std::min(n * 2, start + 4096);
          std::copy(loop.audio[i].get() + start, loop.audio[i].get() + end,
                    snapshot.begin() + start);
          stereo = loop.stereo[i];
        }
        phlooper::saveWav(dir + "/loop-" + std::to_string(i + 1) + ".wav",
                          snapshot.data(), n, stereo);
      }
    } catch (const std::exception& e) {
      WARN("Phlooper save: %s", e.what());
    }
  }
  void onAdd(const AddEvent&) override {
    std::lock_guard<std::mutex> lock(storage);
    for (int i = 0; i < 16; ++i) {
      try {
        auto wav = phlooper::loadWav(getPatchStorageDirectory() + "/loop-" +
                                     std::to_string(i + 1) + ".wav");
        int n = std::min(loop.limit, int(wav.samples.size() / wav.channels));
        for (int j = 0; j < n; ++j)
          for (int ch = 0; ch < 2; ++ch)
            loop.audio[i][2 * j + ch] =
                wav.samples[j * wav.channels + std::min(ch, wav.channels - 1)] *
                5.f;
        if (i == 0) loop.size = n;
        if (n != loop.size) {
          loop.size = 0;
          break;
        }
        loop.stereo[i] = wav.channels > 1;
      } catch (const std::exception&) {
        if (loop.size) loop.size = 0;
        break;
      }
    }
    for (int i = 0; i < 16; ++i)
      if (loop.activeStart[i] >= loop.size) loop.activeStart[i] = -1;
    loadRegionSettleTime = .002f;
    frames = loop.size;
  }
  void onReset(const ResetEvent& e) override {
    Module::onReset(e);
    resetRequest = true;
  }
};

std::string PhlooperLengthQuantity::getDescription() {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  return string::f("%.3f seconds",
                   (m ? m->frames.load() : 0) / 48000.f * getValue());
}

float PhlooperOffsetQuantity::getDisplayValue() {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  bool time = !m || m->params[ComputerscarePhlooper::MODE].getValue() == 0.f;
  return getValue() * (time ? 5.f : 1.f);
}
void PhlooperOffsetQuantity::setDisplayValue(float value) {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  bool time = !m || m->params[ComputerscarePhlooper::MODE].getValue() == 0.f;
  setValue(value / (time ? 5.f : 1.f));
}

std::string PhlooperOffsetQuantity::getUnit() {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  int mode = m ? int(m->params[ComputerscarePhlooper::MODE].getValue()) : 0;
  return mode == 0 ? " ms per loop" : "% per loop";
}

struct PhlooperButton : app::Switch {
  struct Face : phlooper::WarpedDropdown {
    std::string caption;
    bool pressed = false;
    std::string label() override { return caption; }
    bool isPressed() override { return pressed; }
  } face;
  PhlooperButton() {
    momentary = true;
    box.size = Vec(42, 20);
    face.configure(42, 20, 4.f, 3.f, 1.2f, 1);
    face.faceColor = nvgRGB(36, 201, 166);
  }
  void draw(const DrawArgs& args) override {
    auto* quantity = getParamQuantity();
    face.pressed = quantity && quantity->getValue() > .5f;
    auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
    if (m && paramId == ComputerscarePhlooper::HOLD)
      face.pressed |=
          m->inputs[ComputerscarePhlooper::HOLD_GATE].getVoltage() >= 1.f;
    face.draw(args);
  }
};

struct PhlooperRecordIndicator : widget::TransparentWidget {
  ComputerscarePhlooper* module = nullptr;
  PhlooperButton::Face face;
  PhlooperRecordIndicator() {
    box.size = Vec(15, 20);
    face.configure(15, 20, 2.f, 2.f, .8f, 31);
  }
  float pulse() const {
    return .5f +
           .5f * std::sin(float(std::fmod(glfwGetTime(), 1.25)) * 5.026548f);
  }
  void draw(const DrawArgs& args) override {
    bool on = module && module->recording.load(std::memory_order_relaxed);
    face.pressed = false;
    float brightness = pulse();
    face.faceColor =
        on ? nvgRGB(238 + int(17 * brightness), 62 + int(14 * brightness),
                    67 + int(13 * brightness))
           : nvgRGBA(235, 205, 212, 210);
    face.draw(args);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && !args.fb && module &&
        module->recording.load(std::memory_order_relaxed) &&
        settings::haloBrightness > 0.f) {
      nvgSave(args.vg);
      nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
      nvgBeginPath(args.vg);
      nvgRect(args.vg, -12.f, -12.f, box.size.x + 24.f, box.size.y + 24.f);
      auto glow = nvgBoxGradient(
          args.vg, 1.f, 1.f, box.size.x - 2.f, box.size.y - 2.f, 3.f, 18.f,
          nvgRGBAf(1.f, .12f, .15f,
                   (.49f + .06f * pulse()) * settings::haloBrightness),
          nvgRGBA(255, 30, 38, 0));
      nvgFillPaint(args.vg, glow);
      nvgFill(args.vg);
      nvgRestore(args.vg);
    }
    widget::TransparentWidget::drawLayer(args, layer);
  }
};

static std::string phlooperChoiceDescription(int param, int value) {
  if (param == ComputerscarePhlooper::MODE) {
    static const char* descriptions[] = {
        "Offset changes loop duration in milliseconds.\n"
        "Pitch stays the same.",
        "Offset changes loop duration by a percentage of Length.\n"
        "Pitch stays the same.",
        "Offset changes playback speed by a percentage.\n"
        "Pitch changes with speed."};
    return descriptions[clamp(value, 0, 2)];
  }
  if (param == ComputerscarePhlooper::OVERDUB)
    return value == 0
               ? "Record mix crossfades existing audio toward incoming audio.\n"
                 "0% keeps the loop; 100% replaces it."
               : "Keep existing audio and add incoming audio at Record mix "
                 "level.\n"
                 "0% adds nothing; 100% adds the full input.";
  return "";
}

struct PhlooperDescribedChoiceItem : MenuItem {
  std::string description;
  std::function<bool()> checked;
  std::function<void()> action;
  void onAction(const event::Action& e) override { action(); }
  void step() override {
    rightText = CHECKMARK(checked());
    float width = bndLabelWidth(APP->window->vg, -1, text.c_str());
    std::istringstream lines(description);
    std::string line;
    int count = 0;
    while (std::getline(lines, line)) {
      width = std::max(width, bndLabelWidth(APP->window->vg, -1, line.c_str()));
      ++count;
    }
    box.size.x = width + 34.f;
    box.size.y = count ? 24.f + count * 18.f : 20.f;
    Widget::step();
  }
  void draw(const DrawArgs& args) override {
    if (description.empty()) {
      MenuItem::draw(args);
      return;
    }
    BNDwidgetState state =
        APP->event->hoveredWidget == this ? BND_HOVER : BND_DEFAULT;
    const BNDtheme* theme = bndGetTheme();
    if (state != BND_DEFAULT) {
      bndInnerBox(args.vg, 0.f, 0.f, box.size.x, box.size.y, 0, 0, 0, 0,
                  bndOffsetColor(theme->menuItemTheme.innerSelectedColor,
                                 theme->menuItemTheme.shadeTop),
                  bndOffsetColor(theme->menuItemTheme.innerSelectedColor,
                                 theme->menuItemTheme.shadeDown));
      state = BND_ACTIVE;
    }
    NVGcolor color = bndTextColor(&theme->menuItemTheme, state);
    NVGcolor detailColor = state == BND_DEFAULT
                               ? theme->menuTheme.textColor
                               : theme->menuTheme.textSelectedColor;
    bndIconLabelValue(args.vg, 0.f, 3.f, box.size.x, 18.f, -1, color, BND_LEFT,
                      BND_LABEL_FONT_SIZE, text.c_str(), nullptr);
    std::istringstream lines(description);
    std::string line;
    float y = 21.f;
    while (std::getline(lines, line)) {
      bndIconLabelValue(args.vg, 0.f, y, box.size.x, 18.f, -1, detailColor,
                        BND_LEFT, BND_LABEL_FONT_SIZE, line.c_str(), nullptr);
      y += 18.f;
    }
    float x = box.size.x - bndLabelWidth(args.vg, -1, rightText.c_str());
    bndIconLabelValue(args.vg, x, 0.f, box.size.x, 18.f, -1, color, BND_LEFT,
                      BND_LABEL_FONT_SIZE, rightText.c_str(), nullptr);
  }
};

struct PhlooperChoice : phlooper::WarpedDropdown {
  ComputerscarePhlooper* module = nullptr;
  int param = 0;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  ui::Tooltip* hoverTooltip = nullptr;
  ~PhlooperChoice() { destroyHoverTooltip(); }
  void destroyHoverTooltip() {
    if (!hoverTooltip) return;
    APP->scene->removeChild(hoverTooltip);
    delete hoverTooltip;
    hoverTooltip = nullptr;
  }
  void onEnter(const event::Enter& e) override {
    if (module && settings::tooltips && !isPressed() && !hoverTooltip) {
      hoverTooltip = new ui::Tooltip;
      APP->scene->addChild(hoverTooltip);
    }
    phlooper::WarpedDropdown::onEnter(e);
  }
  void onLeave(const event::Leave& e) override {
    destroyHoverTooltip();
    phlooper::WarpedDropdown::onLeave(e);
  }
  void step() override {
    phlooper::WarpedDropdown::step();
    if (!module || !settings::tooltips || isPressed()) destroyHoverTooltip();
    if (hoverTooltip) {
      auto* quantity = module->getParamQuantity(param);
      hoverTooltip->text = (param == ComputerscarePhlooper::LOOP_DIRECTION
                                ? "All loop directions"
                                : quantity->getLabel()) +
                           std::string(": ") + label();
    }
  }
  std::string label() override {
    int value = module ? int(module->params[param].getValue()) : 0;
    if (param == ComputerscarePhlooper::COUNT)
      return std::to_string(module ? value : 2) + " loops";
    if (param == ComputerscarePhlooper::MODE)
      return std::string(value == 0   ? "Time ms"
                         : value == 1 ? "Length %"
                                      : "Speed %");
    if (param == ComputerscarePhlooper::LOOP_DIRECTION) {
      if (module)
        for (int i = 1; i < 16; ++i)
          if (int(module->params[param + i].getValue()) != value)
            return "Mixed";
      return value == 0 ? "Forward" : value == 1 ? "Reverse" : "Fwd / Rev";
    }
    return std::string(value ? "Add" : "Blend");
  }
  bool isPressed() override {
    auto* overlay = activeMenuOverlay.get();
    return overlay && !overlay->requestedDelete;
  }
  void onButton(const event::Button& e) override {
    if (module && e.action == GLFW_PRESS &&
        e.button == GLFW_MOUSE_BUTTON_LEFT) {
      destroyHoverTooltip();
      auto* menu = createMenu();
      activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
      int count = param == ComputerscarePhlooper::COUNT ? 16 : 3;
      for (int i = 0; i < count; ++i) {
        int value = param == ComputerscarePhlooper::COUNT ? i + 1 : i;
        std::string name = param == ComputerscarePhlooper::COUNT
                               ? std::to_string(value) + " loops"
                           : param == ComputerscarePhlooper::MODE
                               ? (i == 0   ? "Time (ms)"
                                  : i == 1 ? "Length (%)"
                                           : "Speed (%)")
                               : (i == 0   ? "Forward"
                                  : i == 1 ? "Reverse"
                                           : "Forward then reverse");
        auto* item = new PhlooperDescribedChoiceItem;
        item->text = name;
        item->description = phlooperChoiceDescription(param, value);
        item->checked = [=]() {
          int channels =
              param == ComputerscarePhlooper::LOOP_DIRECTION ? 16 : 1;
          for (int j = 0; j < channels; ++j)
            if (module->params[param + j].getValue() != value) return false;
          return true;
        };
        item->action = [=]() {
          auto* changes = new history::ComplexAction;
          changes->name = "Phlooper mode";
          int channels =
              param == ComputerscarePhlooper::LOOP_DIRECTION ? 16 : 1;
          for (int j = 0; j < channels; ++j) {
            auto* change = new history::ParamChange;
            change->moduleId = module->id;
            change->paramId = param + j;
            change->oldValue = module->params[param + j].getValue();
            change->newValue = float(value);
            module->params[param + j].setValue(float(value));
            changes->push(change);
          }
          APP->history->push(changes);
        };
        menu->addChild(item);
      }
      e.consume(this);
    }
  }
};
struct PhlooperTransportButton : ParamWidget {
  const char* letter = "M";
  bool directionButton = false;
  bool externallyActive = false;
  void draw(const DrawArgs& args) override {
    auto* quantity = getParamQuantity();
    bool active = externallyActive || (quantity && quantity->getValue() > .5f);
    nvgBeginPath(args.vg);
    nvgRect(args.vg, .3f, .3f, box.size.x - .6f, box.size.y - .6f);
    nvgFillColor(args.vg, active ? nvgRGB(192, 218, 172) : nvgRGB(35, 59, 54));
    nvgFill(args.vg);
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, std::min(8.f, box.size.y - .5f));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, active ? nvgRGB(13, 29, 28) : nvgRGB(210, 219, 205));
    const char* caption = letter;
    if (directionButton) {
      int mode = quantity ? int(quantity->getValue()) : 0;
      caption = mode == 0 ? "F" : mode == 1 ? "R" : "B";
    }
    nvgText(args.vg, box.size.x * .5f, box.size.y * .5f, caption, nullptr);
  }
  void onButton(const event::Button& e) override {
    ParamWidget::onButton(e);
    auto* quantity = getParamQuantity();
    if (quantity && e.action == GLFW_PRESS &&
        e.button == GLFW_MOUSE_BUTTON_LEFT) {
      auto* change = new history::ParamChange;
      change->name = quantity->getLabel();
      change->moduleId = module->id;
      change->paramId = paramId;
      change->oldValue = quantity->getValue();
      change->newValue = directionButton
                             ? float((int(change->oldValue) + 1) % 3)
                             : (change->oldValue > .5f ? 0.f : 1.f);
      quantity->setValue(change->newValue);
      APP->history->push(change);
      e.consume(this);
    }
  }
};
struct PhlooperView : widget::Widget {
  ComputerscarePhlooper* module = nullptr;
  PhlooperTransportButton* transport[16][4]{};
  phlooper::TimeRange zoomRange;
  uint64_t waveformRange = 0;
  bool showTransport() const {
    return !module ||
           module->params[ComputerscarePhlooper::SHOW_TRANSPORT].getValue() >
               .5f;
  }
  float visualLeft() const { return showTransport() ? 52.f : 10.f; }
  float visualX(float position) const {
    return visualLeft() + (box.size.x - visualLeft() - 10.f) *
                              (position - zoomRange.start) /
                              (zoomRange.end - zoomRange.start);
  }

  static float wrapWav(float position) {
    return position - std::floor(position);
  }
  static float endPoint(float position) {
    float wrapped = wrapWav(position);
    // Exact source-boundary endpoints belong at the right edge, not under
    // Start.
    return position > 0.f && (wrapped < .00001f || wrapped > .99999f) ? 1.f
                                                                      : wrapped;
  }
  Vec requestedRegion(int voice) const {
    if (!module) return Vec(.2f, 1.f);
    if (module->speedHoldVisual.load(std::memory_order_relaxed) &&
        int(module->params[ComputerscarePhlooper::MODE].getValue()) == 2)
      return Vec(module->starts[voice].load(), module->ends[voice].load());
    auto cv = [&](int port, int channel) {
      auto& input = module->inputs[port];
      return input.getChannels() == 1        ? input.getVoltage()
             : channel < input.getChannels() ? input.getVoltage(channel)
                                             : 0.f;
    };
    bool hold =
        module->params[ComputerscarePhlooper::HOLD].getValue() > .5f ||
        module->inputs[ComputerscarePhlooper::HOLD_GATE].getVoltage() >= 1.f;
    float start =
        clamp(module->params[ComputerscarePhlooper::START].getValue() +
                  cv(ComputerscarePhlooper::START_CV, voice) / 10.f,
              0.f, 1.f);
    float length =
        clamp(module->params[ComputerscarePhlooper::LENGTH].getValue() +
                  cv(ComputerscarePhlooper::LENGTH_CV, hold ? 0 : voice) / 10.f,
              .001f, 1.f);
    int frames = module->frames.load(std::memory_order_relaxed);
    if (frames <= 0) return Vec(start, start + length);
    int mode = int(module->params[ComputerscarePhlooper::MODE].getValue());
    int offsetVoice = hold ? 0 : voice;
    auto& offsetCV = module->inputs[ComputerscarePhlooper::OFFSET_CV];
    float offset =
        offsetVoice < offsetCV.getChannels()
            ? clamp(offsetCV.getVoltage(offsetVoice) / 10.f, -1.f, 1.f) *
                  (mode == 0 ? 100.f : 20.f)
        : hold
            ? 0.f
            : voice * module->params[ComputerscarePhlooper::OFFSET].getValue() *
                  (mode == 0 ? 5.f : 1.f);
    double period = std::max(1, int(length * frames));
    if (mode == 0)
      period += offset * phlooper::rate / 1000.;
    else if (mode == 1)
      period *= 1. + offset / 100.;
    period = std::max(48., std::min(double(phlooper::capacity), period));
    start = float(std::min(frames - 1, int(start * frames))) / frames;
    return Vec(start, start + float(period / frames));
  }
  void drawLoopMarker(const DrawArgs& args, float x, float y, float height,
                      bool start) {
    float tip = x + (start ? 1.f : -1.f) * std::min(3.f, height);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, tip, y - height);
    nvgLineTo(args.vg, x, y - height);
    nvgLineTo(args.vg, x, y + height);
    nvgLineTo(args.vg, tip, y + height);
    nvgStrokeColor(args.vg, nvgRGB(13, 29, 28));
    nvgStrokeWidth(args.vg, 3.f);
    nvgStroke(args.vg);
    nvgStrokeColor(args.vg, nvgRGB(240, 243, 240));
    nvgStrokeWidth(args.vg, 1.2f);
    nvgStroke(args.vg);
  }
  void drawSpan(const DrawArgs& args, float from, float to, float y,
                float width, NVGcolor color) {
    auto xAt = [&](float value) { return visualX(value); };
    auto line = [&](float left, float right) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, xAt(left), y);
      nvgLineTo(args.vg, xAt(right), y);
      nvgStrokeColor(args.vg, color);
      nvgStrokeWidth(args.vg, width);
      nvgStroke(args.vg);
    };
    float length = std::max(0.f, to - from);
    if (length >= 1.f) {
      line(zoomRange.start, zoomRange.end);
      return;
    }
    float start = wrapWav(from);
    int first = int(std::floor(zoomRange.start - start));
    int last = int(std::ceil(zoomRange.end - start));
    for (int copy = first; copy <= last; ++copy) {
      float left = std::max(zoomRange.start, start + copy);
      float right = std::min(zoomRange.end, start + copy + length);
      if (right > left) line(left, right);
    }
  }

  static void drawTracker(const DrawArgs& args, int i, float x, float y,
                          float dotScaleX, float dotScaleY) {
    // Stable, independent corner offsets give every loop its own silhouette.
    auto warp = [i](unsigned corner) {
      unsigned bits = unsigned(i + 1) * 0x9e3779b9u + corner * 0x85ebca6bu;
      bits ^= bits >> 16;
      bits *= 0x7feb352du;
      bits ^= bits >> 15;
      return float(bits & 1023u) / 1023.f;
    };
    float blobX[8], blobY[8];
    for (int corner = 0; corner < 8; ++corner) {
      float angle = corner * 0.78539816f + (warp(corner + 8) - .5f) * .3f;
      float radius = .55f + .45f * warp(corner);
      blobX[corner] = x + std::cos(angle) * radius * 8.2f * dotScaleX;
      blobY[corner] = y + std::sin(angle) * radius * 7.f * dotScaleY;
    }
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, blobX[0], blobY[0]);
    for (int corner = 0; corner < 8; ++corner) {
      int prev = (corner + 7) % 8, next = (corner + 1) % 8;
      int after = (corner + 2) % 8;
      nvgBezierTo(args.vg, blobX[corner] + (blobX[next] - blobX[prev]) * .18f,
                  blobY[corner] + (blobY[next] - blobY[prev]) * .18f,
                  blobX[next] - (blobX[after] - blobX[corner]) * .18f,
                  blobY[next] - (blobY[after] - blobY[corner]) * .18f,
                  blobX[next], blobY[next]);
    }
    nvgClosePath(args.vg);
    nvgFillColor(args.vg, nvgRGB(220, 222, 220));
    nvgFill(args.vg);
    nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
    nvgStrokeWidth(args.vg, .65f);
    nvgStroke(args.vg);
  }
  void step() override {
    auto* draggedKnob =
        dynamic_cast<app::Knob*>(APP->event->getDraggedWidget());
    bool movingKnob = module && draggedKnob && draggedKnob->module == module;
    bool zoomed =
        module && module->params[ComputerscarePhlooper::ZOOM].getValue() > .5f;
    if (!zoomed)
      zoomRange = {};
    else if (!movingKnob) {
      std::array<phlooper::TimeRange, 16> regions{};
      int count = clamp(
          int(module->params[ComputerscarePhlooper::COUNT].getValue()), 1, 16);
      for (int i = 0; i < count; ++i) {
        Vec region = requestedRegion(i);
        regions[i] = {region.x, region.y};
      }
      zoomRange = phlooper::fitLoopZoom(regions, count);
    }
    waveformRange = ComputerscarePhlooper::packRange(zoomRange);
    if (module)
      module->waveformRange.store(waveformRange, std::memory_order_release);
    int count =
        module
            ? clamp(
                  int(module->params[ComputerscarePhlooper::COUNT].getValue()),
                  1, 16)
            : 2;
    unsigned externalMute = 0, solo = 0;
    if (module) {
      externalMute = module->gates(ComputerscarePhlooper::MUTE_GATE);
      if (module->params[ComputerscarePhlooper::MUTE].getValue() > .5f)
        externalMute = 65535u;
      for (int i = 0; i < count; ++i)
        if (module->params[ComputerscarePhlooper::LOOP_SOLO + i].getValue() >
            .5f)
          solo |= 1u << i;
      if (solo) externalMute |= ((1u << count) - 1u) & ~solo;
    }
    float row = (box.size.y - 12.f) / count;
    float height = std::min(12.f, row - .7f);
    for (int i = 0; i < 16; ++i)
      for (int j = 0; j < 4; ++j) {
        if (!transport[i][j]) continue;
        transport[i][j]->externallyActive =
            j == 0 && (externalMute & (1u << i));
        transport[i][j]->visible = showTransport() && i < count;
        transport[i][j]->box =
            Rect(Vec(3 + j * 10, 6 + row * (i + .5f) - height * .5f),
                 Vec(9, height));
      }
    widget::Widget::step();
  }
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(13, 29, 28));
    nvgFill(args.vg);
    int count =
        module
            ? clamp(
                  int(module->params[ComputerscarePhlooper::COUNT].getValue()),
                  1, 16)
            : 2;
    unsigned soloMask = 0;
    for (int i = 0; module && i < count; ++i)
      if (module->params[ComputerscarePhlooper::LOOP_SOLO + i].getValue() > .5f)
        soloMask |= 1u << i;
    unsigned muteMask =
        module ? module->gates(ComputerscarePhlooper::MUTE_GATE) : 0;
    if (module && module->params[ComputerscarePhlooper::MUTE].getValue() > .5f)
      muteMask = 65535u;
    unsigned pauseMask =
        module ? module->gates(ComputerscarePhlooper::STOP_GATE) : 0;
    float rowHeight = (box.size.y - 12.f) / count;
    float dotScaleY = std::min(1.f, rowHeight * .36f / 7.f);
    float dotScaleX = std::max(.65f, dotScaleY);
    float tickHeight = rowHeight * .46f;
    float lineWidth = std::min(4.f, rowHeight * .45f);
    // Keep a fixed source-WAV scale; wrapped spans continue from the left.
    auto xAt = [&](float position) {
      return visualX(phlooper::zoomPoint(position, zoomRange));
    };
    for (int i = 0; i < count; ++i) {
      Vec requested = requestedRegion(i);
      float y = 6.f + rowHeight * (i + .5f);
      if (module) {
        unsigned bit = 1u << i;
        bool soloed = soloMask & bit;
        bool muted =
            (muteMask & bit) ||
            module->params[ComputerscarePhlooper::LOOP_MUTE + i].getValue() >
                .5f ||
            (soloMask && !soloed);
        bool paused =
            (pauseMask & bit) ||
            module->params[ComputerscarePhlooper::LOOP_PAUSE + i].getValue() >
                .5f ||
            module->params[ComputerscarePhlooper::STOP].getValue() > .5f;
        if (muted || soloed || paused) {
          nvgBeginPath(args.vg);
          nvgRect(args.vg, 1.f, y - rowHeight * .5f + .35f, box.size.x - 2.f,
                  rowHeight - .7f);
          nvgFillColor(args.vg, muted    ? nvgRGBA(154, 46, 52, 45)
                                : soloed ? nvgRGBA(47, 104, 180, 55)
                                         : nvgRGBA(157, 164, 157, 26));
          nvgFill(args.vg);
        }
      }
      nvgSave(args.vg);
      nvgIntersectScissor(args.vg, visualLeft(), 0.f,
                          box.size.x - visualLeft() - 10.f, box.size.y);
      float start = module ? module->starts[i].load() : .2f;
      float end = module ? module->ends[i].load() : 1.f;
      float shade = float(i) / 15.f;
      bool lineMode =
          !module ||
          int(module->params[ComputerscarePhlooper::VISUAL_MODE].getValue()) ==
              0;
      NVGcolor color =
          lineMode ? nvgRGB(67 + int(123 * shade), 126 + int(104 * shade),
                            119 + int(38 * shade))
                   : nvgRGB(240, 243, 240);
      bool waveformMode =
          module &&
          int(module->params[ComputerscarePhlooper::VISUAL_MODE].getValue()) ==
              1;
      if (!waveformMode) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, visualLeft(), y);
        nvgLineTo(args.vg, box.size.x - 10.f, y);
        nvgStrokeColor(args.vg, nvgRGBA(193, 219, 187, 85));
        nvgStrokeWidth(args.vg, .75f);
        nvgStroke(args.vg);
      }
      if (module &&
          int(module->params[ComputerscarePhlooper::VISUAL_MODE].getValue()) ==
              1) {
        nvgBeginPath(args.vg);
        float amplitude = std::max(.5f, rowHeight * .5f - .45f);
        for (int bin = 0; bin < phlooper::waveformBins; ++bin) {
          if (module->waveform[i].range[bin].load(std::memory_order_acquire) !=
              waveformRange)
            continue;
          float x = visualLeft() + (box.size.x - visualLeft() - 10.f) *
                                       ((bin + .5f) / phlooper::waveformBins);
          float low =
              module->waveform[i].low[bin].load(std::memory_order_relaxed);
          float high =
              module->waveform[i].high[bin].load(std::memory_order_relaxed);
          nvgMoveTo(args.vg, x,
                    y - phlooper::waveformAmplitude(high) * amplitude);
          nvgLineTo(args.vg, x,
                    y - phlooper::waveformAmplitude(low) * amplitude);
        }
        nvgStrokeColor(args.vg, nvgRGBA(111, 169, 151, 190));
        nvgStrokeWidth(args.vg, .8f);
        nvgStroke(args.vg);
      }
      if (!waveformMode) drawSpan(args, start, end, y, lineWidth, color);
      if (module) {
        auto& visual = module->writeVisual[i];
        int kind = visual.kind.load(std::memory_order_relaxed);
        float base = visual.base.load(std::memory_order_relaxed);
        float period = visual.period.load(std::memory_order_relaxed);
        float from = visual.from.load(std::memory_order_relaxed);
        float extent = visual.extent.load(std::memory_order_relaxed);
        bool recordingNow =
            module->recordVisual.load(std::memory_order_relaxed) & (1u << i);
        bool erasingNow =
            module->eraseVisual.load(std::memory_order_relaxed) & (1u << i);
        if (extent > 0.f &&
            ((kind == 1 && recordingNow) || (kind == 2 && erasingNow))) {
          auto stroke = [&](float left, float right) {
            drawSpan(args, left, right, y, lineWidth + .6f,
                     kind == 2 ? nvgRGB(235, 208, 70) : nvgRGB(232, 69, 67));
          };
          if (extent >= period)
            stroke(base, base + period);
          else {
            stroke(from, std::min(from + extent, base + period));
            if (from + extent > base + period)
              stroke(base, from + extent - period);
          }
        }
      }
      drawLoopMarker(args, xAt(requested.x), y, tickHeight, true);
      drawLoopMarker(args, xAt(endPoint(requested.y)), y, tickHeight, false);
      float position = module ? module->positions[i].load() : start + i * .1f;
      float x = xAt(wrapWav(position));
      drawTracker(args, i, x, y, dotScaleX, dotScaleY);
      nvgRestore(args.vg);
    }
    widget::Widget::draw(args);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && !args.fb && module) {
      int count = clamp(
          int(module->params[ComputerscarePhlooper::COUNT].getValue()), 1, 16);
      float row = (box.size.y - 12.f) / count;
      auto xAt = [&](float position) {
        return visualX(phlooper::zoomPoint(position, zoomRange));
      };
      nvgSave(args.vg);
      nvgIntersectScissor(args.vg, visualLeft(), 0.f,
                          box.size.x - visualLeft() - 10.f, box.size.y);
      nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
      auto stroke = [&](float left, float right, float y, NVGcolor color) {
        drawSpan(args, left, right, y, std::min(4.f, row * .45f), color);
      };
      // The right-hand marker is always the actual source WAV boundary.
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, xAt(1.f), 3.f);
      nvgLineTo(args.vg, xAt(1.f), box.size.y - 3.f);
      nvgStrokeColor(args.vg, nvgRGBA(205, 213, 205, 140));
      nvgStrokeWidth(args.vg, .8f);
      nvgStroke(args.vg);
      for (int i = 0; i < count; ++i) {
        float y = 6.f + row * (i + .5f);
        if (int(module->params[ComputerscarePhlooper::VISUAL_MODE]
                    .getValue()) != 1)
          stroke(
              module->starts[i].load(), module->ends[i].load(), y,
              int(module->params[ComputerscarePhlooper::VISUAL_MODE]
                      .getValue()) == 0
                  ? nvgRGB(67 + int(123 * i / 15.f), 126 + int(104 * i / 15.f),
                           119 + int(38 * i / 15.f))
                  : nvgRGB(240, 243, 240));
        auto& visual = module->writeVisual[i];
        int kind = visual.kind.load(std::memory_order_relaxed);
        unsigned active =
            kind == 1 ? module->recordVisual.load(std::memory_order_relaxed)
                      : module->eraseVisual.load(std::memory_order_relaxed);
        if (kind && (active & (1u << i))) {
          float base = visual.base.load(), period = visual.period.load();
          float from = visual.from.load(), extent = visual.extent.load();
          NVGcolor color =
              kind == 1 ? nvgRGB(232, 69, 67) : nvgRGB(235, 208, 70);
          if (extent >= period)
            stroke(base, base + period, y, color);
          else {
            stroke(from, std::min(from + extent, base + period), y, color);
            if (from + extent > base + period)
              stroke(base, from + extent - period, y, color);
          }
        }
        float x = xAt(wrapWav(module->positions[i].load()));
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        Vec requested = requestedRegion(i);
        drawLoopMarker(args, xAt(requested.x), y, row * .46f, true);
        drawLoopMarker(args, xAt(endPoint(requested.y)), y, row * .46f, false);
        float scaleY = std::min(1.f, row * .36f / 7.f);
        drawTracker(args, i, x, y, std::max(.65f, scaleY), scaleY);
      }
      nvgRestore(args.vg);
    }
    widget::Widget::drawLayer(args, layer);
  }
};
struct PhlooperTiming : widget::TransparentWidget {
  ComputerscarePhlooper* module = nullptr;
  void draw(const DrawArgs& args) override {
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 8.5f);
    nvgFillColor(args.vg, nvgRGB(15, 42, 36));
    float base = module ? module->frames.load() / 48000.f : 0.f;
    float length =
        module ? module->params[ComputerscarePhlooper::LENGTH].getValue() : 1.f;
    nvgText(args.vg, 0, 9, string::f("Base: %.3fs", base).c_str(), nullptr);
    nvgText(args.vg, 0, 21, string::f("Loop: %.2fs", base * length).c_str(),
            nullptr);
  }
};
struct PhlooperBackplate : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(222, 222, 222));
    nvgFill(args.vg);
    widget::TransparentWidget::draw(args);
  }
};
// Rack draws cables on layer 3 after module lights. A rack-level sibling
// placed after the cable container keeps shortcut badges above the cables.
struct PhlooperShortcutOverlay : widget::TransparentWidget {
  std::weak_ptr<std::function<void(const DrawArgs&)>> drawShortcuts;
  void step() override {
    if (drawShortcuts.expired()) requestDelete();
    widget::TransparentWidget::step();
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 3)
      if (auto draw = drawShortcuts.lock()) (*draw)(args);
  }
};
struct ComputerscarePhlooperWidget : ModuleWidget {
  std::shared_ptr<std::function<void(const DrawArgs&)>> shortcutDraw;
  void drawShortcuts(const DrawArgs& args) {
    if (args.fb || !module || !APP->window->win ||
        glfwGetKey(APP->window->win, GLFW_KEY_GRAVE_ACCENT) != GLFW_PRESS)
      return;
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    const int ids[] = {
        ComputerscarePhlooper::STOP,  ComputerscarePhlooper::MUTE,
        ComputerscarePhlooper::HOLD,  ComputerscarePhlooper::RECORD,
        ComputerscarePhlooper::ERASE, ComputerscarePhlooper::RESTART,
        ComputerscarePhlooper::ZOOM,  ComputerscarePhlooper::SHOW_TRANSPORT};
    const char* keys[] = {"[space]", "[m]", "[h]", "[r]",
                          "[e]",     "[t]", "[z]", "[c]"};
    nvgSave(args.vg);
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 12.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    for (int i = 0; i < 8; ++i) {
      auto* control = getParam(ids[i]);
      if (!control) continue;
      float x = control->box.pos.x + control->box.size.x - 2.f;
      float y = control->box.pos.y + 1.f;
      float width =
          nvgTextBounds(args.vg, 0, 0, keys[i], nullptr, nullptr) + 8.f;
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, x - width * .5f, y - 8.f, width, 16.f, 3.f);
      nvgFillColor(args.vg, nvgRGBA(13, 29, 28, 185));
      nvgFill(args.vg);
      nvgFillColor(args.vg, nvgRGB(220, 243, 228));
      nvgText(args.vg, x, y, keys[i], nullptr);
    }
    nvgRestore(args.vg);
  }
  bool eraseKeyHeld = false;
  bool restartKeyHeld = false;
  void step() override {
    if (module && parent && !shortcutDraw) {
      shortcutDraw = std::make_shared<std::function<void(const DrawArgs&)>>(
          [this](const DrawArgs& args) {
            nvgSave(args.vg);
            Vec offset = getRelativeOffset(Vec(), APP->scene->rack);
            nvgTranslate(args.vg, offset.x, offset.y);
            drawShortcuts(args);
            nvgRestore(args.vg);
          });
      auto* overlay = new PhlooperShortcutOverlay;
      overlay->drawShortcuts = shortcutDraw;
      APP->scene->rack->addChild(overlay);
    }
    if (module && APP->window->win) {
      if (eraseKeyHeld &&
          glfwGetKey(APP->window->win, GLFW_KEY_E) != GLFW_PRESS) {
        module->params[ComputerscarePhlooper::ERASE].setValue(0.f);
        eraseKeyHeld = false;
      }
      if (restartKeyHeld &&
          glfwGetKey(APP->window->win, GLFW_KEY_T) != GLFW_PRESS) {
        module->params[ComputerscarePhlooper::RESTART].setValue(0.f);
        restartKeyHeld = false;
      }
    }
    ModuleWidget::step();
  }
  void onHoverKey(const event::HoverKey& e) override {
    if (!module || e.isConsumed() || (e.mods & RACK_MOD_MASK)) {
      ModuleWidget::onHoverKey(e);
      return;
    }
    int param = -1;
    bool momentary = false;
    switch (e.key) {
      case GLFW_KEY_SPACE:
        param = ComputerscarePhlooper::STOP;
        break;
      case GLFW_KEY_Z:
        param = ComputerscarePhlooper::ZOOM;
        break;
      case GLFW_KEY_C:
        param = ComputerscarePhlooper::SHOW_TRANSPORT;
        break;
      case GLFW_KEY_M:
        param = ComputerscarePhlooper::MUTE;
        break;
      case GLFW_KEY_H:
        param = ComputerscarePhlooper::HOLD;
        break;
      case GLFW_KEY_R:
        param = ComputerscarePhlooper::RECORD;
        break;
      case GLFW_KEY_E:
        param = ComputerscarePhlooper::ERASE;
        momentary = true;
        break;
      case GLFW_KEY_T:
        param = ComputerscarePhlooper::RESTART;
        momentary = true;
        break;
      default:
        ModuleWidget::onHoverKey(e);
        return;
    }
    if (e.action == GLFW_PRESS) {
      if (momentary) {
        module->params[param].setValue(1.f);
        if (param == ComputerscarePhlooper::ERASE)
          eraseKeyHeld = true;
        else
          restartKeyHeld = true;
      } else {
        auto* change = new history::ParamChange;
        change->name = module->getParamQuantity(param)->getLabel();
        change->moduleId = module->id;
        change->paramId = param;
        change->oldValue = module->params[param].getValue();
        change->newValue = change->oldValue > .5f ? 0.f : 1.f;
        module->params[param].setValue(change->newValue);
        APP->history->push(change);
      }
    } else if (momentary && e.action == GLFW_RELEASE) {
      module->params[param].setValue(0.f);
      if (param == ComputerscarePhlooper::ERASE)
        eraseKeyHeld = false;
      else
        restartKeyHeld = false;
    }
    e.consume(this);
  }
  ComputerscarePhlooperWidget(ComputerscarePhlooper* module) {
    setModule(module);
    box.size = Vec(330, 380);
    auto* backplate = new PhlooperBackplate;
    backplate->box.size = box.size;
    addChild(backplate);
    auto block = [&](Rect bounds, float depth, float distortion, unsigned seed,
                     int grey) {
      auto* plaque = new phlooper::WarpedBlock;
      plaque->configure(bounds, depth, distortion, seed,
                        nvgRGB(grey, grey, grey));
      backplate->addChild(plaque);
    };
    block(Rect(Vec(9, 3), Vec(156, 31)), 4.f, .9f, 11, 205);
    block(Rect(Vec(174, 3), Vec(147, 31)), 4.f, 1.3f, 12, 210);
    block(Rect(Vec(9, 171), Vec(312, 78)), 4.f, 2.5f, 13, 236);
    block(Rect(Vec(9, 251), Vec(312, 62)), 4.f, 1.7f, 14, 210);
    block(Rect(Vec(9, 313), Vec(153, 58)), 4.f, 1.8f, 15, 232);
    block(Rect(Vec(168, 313), Vec(153, 58)), 4.f, 2.2f, 16, 175);
    for (int i = 0; i < 2; ++i)
      block(Rect(Vec(18 + i * 159, 316), Vec(135, 18)), 2.5f, .9f,
            unsigned(17 + i), 205);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscarePhlooperPanel.svg")));
    addChild(panel);
    auto* view = new PhlooperView;
    view->module = module;
    view->box = Rect(Vec(10, 60), Vec(310, 108));
    const int transportParams[] = {ComputerscarePhlooper::LOOP_MUTE,
                                   ComputerscarePhlooper::LOOP_SOLO,
                                   ComputerscarePhlooper::LOOP_PAUSE,
                                   ComputerscarePhlooper::LOOP_DIRECTION};
    const char* letters[] = {"M", "S", "P", "F"};
    for (int i = 0; i < 16; ++i)
      for (int j = 0; j < 4; ++j) {
        auto* button = createParam<PhlooperTransportButton>(
            Vec(), module, transportParams[j] + i);
        button->letter = letters[j];
        button->directionButton = j == 3;
        view->transport[i][j] = button;
        view->addChild(button);
      }
    addChild(view);
    auto buttonWidth = [&](const char* caption) {
      auto font = APP->window->loadFont(
          asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
      if (!font) return 48.f;
      nvgSave(APP->window->vg);
      nvgFontFaceId(APP->window->vg, font->handle);
      nvgFontSize(APP->window->vg, 13.f);
      float width = std::ceil(nvgTextBounds(APP->window->vg, 0.f, 0.f, caption,
                                            nullptr, nullptr)) +
                    12.f;
      nvgRestore(APP->window->vg);
      return width;
    };
    float modeWidth =
        std::max(buttonWidth("Length %"),
                 std::max(buttonWidth("Time ms"), buttonWidth("Speed %")));
    float directionWidth =
        std::max(buttonWidth("Fwd / Rev"),
                 std::max(buttonWidth("Forward"), buttonWidth("Reverse")));
    float directionX = 12.f + modeWidth + 6.f;
    float controlsX = directionX + directionWidth + 6.f;
    float controlsWidth = buttonWidth("Ctrl");
    float zoomX = controlsX + controlsWidth + 6.f;
    float zoomWidth = buttonWidth("Zoom");
    auto* showControls = createParam<PhlooperButton>(
        Vec(controlsX, 38), module, ComputerscarePhlooper::SHOW_TRANSPORT);
    showControls->momentary = false;
    showControls->face.caption = "Ctrl";
    showControls->box.size = Vec(controlsWidth, 20);
    showControls->face.configure(controlsWidth, 20, 4.f, 3.f, 1.2f, 23);
    addParam(showControls);
    auto* timing = new PhlooperTiming;
    timing->module = module;
    timing->box = Rect(Vec(181, 7), Vec(72, 24));
    addChild(timing);
    for (int i = 0; i < 3; ++i) {
      auto* choice = new PhlooperChoice;
      choice->module = module;
      choice->param = i == 0   ? ComputerscarePhlooper::COUNT
                      : i == 1 ? ComputerscarePhlooper::MODE
                               : ComputerscarePhlooper::LOOP_DIRECTION;
      choice->box = i == 0 ? Rect(Vec(255, 9), Vec(buttonWidth("16 loops"), 20))
                    : i == 1
                        ? Rect(Vec(12, 38), Vec(modeWidth, 20))
                        : Rect(Vec(directionX, 38), Vec(directionWidth, 20));
      choice->configure(choice->box.size.x, choice->box.size.y, 4.f, 4.f, 1.2f,
                        unsigned(i + 1));
      addChild(choice);
    }
    auto* zoom = createParam<PhlooperButton>(Vec(zoomX, 38), module,
                                             ComputerscarePhlooper::ZOOM);
    zoom->momentary = false;
    zoom->face.caption = "Zoom";
    zoom->box.size = Vec(zoomWidth, 20);
    zoom->face.configure(zoomWidth, 20, 4.f, 3.f, 1.2f, 24);
    addParam(zoom);
    for (int i = 0; i < 2; ++i) {
      auto* meter = new ComputerscareMeter;
      meter->signal =
          module ? (i == 0 ? &module->inputMeter : &module->outputMeter)
                 : nullptr;
      meter->label = "";
      meter->shapeSeed = random::u32();
      meter->box = Rect(Vec(21 + i * 159, 319), Vec(127, 12));
      addChild(meter);
    }
    const int knobs[] = {
        ComputerscarePhlooper::START,   ComputerscarePhlooper::LENGTH,
        ComputerscarePhlooper::OFFSET,  ComputerscarePhlooper::SPEED,
        ComputerscarePhlooper::REC_MIX, ComputerscarePhlooper::MIX};
    for (int i = 0; i < 6; ++i)
      addParam(createParamCentered<SmoothKnob>(Vec(35 + i * 52, 190), module,
                                               knobs[i]));
    const int buttons[] = {
        ComputerscarePhlooper::RECORD,  ComputerscarePhlooper::ERASE,
        ComputerscarePhlooper::RESTART, ComputerscarePhlooper::STOP,
        ComputerscarePhlooper::MUTE,    ComputerscarePhlooper::HOLD};
    const int controlInputs[] = {
        ComputerscarePhlooper::REC_GATE,     ComputerscarePhlooper::ERASE_GATE,
        ComputerscarePhlooper::RESTART_TRIG, ComputerscarePhlooper::STOP_GATE,
        ComputerscarePhlooper::MUTE_GATE,    ComputerscarePhlooper::HOLD_GATE};
    for (int i = 0; i < 6; ++i) {
      float x = i == 0 ? 35.f : 104.f + (i - 1) * 46.f;
      auto* b =
          createParamCentered<PhlooperButton>(Vec(x, 266), module, buttons[i]);
      if (i > 0) {
        float width = i == 2 ? 38.f : 34.f;
        b->box.size.x = width;
        b->box.pos.x = x - width * .5f;
        b->face.configure(width, 20, 4.f, 3.f, 1.2f, unsigned(i + 4));
      }
      b->momentary = i == 1 || i == 2;
      const char* captions[] = {"Record", "Erase", "Restart",
                                "Stop",   "Mute",  "Hold"};
      b->face.caption = captions[i];
      b->face.shapeSeed = unsigned(i + 4);
      addParam(b);
      addInput(
          createInputCentered<InPort>(Vec(x, 290), module, controlInputs[i]));
    }
    auto* recordIndicator = new PhlooperRecordIndicator;
    recordIndicator->module = module;
    recordIndicator->box.pos = Vec(53, 256);
    addChild(recordIndicator);
    const int regionInputs[] = {
        ComputerscarePhlooper::START_CV,   ComputerscarePhlooper::LENGTH_CV,
        ComputerscarePhlooper::OFFSET_CV,  ComputerscarePhlooper::SPEED_CV,
        ComputerscarePhlooper::REC_MIX_CV, ComputerscarePhlooper::OUT_MIX_CV};
    for (int i = 0; i < 6; ++i)
      addInput(createInputCentered<InPort>(Vec(35 + i * 52, 227), module,
                                           regionInputs[i]));
    addParam(createParamCentered<SmallKnob>(Vec(97, 347), module,
                                            ComputerscarePhlooper::INPUT_GAIN));
    addParam(createParamCentered<SmallKnob>(
        Vec(233, 347), module, ComputerscarePhlooper::OUTPUT_GAIN));
    addInput(createInputCentered<InPort>(Vec(139, 347), module,
                                         ComputerscarePhlooper::VCA_CV));
    const float inputX[] = {35.f, 70.f};
    const int audioInputs[] = {ComputerscarePhlooper::LEFT,
                               ComputerscarePhlooper::RIGHT};
    for (int i = 0; i < 2; ++i) {
      if (i == 1)
        addInput(createInputCentered<PointingUpPentagonPort>(
            Vec(inputX[i], 347), module, audioInputs[i]));
      else
        addInput(createInputCentered<InPort>(Vec(inputX[i], 347), module,
                                             audioInputs[i]));
    }
    const float outputX[] = {261.f, 295.f, 191.f};
    const int audioOutputs[] = {ComputerscarePhlooper::OUT_L,
                                ComputerscarePhlooper::OUT_R,
                                ComputerscarePhlooper::EOC};
    for (int i = 0; i < 3; ++i) {
      if (i == 1)
        addOutput(createOutputCentered<PhlooperRightOutputPort>(
            Vec(outputX[i], 347), module, audioOutputs[i]));
      else
        addOutput(createOutputCentered<PointingUpPentagonPort>(
            Vec(outputX[i], 347), module, audioOutputs[i]));
    }
  }
  void onPathDrop(const event::PathDrop& e) override {
    auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
    if (m && !e.paths.empty()) {
      m->load(e.paths.front());
      e.consume(this);
    }
  }
  void appendContextMenu(Menu* menu) override {
    auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createSubmenuItem("Output", "", [m](Menu* submenu) {
      for (int value = 0; value < 2; ++value)
        submenu->addChild(createCheckMenuItem(
            value ? "Polyphonic" : "Mix", "",
            [m, value]() {
              return m->params[ComputerscarePhlooper::OUTPUT_MODE].getValue() ==
                     value;
            },
            [m, value]() {
              auto* change = new history::ParamChange;
              change->name = "Output mode";
              change->moduleId = m->id;
              change->paramId = ComputerscarePhlooper::OUTPUT_MODE;
              change->oldValue =
                  m->params[ComputerscarePhlooper::OUTPUT_MODE].getValue();
              change->newValue = float(value);
              m->params[ComputerscarePhlooper::OUTPUT_MODE].setValue(
                  float(value));
              APP->history->push(change);
            }));
    }));
    menu->addChild(createSubmenuItem("Visualization", "", [m](Menu* submenu) {
      for (int value = 0; value < 2; ++value)
        submenu->addChild(createCheckMenuItem(
            value ? "Waveform" : "Line", "",
            [m, value]() {
              return m->params[ComputerscarePhlooper::VISUAL_MODE].getValue() ==
                     value;
            },
            [m, value]() {
              auto* change = new history::ParamChange;
              change->name = "Visualization";
              change->moduleId = m->id;
              change->paramId = ComputerscarePhlooper::VISUAL_MODE;
              change->oldValue =
                  m->params[ComputerscarePhlooper::VISUAL_MODE].getValue();
              change->newValue = float(value);
              m->params[ComputerscarePhlooper::VISUAL_MODE].setValue(
                  float(value));
              APP->history->push(change);
            }));
    }));
    menu->addChild(createSubmenuItem("Overdub", "", [m](Menu* submenu) {
      for (int value = 0; value < 2; ++value)
        submenu->addChild(createCheckMenuItem(
            value ? "Add" : "Blend", "",
            [m, value]() {
              return m->params[ComputerscarePhlooper::OVERDUB].getValue() ==
                     value;
            },
            [m, value]() {
              auto* change = new history::ParamChange;
              change->name = "Overdub mode";
              change->moduleId = m->id;
              change->paramId = ComputerscarePhlooper::OVERDUB;
              change->oldValue =
                  m->params[ComputerscarePhlooper::OVERDUB].getValue();
              change->newValue = float(value);
              m->params[ComputerscarePhlooper::OVERDUB].setValue(float(value));
              APP->history->push(change);
            }));
    }));
    menu->addChild(createMenuItem("Import WAV to all loops…", "", [m]() {
      char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, nullptr);
      if (path) {
        m->load(path);
        std::free(path);
        if (!m->error.empty())
          osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, m->error.c_str());
      }
    }));
    menu->addChild(
        createSubmenuItem("Import WAV to loop", "", [m](Menu* submenu) {
          for (int i = 0; i < phlooper::voices; ++i)
            submenu->addChild(createMenuItem(
                "Loop " + std::to_string(i + 1) + "…", "", [m, i]() {
                  char* path =
                      osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, nullptr);
                  if (path) {
                    m->load(path, i);
                    std::free(path);
                    if (!m->error.empty())
                      osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK,
                                       m->error.c_str());
                  }
                }));
        }));
    menu->addChild(createSubmenuItem("Export WAV", "", [m](Menu* submenu) {
      auto exportVoice = [m](int voice) {
        std::string filename =
            voice < 0 ? "phlooper.wav"
                      : "loop-" + std::to_string(voice + 1) + ".wav";
        char* path =
            osdialog_file(OSDIALOG_SAVE, nullptr, filename.c_str(), nullptr);
        if (path) {
          std::string destination = path;
          std::free(path);
          m->exportWavs(destination, voice);
        }
      };
      submenu->addChild(createMenuItem("All 16 loops (separate files)…", "",
                                       [exportVoice]() { exportVoice(-1); }));
      submenu->addChild(new MenuSeparator);
      for (int i = 0; i < 16; ++i)
        submenu->addChild(
            createMenuItem("Loop " + std::to_string(i + 1) + "…", "",
                           [exportVoice, i]() { exportVoice(i); }));
    }));
    menu->addChild(createMenuItem("Clear recording", "",
                                  [m]() { m->resetRequest = true; }));
  }
};
Model* modelComputerscarePhlooper =
    createModel<ComputerscarePhlooper, ComputerscarePhlooperWidget>(
        "computerscare-phlooper");
