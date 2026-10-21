# Module previews

Run from the repository checkout:

```sh
scripts/preview-module.sh phlooper
```

This builds the plugin and, when needed, a small development runner, then renders the **real module widget** with Rack's OpenGL/NanoVG framebuffer renderer. It does not start the Rack application, open an audio device, load a patch, or change your Rack settings. It creates a hidden graphics window; macOS graphics access is required. Its preview-only window initialization also works when no physical monitor is active. This is an offscreen renderer, not a display-server-free renderer.

The default output is `build/previews/phlooper/`, already excluded from Git by the repository's `build` rule. Seven PNGs show the empty module, four-loop playback, sixteen-loop playback, a zoomed region, overdubbing, muted playback with per-channel controls hidden, and a mute/solo background comparison. Audio is generated and recorded through the module's real processing code. The runner uses isolated scratch storage under the output directory.

```sh
# Render 32nd & Columbus or Blunch using their actual browser widgets.
scripts/preview-module.sh 32nd-and-columbus
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

`RACK_SOURCE_DIR` can select a built Rack source checkout containing `libRack.dylib`, `include/`, `dep/include/`, and `res/`. Otherwise the script uses `RACK_DIR`, or `~/dev/VCV-Rack/Rack`. If `RACK_DIR` points at the SDK, it looks for the adjacent `Rack` source checkout because the SDK does not include system artwork/fonts. The current runner supports macOS, Phlooper, Blunch, and 32nd & Columbus.

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

Regenerate screenshots whenever requested; do not serve older captures. The compiled runner remains cached for fast renders.

For optional 32nd & Columbus trigger-routing integration checks, run
`WINDCHIMES_PREVIEW_CHECKS=1 scripts/preview-module.sh 32nd-and-columbus --no-build`.

Set `JACK_PREVIEW_CHECKS=1` when running a preview to verify that all six shared jack variants load at their expected dimensions. Jack names and artwork are listed in [Jack artwork](../../doc/development/jacks.md).

For jack-style comparisons, browser previews also support `mely-porge`, `voly-pector`, and `laundry-soup`.

Blunch’s `--controls` checks include a two-thread regression for editing, staging,
cancellation, serialization, and Ctrl+Space while real audio callbacks process all
16 channels. A deterministic contention test verifies that the callback never
waits on the GUI and that a queued formula cannot be consumed while the GUI owns
the model. Blunch UI/event/render access uses `BlunchStateLock`; model entry points
use `StateGuard`. Keep direct access to formula maps and runtime programs inside
those guards. The audio callback uses a nonblocking attempt and retains elapsed
clock time when access is deferred.

The `32nd-and-columbus` preview also writes `01-forest.png`, showing the Forest graphic option with a live module widget and a fresh arrangement of three to five chimes with varied materials and model settings.

Forest previews also render `02-forest-open.png` and `03-forest-thicket.png` with contrasting Forest settings. The fixture simulates wind through the real module before capturing the scene.

Set `FOREST_STORM_PREVIEW=1` for a short live storm and settling sequence (`04-storm-ground.png`, `05-storm-settled.png`). The native fixture also checks that debris settles at its tree’s radial ground position and leaves the scene without wrapping.

### Toly Pensor

`scripts/preview-module.sh toly-pensor` renders the null-module browser widget. Set `TOLY_PENSOR_PREVIEW_CHECKS=1` to exercise the real SIMD engine, scope actions, trigger edges, persistence, Undo/Redo, mouse routing, contextual views, and a sparse/dense benchmark. Additional captures show sixteen controls, a connected grid, and hidden controls.
