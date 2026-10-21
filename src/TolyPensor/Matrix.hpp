#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
namespace toly {
constexpr int ports = 16, channels = 16, signals = 256, cells = 65536;
enum Axis { InPort, InChannel, OutPort, OutChannel, All };
inline int signal(int port, int channel) { return port * 16 + channel; }
inline int cell(int ip, int ic, int op, int oc) {
  return signal(op, oc) * 256 + signal(ip, ic);
}
inline std::array<int, 4> indices(int cell) {
  return {{(cell % 256) / 16, cell % 16, (cell / 256) / 16, (cell / 256) % 16}};
}
inline int sliceCell(std::array<int, 4> fixed, int rowAxis, int row,
                     int columnAxis = -1, int column = 0) {
  fixed[rowAxis] = row;
  if (columnAxis >= 0) fixed[columnAxis] = column;
  return cell(fixed[0], fixed[1], fixed[2], fixed[3]);
}
struct Matrix {
  std::array<std::atomic<float>, cells> weights;
  std::array<std::array<std::atomic<float>, 16>, 4> scale, offset;
  std::atomic<unsigned> revision{1};
  struct Entry {
    int source;
    alignas(16) float weight[4];
  };
  std::array<std::vector<Entry>, 64> plan;
  std::array<bool, 64> dense{};
  std::unique_ptr<float[]> denseWeights{new float[cells]};
  std::array<int, 16> inputWidths{};
  std::vector<int> sources;
  std::vector<int> groups;
  std::array<float, 256> inputScale{}, inputOffset{}, outputScale{},
      outputOffset{};
  unsigned outputMask = 0, compiledRevision = 0;
  int polyphony = 16;
  Matrix() {
    sources.reserve(256);
    groups.reserve(64);
    for (auto& list : plan) list.reserve(256);
    for (int i = 0; i < cells; ++i)
      weights[i].store(i / 256 == i % 256 ? 1.f : 0.f);
    for (int axis = 0; axis < 4; ++axis)
      for (int i = 0; i < 16; ++i) {
        scale[axis][i] = 1;
        offset[axis][i] = 0;
      }
  }
  void set(int index, float value) {
    weights[index].store(value, std::memory_order_relaxed);
  }
  float get(int index) const {
    return weights[index].load(std::memory_order_relaxed);
  }
  void changed() { revision.fetch_add(1, std::memory_order_release); }
  void prepare(const std::array<int, 16>& widths, unsigned outputs, int poly) {
    unsigned version = revision.load(std::memory_order_acquire);
    if (compiledRevision == version && inputWidths == widths &&
        outputMask == outputs && polyphony == poly)
      return;
    inputWidths = widths;
    outputMask = outputs;
    polyphony = std::max(1, std::min(16, poly));
    for (int group = 0; group < 64; ++group) {
      auto& entries = plan[group];
      entries.clear();
      int op = group / 4, first = (group % 4) * 4;
      if (!(outputs & (1u << op)) || first >= polyphony) continue;
      for (int ip = 0; ip < 16; ++ip)
        for (int ic = 0; ic < std::max(0, std::min(16, widths[ip])); ++ic) {
          Entry entry;
          entry.source = signal(ip, ic);
          bool active = false;
          for (int lane = 0; lane < 4; ++lane) {
            entry.weight[lane] = first + lane < polyphony
                                     ? get(cell(ip, ic, op, first + lane))
                                     : 0;
            active |= entry.weight[lane] != 0;
          }
          if (active) entries.push_back(entry);
        }
    }
    for (int group = 0; group < 64; ++group) {
      dense[group] = plan[group].size() == 256;
      if (dense[group])
        for (const auto& entry : plan[group])
          for (int lane = 0; lane < 4; ++lane)
            denseWeights[(group * 4 + lane) * 256 + entry.source] =
                entry.weight[lane];
    }
    std::array<bool, 256> needed{};
    sources.clear();
    for (const auto& group : plan)
      for (const auto& entry : group) needed[entry.source] = true;
    for (int i = 0; i < 256; ++i)
      if (needed[i]) sources.push_back(i);
    groups.clear();
    for (int group = 0; group < 64; ++group)
      if ((outputs & (1u << (group / 4))) && (group % 4) * 4 < polyphony)
        groups.push_back(group);
    for (int p = 0; p < 16; ++p)
      for (int c = 0; c < 16; ++c) {
        int i = signal(p, c);
        float inPortScale = scale[InPort][p].load(std::memory_order_relaxed);
        float inChannelScale =
            scale[InChannel][c].load(std::memory_order_relaxed);
        inputScale[i] = inPortScale * inChannelScale;
        inputOffset[i] = offset[InPort][p].load(std::memory_order_relaxed) +
                         offset[InChannel][c].load(std::memory_order_relaxed);
        float outPortScale = scale[OutPort][p].load(std::memory_order_relaxed);
        outputScale[i] =
            scale[OutChannel][c].load(std::memory_order_relaxed) * outPortScale;
        outputOffset[i] =
            offset[OutChannel][c].load(std::memory_order_relaxed) +
            offset[OutPort][p].load(std::memory_order_relaxed);
      }
    compiledRevision = version;
  }
  template <class Vector>
  void process(const std::array<float, 256>& input,
               std::array<float, 256>& output) const {
    alignas(16) std::array<float, 256> source;
    if (sources.size() == 256) {
      for (int i = 0; i < 256; i += 4)
        (Vector::load(input.data() + i) * Vector::load(inputScale.data() + i) +
         Vector::load(inputOffset.data() + i))
            .store(source.data() + i);
    } else {
      for (int index : sources)
        source[index] = input[index] * inputScale[index] + inputOffset[index];
    }
    output.fill(0);
    for (int group : groups) {
      int op = group / 4, first = (group % 4) * 4;
      alignas(16) float mixed[4]{};
      if (dense[group]) {
#if defined(__aarch64__)
        // ARM has enough vector registers to accumulate four destinations
        // together. Reuse input loads; keep the smaller dot kernel for tails
        // and SSE targets, which have fewer available vector registers.
        if (first + 4 <= polyphony) {
          Vector a[4] = {Vector(0.f), Vector(0.f), Vector(0.f), Vector(0.f)};
          Vector b[4] = {Vector(0.f), Vector(0.f), Vector(0.f), Vector(0.f)};
          Vector c[4] = {Vector(0.f), Vector(0.f), Vector(0.f), Vector(0.f)};
          Vector d[4] = {Vector(0.f), Vector(0.f), Vector(0.f), Vector(0.f)};
          const float* weights = denseWeights.get() + group * 4 * 256;
          for (int i = 0; i < 256; i += 16) {
            Vector s0 = Vector::load(source.data() + i),
                   s1 = Vector::load(source.data() + i + 4);
            Vector s2 = Vector::load(source.data() + i + 8),
                   s3 = Vector::load(source.data() + i + 12);
            for (int lane = 0; lane < 4; ++lane) {
              const float* w = weights + lane * 256 + i;
              a[lane] += s0 * Vector::load(w);
              b[lane] += s1 * Vector::load(w + 4);
              c[lane] += s2 * Vector::load(w + 8);
              d[lane] += s3 * Vector::load(w + 12);
            }
          }
          for (int lane = 0; lane < 4; ++lane) {
            alignas(16) float reduced[4];
            ((a[lane] + b[lane]) + (c[lane] + d[lane])).store(reduced);
            mixed[lane] = (reduced[0] + reduced[1]) + (reduced[2] + reduced[3]);
          }
        } else
#endif
        {
          for (int lane = 0; lane < 4 && first + lane < polyphony; ++lane) {
            const float* weights =
                denseWeights.get() + (group * 4 + lane) * 256;
            Vector a(0.f), b(0.f), c(0.f), d(0.f);
            for (int sourceIndex = 0; sourceIndex < 256; sourceIndex += 16) {
              a += Vector::load(source.data() + sourceIndex) *
                   Vector::load(weights + sourceIndex);
              b += Vector::load(source.data() + sourceIndex + 4) *
                   Vector::load(weights + sourceIndex + 4);
              c += Vector::load(source.data() + sourceIndex + 8) *
                   Vector::load(weights + sourceIndex + 8);
              d += Vector::load(source.data() + sourceIndex + 12) *
                   Vector::load(weights + sourceIndex + 12);
            }
            alignas(16) float reduced[4];
            ((a + b) + (c + d)).store(reduced);
            mixed[lane] = (reduced[0] + reduced[1]) + (reduced[2] + reduced[3]);
          }
        }
      } else {
        Vector a(0.f), b(0.f), c(0.f), d(0.f);
        const auto& entries = plan[group];
        size_t i = 0;
        for (; i + 3 < entries.size(); i += 4) {
          a += Vector(source[entries[i].source]) *
               Vector::load(entries[i].weight);
          b += Vector(source[entries[i + 1].source]) *
               Vector::load(entries[i + 1].weight);
          c += Vector(source[entries[i + 2].source]) *
               Vector::load(entries[i + 2].weight);
          d += Vector(source[entries[i + 3].source]) *
               Vector::load(entries[i + 3].weight);
        }
        for (; i < entries.size(); ++i)
          a += Vector(source[entries[i].source]) *
               Vector::load(entries[i].weight);
        ((a + b) + (c + d)).store(mixed);
      }
      for (int lane = 0; lane < 4 && first + lane < polyphony; ++lane) {
        int oc = first + lane;
        int index = signal(op, oc);
        float value = mixed[lane] * outputScale[index] + outputOffset[index];
        output[signal(op, oc)] = std::isfinite(value) ? value : 0.f;
      }
    }
  }
  size_t activePacks() const {
    size_t n = 0;
    for (const auto& list : plan) n += list.size();
    return n;
  }
};
}  // namespace toly
