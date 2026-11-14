# Beekapoo development

Engine modules and cables remain in Rack. Collapsed module widgets live inside zero-size holders registered under their original engine IDs. Their original widgets keep stepping. Exposed controls use the original parameter quantities; mapped jacks retain the original engine endpoints.

`BeekapooRackEdit.hpp` implements Edit subpatch. Editing unwraps originals into direct Rack children in a clear row four rows below Beekapoo. A nonmodal toolbar provides Done, Expose, and an optional read-only panel preview. The stock Rack handles movement, selection, keyboard shortcuts, browser placement and cables. New eligible modules join after the placement drag ends. Done recreates holders and restores zoom, offset and the previous visible selection. Only one editing target is active at a time.

`BeekapooLayout.hpp` is the pure responsive planner. It runs on resize/mapping changes. `layoutControls()` applies control-type or module-first ordering and visibility filters. Hiding controls does not alter engine connections. Small layouts keep tiny jacks and icon controls; labels are added as space permits.

`BeekapooPortable.hpp` serializes contained modules and internal cables when Rack requests JSON, not on the audio or UI tick. Imported/duplicated groups receive independent module and cable IDs, remapped exposure and visibility references, and child actions attached to the native duplicate/paste undo action. Normal full-patch loading still uses the existing engine modules. Missing dependencies fail open rather than binding another group's modules. Local duplication copies available per-module patch storage. Rack selection files do not bundle external assets.

The older proxy-based subview remains as an internal regression fixture; panel and context-menu editing use the native-rack session. Its container-swapping API is not used for the new editing path.

`computerscare-beekapoo` is the current slug. Hidden aliases support `computerscare-peekaboo` and `computerscare-patch-abstractor`. Expanders and nested groups are unsupported.

## Validation and renders

`scripts/preview-module.sh beekapoo` builds an isolated runner using local Rack graphics libraries. It does not operate live Rack or vcvmon. `--no-build` skips the plugin build, and unchanged runners are reused. The former `peekaboo` command is an alias.

The fixture checks native JSON reload, legacy slugs, four-channel DSP, stable cable identities, free rack footprint, mappings, switch behavior, tooltips, grouping undo/redo, native browser return, real-rack editing, module adoption, viewport restoration, independent duplication and selection paste with undo/redo. It also renders 16 responsive layout cases and native editing screenshots into ignored `build/previews/beekapoo/`.

Keyboard and menu deletion include the contents in Rack’s native selection-delete transaction. Other programmatic removal queues contained-module cleanup for the scene hook after native bulk-deletion traversal. This avoids invalidating Rack's copied selection/module lists. Surviving children and their cables are removed with actions inserted into the native delete transaction; Undo restores the complete group. A restored owner cancels an unprocessed removal. End abstraction still releases the children without deleting them.

Expose draws bordered, translucent irregular rectangles over the original controls, green for mapped controls and yellow for hover. Overlays are visible only in Expose mode; the source controls draw normally beneath them. Mapped panel ports draw computerscare jacks, using small artwork at compact widths and standard large artwork otherwise. The original port remains the engine endpoint and receives patching/tooltip events; its graphics and framebuffer are never reused or modified.

Preset imports use one history transaction for the host, contained modules and cables, including replacement of an existing group. Exposure changes use group history; automatic boundary exposure and state restoration do not add history entries. Native selection-menu hooks recognize the complete translated Rack menu and shortcuts, rather than intercepting unrelated Delete/Duplicate labels.

Rack dispatches patch-save callbacks to the original engine modules once; Beekapoo does not forward them again. Explicit duplication prepares contained patch storage before copying it. Cable plugs remain native Rack widgets, with weak references cached per cable to avoid repeated widget-tree searches.

Header artwork is separate from panel geometry. Edit `res/Beekapoo/computerscare-logo.svg` directly; the panel generator does not overwrite it. `scripts/generate-beekapoo-panel.py` generates `res/Beekapoo/title.svg`. Both assets fit their own header areas, so compact widths cannot reveal clipped remnants of the title. The preview fixture includes an 8–12 HP strip with eight parameters and one input/output, checking visibility, labels and control bounds.

Headers show only the full logo at 4–6 HP, `res/Beekapoo/title-stacked.svg` beside it at 7–9 HP, and the single-line title from 10 HP. The panel generator produces both title variants. Section frames expand around fixed control grids, with small gaps and irregular corners; sparse parameter frames fill the space above the bottom I/O blocks.
