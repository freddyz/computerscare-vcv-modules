#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace phlooper {
inline uint32_t word(const unsigned char* p, int n) {
  uint32_t v = 0;
  for (int i = 0; i < n; ++i) v |= uint32_t(p[i]) << (8 * i);
  return v;
}
struct Wav {
  int channels = 1, rate = 48000;
  std::vector<float> samples;
};
inline Wav loadWav(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  unsigned char header[12];
  if (!in.read(reinterpret_cast<char*>(header), 12) ||
      std::memcmp(header, "RIFF", 4) || std::memcmp(header + 8, "WAVE", 4))
    throw std::runtime_error("Expected a WAV file");
  Wav wav;
  int format = 0, bits = 0;
  std::vector<unsigned char> data;
  while (in) {
    unsigned char h[8];
    if (!in.read(reinterpret_cast<char*>(h), 8)) break;
    uint32_t length = word(h + 4, 4);
    if (length > 128 * 1024 * 1024)
      throw std::runtime_error("WAV chunk is too large");
    if (!std::memcmp(h, "fmt ", 4)) {
      if (length < 16 || length > 65536)
        throw std::runtime_error("Invalid WAV format");
      std::vector<unsigned char> f(length);
      if (!in.read(reinterpret_cast<char*>(f.data()), length))
        throw std::runtime_error("Truncated WAV");
      format = int(word(f.data(), 2));
      wav.channels = int(word(f.data() + 2, 2));
      wav.rate = int(word(f.data() + 4, 4));
      bits = int(word(f.data() + 14, 2));
    } else if (!std::memcmp(h, "data", 4)) {
      data.resize(length);
      if (!in.read(reinterpret_cast<char*>(data.data()), length))
        throw std::runtime_error("Truncated WAV");
    } else
      in.seekg(length, std::ios::cur);
    if (length & 1) in.seekg(1, std::ios::cur);
  }
  if ((wav.channels != 1 && wav.channels != 2) || wav.rate < 8000 ||
      wav.rate > 384000 ||
      !((format == 1 && (bits == 16 || bits == 24 || bits == 32)) ||
        (format == 3 && bits == 32)) ||
      data.empty())
    throw std::runtime_error("Use mono/stereo PCM or float WAV");
  int bytes = bits / 8;
  wav.samples.resize(data.size() / bytes);
  for (size_t i = 0; i < wav.samples.size(); ++i) {
    uint32_t v = word(data.data() + i * bytes, bytes);
    float x = 0;
    if (format == 3)
      std::memcpy(&x, &v, 4);
    else {
      int32_t signedValue = int32_t(v << (32 - bits));
      x = float(signedValue) / 2147483648.f;
    }
    wav.samples[i] = std::isfinite(x) ? x : 0;
  }
  return wav;
}
inline void saveWav(const std::string& path, const float* samples, int frames,
                    bool stereo) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("Cannot write WAV");
  auto put = [&](uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) out.put(char(v >> (8 * i)));
  };
  int channels = stereo ? 2 : 1;
  uint32_t length = uint32_t(frames * channels * 4);
  out.write("RIFF", 4);
  put(36 + length, 4);
  out.write("WAVEfmt ", 8);
  put(16, 4);
  put(3, 2);
  put(channels, 2);
  put(48000, 4);
  put(48000 * channels * 4, 4);
  put(channels * 4, 2);
  put(32, 2);
  out.write("data", 4);
  put(length, 4);
  for (int i = 0; i < frames; ++i)
    for (int ch = 0; ch < channels; ++ch) {
      float x = samples[i * 2 + ch] / 5.f;
      uint32_t bits;
      std::memcpy(&bits, &x, 4);
      put(bits, 4);
    }
  if (!out) throw std::runtime_error("Cannot finish WAV");
}
}  // namespace phlooper
