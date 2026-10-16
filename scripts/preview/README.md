# Module previews

Run from the repository checkout:

```sh
scripts/preview-module.sh phlooper
```

This builds the plugin and, when needed, a small development runner, then renders the **real module widget** with Rack's OpenGL/NanoVG framebuffer renderer. It does not start the Rack application, open an audio device, load a patch, or change your Rack settings. It creates a hidden graphics window; macOS graphics access is required. Its preview-only window initialization also works when no physical monitor is active. This is an offscreen renderer, not a display-server-free renderer.

The default output is `build/previews/phlooper/`, already excluded from Git by the repository's `build` rule. Six PNGs show the empty module, four-loop playback, sixteen-loop playback, a zoomed region, overdubbing, and muted playback with per-channel controls hidden. Audio is generated and recorded through the module's real processing code. The runner uses isolated scratch storage under the output directory.

```sh
# Render Blunch's actual browser widget.
scripts/preview-module.sh blunch

# Exercise Blunch's real Control-mode keyboard/mouse, transport, and row layout,
# then capture its selection and inactive-row stripes.
scripts/preview-module.sh blunch --controls

# Render only the exact null-module browser preview.
scripts/preview-module.sh --browser

# Skip the plugin build; reuse the runner when its dependencies are unchanged.
scripts/preview-module.sh --no-build

# Increase resolution or save elsewhere.
scripts/preview-module.sh --scale 3 --output /tmp/phlooper-previews
```

`RACK_SOURCE_DIR` can select a built Rack source checkout containing `libRack.dylib`, `include/`, `dep/include/`, and `res/`. Otherwise the script uses `RACK_DIR`, or `~/dev/VCV-Rack/Rack`. If `RACK_DIR` points at the SDK, it looks for the adjacent `Rack` source checkout because the SDK does not include system artwork/fonts. The current runner supports macOS, Phlooper, and Blunch.

The `--browser` option renders only `00-browser.png`, using the exact null-module widget created by Rack’s module browser. Its loop count and region controls are randomized once per widget, with a randomly shifted and stretched speech waveform shared across rows, plus random input/output meter levels and Time/Length/Speed and transport choices. Per-loop controls are hidden in browser previews. It does not construct a DSP module.

Edit `phlooper.cpp` here to change the preview scenarios. It includes the module implementation so fixtures can use its real parameters and processing without adding preview-only behavior to the shipped plugin. The plugin, runner, and Rack library must have matching architectures and compatible Rack versions. This tool deliberately uses Rack's internal rendering APIs and may need adjustment when upgrading Rack.

To replace the baked waveform, generate a local synthetic voice recording and convert it to 16-bit PCM first:

```sh
say -v Samantha -o /tmp/browser-speech.aiff 'A little space. Then another phrase, with a few quiet words. Stop.'
afconvert -f WAVE -d LEI16 /tmp/browser-speech.aiff /tmp/browser-speech.wav
python3 scripts/preview/generate-browser-waveform.py /tmp/browser-speech.wav
```

Only the small extrema table is included in the plugin. Audio files and preview images remain local.

The window helper derives a preview-only constructor from the matching Rack source tree. It keeps Rack’s private layout intact while omitting monitor lookup and interactive window setup. It writes generated code only under ignored `build/previews/`, and never edits the Rack checkout.
