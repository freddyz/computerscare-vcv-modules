# Module previews

Run from the repository checkout:

```sh
scripts/preview-module.sh phlooper
```

This builds the plugin and, when needed, a small development runner, then renders the **real module widget** with Rack's OpenGL/NanoVG framebuffer renderer. It does not start the Rack application, open an audio device, load a patch, or change your Rack settings. It creates a hidden graphics window; macOS graphics access is required. Its preview-only window initialization also works when no physical monitor is active. This is an offscreen renderer, not a display-server-free renderer.

The default output is `build/previews/phlooper/`, already excluded from Git by the repository's `build` rule. Seven PNGs show the empty module, four-loop playback, sixteen-loop playback, a zoomed region, overdubbing, muted playback with per-channel controls hidden, and a mute/solo background comparison. Audio is generated and recorded through the module's real processing code. The runner uses isolated scratch storage under the output directory.

```sh
# Render Blunch using its actual browser widget.
scripts/preview-module.sh blunch

# Exercise Blunch's real editor keyboard/mouse, transport, and row layout,
# then capture its selection and inactive-row stripes.
scripts/preview-module.sh blunch --controls

# Compare editor stepping/rendering cost (excludes PNG writing).
BLUNCH_EDITOR_BENCHMARK=1 scripts/preview-module.sh blunch --controls

# Benchmark Phlooper playback at 3 and 16 loops (no PNG capture).
PHLOOPER_DSP_BENCHMARK=1 scripts/preview-module.sh phlooper --no-build

# Render only the exact null-module browser preview.
scripts/preview-module.sh --browser

# Skip the plugin build; reuse the runner when its dependencies are unchanged.
scripts/preview-module.sh --no-build

# Increase resolution or save elsewhere.
scripts/preview-module.sh --scale 3 --output /tmp/phlooper-previews
```

`RACK_SOURCE_DIR` can select a built Rack source checkout containing `libRack.dylib`, `include/`, `dep/include/`, and `res/`. Otherwise the script uses `RACK_DIR`, or `~/dev/VCV-Rack/Rack`. If `RACK_DIR` points at the SDK, it looks for the adjacent `Rack` source checkout because the SDK does not include system artwork/fonts. The current runner supports macOS, Phlooper and Blunch.

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

Blunch editor captures also include `theme-Mauer.png`, `theme-Garnett.png`, `theme-Moss.png`, and `theme-Puckett.png`. These use different channel arrangements to check both overview and detail views. Theme changes are checked against the same caret and viewport geometry.

Use `scripts/preview-module.sh groups` to render the current family, basic polyphonic utilities, advanced polyphonic utilities, and visualization modules. The family is checked against all non-hidden entries in `plugin.json`. The utility groups use real offline patches and Rack cables. Portaloof processes a fresh capture of the displayed modules. No audio device or existing patch is opened. Generated files stay in `build/previews/groups/`; copy approved documentation images into `doc/images/`.

Voly Pector normally renders just its browser preview. Set `VOLY_PREVIEW_CHECKS=1` to run the keyboard and multi-selection interaction checks and capture their example states.

Blunch's `--controls` checks also exercise Settings navigation, shared context-menu values, immediate changes, and formula/focus preservation. They capture `settings-root.png`, `settings-playback.png`, `settings-wait.png`, and `settings-splash.png`. Set `BLUNCH_SETTINGS_ANIMATION=1` to export 60 fps transition frames under `settings-animation/`.

Blunch Help is generated from `doc/blunch.md` with `python3 scripts/generate-blunch-help.py`. `scripts/dev.sh check` checks that it is current. The Settings fixture compiles every copyable example and checks reader navigation, clipboard copying, and preservation of editor text and selection; it also captures the Help pages.

The Blunch interaction checks include repeat-argument sequences across line/page cycles and resets, plus complete Rack save/reload of per-channel started/stopped states with master Run on or off.

For a sparse Beekapoo print spectrum, render 36 cases (mostly zero to five
exposed controls, including mixed knobs/buttons and empty cases):

```sh
BEEKAPOO_PRINT_SPECTRUM=1 scripts/preview-module.sh beekapoo --scale 3 --output build/previews/beekapoo-print
python3 scripts/preview/make-spectrum-pdf.py build/previews/beekapoo-print output/pdf/beekapoo-spectrum-36.pdf
```

The PDF command requires ReportLab and embeds its bundled Vera fonts. It produces
two Letter pages, 18 layouts per page. Images and the count manifest stay in the
ignored preview directory; no production module code is changed.

