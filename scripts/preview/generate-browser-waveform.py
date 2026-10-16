#!/usr/bin/env python3
"""Bake 256 speech waveform extrema into the browser's display-only fixture."""
import argparse
from pathlib import Path
import struct
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("wav", type=Path, help="16-bit PCM WAV; mono or stereo")
parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[2] / "src/Phlooper/BrowserWaveform.hpp")
args = parser.parse_args()
with wave.open(str(args.wav)) as audio:
    if audio.getsampwidth() != 2 or audio.getcomptype() != "NONE":
        parser.error("Expected uncompressed 16-bit PCM WAV")
    count, channels, rate = audio.getnframes(), audio.getnchannels(), audio.getframerate()
    if count < 256:
        parser.error("Need at least 256 audio frames")
    samples = struct.unpack("<" + "h" * (count * channels), audio.readframes(count))
peak = max(abs(value) for value in samples)
if not peak:
    parser.error("The WAV is silent")
rows = []
for index in range(256):
    segment = samples[index * count // 256 * channels:(index + 1) * count // 256 * channels]
    rows.append((min(segment) / peak * 5, max(segment) / peak * 5))
source = """#pragma once

namespace phlooper {
// Fixed extrema from locally synthesized speech, not recorded user audio.
// Regenerate with scripts/preview/generate-browser-waveform.py.
static const float browserWaveformSeconds = %.6ff;
static const float browserWaveform[256][2] = {
""" % (count / rate)
source += "".join("  {%.6ff, %.6ff},\n" % row for row in rows)
source += "};\n}  // namespace phlooper\n"
args.output.write_text(source)
print(args.output)
