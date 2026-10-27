# Reusing the text editor

`ComputerscareTextEditor` is a Rack text-display widget with selection, clipboard shortcuts, per-document undo, layered highlights, and adjustable typography. Blunch adds channel selection, transport, formula versions, and the perspective wall in its own adapter. The editor does not own a sequencer or run on the audio thread.

## Documents and commands

Keep a `ComputerscareTextEditorState` at a stable address for each document. Bind it with `setState()`. The widget owns its undo histories, keyed by document address; those documents must outlive their editor. Switching documents swaps history stacks rather than copying their text snapshots.

The state holds text, cursor/selection offsets, highlights, and a dirty flag. Set `dirty` after an external edit so the widget refreshes. Ordinary editor changes update the bound state. Module-specific editing restrictions belong in a derived adapter, not in the shared state or renderer.

Generic command shortcuts live in `ComputerscareTextEditorShortcuts.hpp`, under `computerscare::text_editor`. Options enable submission, opening, and stop commands. The resulting counters in `ComputerscareTextEditorCommands` are requests for the adapter to handle; the editor does not perform transport actions. Blunch's old shortcut namespace forwards to this shared API for compatibility.

## Viewport and highlights

`ComputerscareTextEditorViewport` converts physical widget coordinates into logical font coordinates. `ComputerscareTextEditorLayout` handles wrapping, logical rows, caret geometry, and hit tests. Measurement must use the same NanoVG transform and font settings as rendering: Rack zoom changes glyph advance quantization. Do not replace it with fixed character widths.

The viewport is currently fixed to the widget rectangle. Scrolling or other window-management policies can be added by an adapter; no scrolling or text navigation behavior changed in this refactor.

Highlights are data: ranges, foreground/background colors, borders, progress bars, and row appearance. Shared distorted paths are in `ComputerscareTextEditorHighlightGeometry.hpp`. The renderer reuses one layout within each synchronous background, foreground, or decoration pass. It resets that cache before every pass, so document, font, viewport, and zoom changes cannot leave stale geometry. Painting and hit testing retain the same coordinates.

## Themes

`ComputerscareTextEditorTheme.hpp` provides semantic colors for text, selection, stripes, inactive rows, focus, timing highlights, and captions. Applying a theme changes colors only; it does not change font metrics, playback, or document state. Adapters use the same color roles for their own decorations.

Blunch saves its theme name in patch data. Missing or unknown names fall back to Moss, preserving the original appearance. The shared widget does not serialize module settings itself.

## Checks and profiling

Run `scripts/dev.sh check` for the Rack build and standalone tests. Run `scripts/preview-module.sh blunch --controls` for native interaction, clipboard, layout, theme persistence, and caret checks, plus screenshots of all four themes.

Set `BLUNCH_EDITOR_BENCHMARK=1` on that preview command to measure widget stepping plus native rendering of the same 16-row Control-view fixture. It reports the median of five batches of twenty frames, without PNG-writing time. Compare runs on the same machine and scale; it is a UI measurement, not audio DSP CPU usage. Output stays under ignored `build/previews/`.