For Beekapoo review PDFs, use one page unless requested otherwise. Keep source
module names below each panel; omit HP and parameter/input/output counts.

`BEEKAPOO_KNOBS_ONLY=15 scripts/preview-module.sh beekapoo --no-build` renders
15 labeled knobs across varied patch hashes, values and light/dark backgrounds.
Use `BEEKAPOO_KNOBS_ONLY=1` for the wider 96-knob sheet.

`BEEKAPOO_THEMES=1 scripts/preview-module.sh beekapoo --no-build` renders three
panels in Light and Dark, plus both browser themes. It checks saved overrides,
the shared default, live Rack preference changes, and unchanged parameter
values and positions. Preferences are written only to the preview's scratch
directory.

`BEEKAPOO_FONTS=1 scripts/preview-module.sh beekapoo --no-build` renders both
computerscare fonts with alphabets, numbers, punctuation, four glyph variants
and real label examples in light and dark. It checks equal advances, stable
cached selection, Unicode/whitespace preservation, and native name-field hit
testing. Font sources, profiles and regeneration instructions are under
`scripts/fonts/` and `res/fonts/computerscare/`.

`BEEKAPOO_CHOMP=1 scripts/preview-module.sh beekapoo --no-build` captures the
selection-logo animation in light and dark (idle and hover speed). It checks
header hit testing and stable placement across widths, selection with the
owner included, undo/redo and tooltip cleanup.

`BEEKAPOO_POLYPHONY=1 scripts/preview-module.sh beekapoo --no-build` validates
native input/output polyphony, attenuation and Eject/Undo persistence, then
renders four different panel widths, alternating light and dark themes.

`BEEKAPOO_LABELS=1 scripts/preview-module.sh beekapoo --no-build` checks compact
input/output label fitting using library names and renders four widths,
alternating light and dark themes.

`BEEKAPOO_REGENERATE=1 scripts/preview-module.sh beekapoo --no-build` checks
exact exposed-control counts (including zero), exterior cable stacks,
generated/custom names, locks and Undo/Redo. Full Beekapoo checks also test
logo-based merging alongside Create Beek-A-Poo, including feedback and gains.

`BEEKAPOO_SECTION_CONTROLS=1 scripts/preview-module.sh beekapoo --no-build` checks
section re-roll/remove actions, locks, input gains, stacked cables, self-patches
and Undo/Redo, then renders the hover strips at four widths in light/dark.

`BEEKAPOO_COPY_CABLES=1 scripts/preview-module.sh beekapoo --no-build` checks
native duplication with and without cables, stacked inputs, panel loopbacks,
hidden bridge plugs, Undo/Redo and repair of old phantom connections.

`BEEKAPOO_CONTEXT_CHECKS=1 scripts/preview-module.sh beekapoo --no-build`
checks graphics cleanup and recreation for all exposed jack sizes, then tests
window-before-scene shutdown with collapsed, expanded and editing subpatches.

`BEEKAPOO_MODULE_SETS=1 scripts/preview-module.sh beekapoo --no-build` checks
module-set persistence, per-Beek selection, defaults, restricted generation and
growth, Undo/Redo, and browser interactions. It renders `module-sets.png` and
writes preferences only in the isolated runner's scratch directory.

`BEEKAPOO_CABLE_STATE=1 scripts/preview-module.sh beekapoo --no-build` checks
merge/absorb wiring in both directions, native plug positions and visibility,
stacked polyphonic signal gains, Edit/Near/Far, Eject, Undo/Redo and reload.
It also checks input minus: unpatched-first removal, self-patch folding,
outside-source protection, locks and native 8vert gain preservation.
Optionally set `BEEKAPOO_MERGE_PATCH` to an extracted `patch.json` to inspect
the reported dangling-cable topology. Device modules are excluded; the source
patch is read only. This capture is written as `merged-panel.png`.

`BEEKAPOO_SHARED_INPUTS=1 scripts/preview-module.sh beekapoo --no-build`
checks Core/stoermelder conversion without custom relays, shared fan-out,
overlapping input destinations, duplicate stacks, four-channel attenuation,
self-patching, input-minus internalization, silence on unplug/bypass, destination
removal, native and portable save/load, duplication, both merge paths, Eject and
Undo/Redo. It renders a shared-input example and uses the real Fundamental 8vert for output sums and retained gains.
