#pragma once
#include <array>
#include <cstdio>
#include <iomanip>
#include <sstream>

#include "Wav.hpp"
namespace phlooper {
// Hash the actual WAV samples, not the unused right lane of mono recordings.
inline std::string storeAudio(const std::string& dir, const float* data,
                              int frames, bool stereo) {
  uint64_t hash = 14695981039346656037ull;
  auto add = [&](uint32_t bits) {
    for (int b = 0; b < 4; ++b) {
      hash ^= (bits >> (8 * b)) & 255;
      hash *= 1099511628211ull;
    }
  };
  add(frames);
  add(stereo ? 2 : 1);
  for (int i = 0; i < frames; ++i)
    for (int ch = 0; ch < (stereo ? 2 : 1); ++ch) {
      float x = data[2 * i + ch] / 5.f;
      uint32_t bits;
      std::memcpy(&bits, &x, 4);
      add(bits);
    }
  std::ostringstream name;
  name << "audio-" << std::hex << hash;
  for (int collision = 0;; ++collision) {
    std::string file = name.str() +
                       (collision ? "-" + std::to_string(collision) : "") +
                       ".wav",
                path = dir + "/" + file;
    std::ifstream exists(path, std::ios::binary);
    if (exists.good()) {
      exists.close();
      auto wav = loadWav(path);
      bool same = wav.rate == 48000 && wav.channels == (stereo ? 2 : 1) &&
                  wav.samples.size() == size_t(frames * wav.channels);
      for (int i = 0; same && i < frames; ++i)
        for (int ch = 0; ch < wav.channels; ++ch) {
          float x = data[2 * i + ch] / 5.f;
          if (std::memcmp(&x, &wav.samples[i * wav.channels + ch], 4)) {
            same = false;
            break;
          }
        }
      if (same) return file;
      continue;
    }
    saveWav(path + ".tmp", data, frames, stereo);
    if (std::rename((path + ".tmp").c_str(), path.c_str()))
      throw std::runtime_error("Cannot install loop audio");
    return file;
  }
}
inline std::array<std::string, 16> readAudioIndex(const std::string& dir) {
  std::array<std::string, 16> files;
  std::ifstream in(dir + "/audio-index.txt");
  if (!in) {
    for (int i = 0; i < 16; ++i)
      files[i] = "loop-" + std::to_string(i + 1) + ".wav";
    return files;
  }
  std::string magic;
  std::getline(in, magic);
  if (magic != "Phlooper audio 1")
    throw std::runtime_error("Invalid loop audio index");
  for (auto& file : files) {
    if (!std::getline(in, file) || file.empty() ||
        file.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.") !=
            std::string::npos ||
        file.find("..") != std::string::npos)
      throw std::runtime_error("Invalid loop audio reference");
  }
  return files;
}
inline void writeAudioIndex(const std::string& dir,
                            const std::array<std::string, 16>& files) {
  std::ofstream out(dir + "/audio-index.txt.tmp");
  out << "Phlooper audio 1\n";
  for (auto& file : files) out << file << '\n';
  out.close();
  if (!out || std::rename((dir + "/audio-index.txt.tmp").c_str(),
                          (dir + "/audio-index.txt").c_str()))
    throw std::runtime_error("Cannot save loop audio index");
}
}  // namespace phlooper
