# Daddy's Home

16 polyphonic inputs and outputs. Each output channel takes one input channel, or blank (0 V). One input can feed several outputs. Initial routing is a to A, b to B, and so on, preserving channel numbers.

The display always shows the full matrix. Rows correspond to outputs A–P; columns are output channels 1–16. Each cell names its source: `a4` means input a, channel 4. `--` means blank. Unavailable sources, unpatched outputs, and destinations beyond the output’s polyphony appear faded.

The outside polyphony knob sets all outputs. `~` means their settings differ. Set individual outputs under **Output polyphony** in the context menu. Auto uses enough channels to include the highest destination with an available source. Missing input channels produce 0 V; mono inputs are not repeated across channels.

## Editing

Click a cell to choose a source from the larger 16×16 input grid. Hover highlights the choice. Click **X** to leave it blank. Escape or clicking outside cancels.

Right-click a cell to enter a source code such as `a5` or `i8`. Enter applies; **X** or `--` clears it. Invalid codes stay in the field until corrected.

The top **Grid / Knobs** button switches the display. In Knobs mode, drag a knob vertically to browse all 257 settings: blank, then a1 through p16. Mod slows dragging; Shift speeds it up. Clicking a knob does not open the source picker. Right-click still accepts text. Hover a knob or grid cell to see its route, such as `a4 → N2`. Grid cells highlight on hover. The last manually edited cell has a yellow highlight that fades. Edits support undo/redo.

## MIDI mapping

All 256 routes are parameters. Learn a cell with MIDI Map in either display mode; switching Grid/Knobs keeps its mapping. A standard 7-bit MIDI CC has only 128 values, so it skips some of the 257 source settings. Grid and text entry select every source exactly.

## Randomize and wiggle

The dice and wavy-line buttons change routes:

- Top right: all routes.
- Right of a row: that output jack.
- Above a column: that output channel across all jacks.

Mod+R (Cmd+R on macOS, Ctrl+R elsewhere) also randomizes all routes. Manual actions support undo/redo. Polyphony stays unchanged.

The two probability knobs set the chance of changing each route. Their CV inputs add to the knob: +10 V adds 100 percentage points, −10 V subtracts 100, clipped to 0–100%. Probability is read when the action happens.

Each action has three trigger inputs at the top right: **All**, **Out**, **Ch**. All triggers the whole matrix when any channel goes high. Out uses trigger channels 1–16 for outputs A–P; Ch uses them for output channels 1–16. Mono Out or Ch controls the first row or column. Triggers rise at 2 V and reset below 0.2 V.

Choose a mode under **Randomization**:

- **New sources**: choose new input jack/channel pairs.
- **Shuffle existing sources**: rearrange routes, preserving source pairs, duplicates, and blanks.
- **Keep jacks, randomize channels**: keep each route’s input jack.
- **Keep channels, randomize jacks**: keep each route’s input channel number.

Wiggle moves the source jack/channel by at most one position, respecting the chosen mode. Blank routes stay blank. In Shuffle mode, wiggle swaps nearby selected destinations.

Options restrict choices to connected input channels, prevent repeated sources within the affected routes, limit changes to active output channels, or include blank. Without replacement, excess random destinations become blank when the source pool runs out. With no eligible sources, nothing changes unless Include blank is enabled. Source-pool, replacement, and blank options do not alter Shuffle existing sources.

## Scenes

Sixteen scenes store routing and output polyphony. **Edit** chooses the scene shown in the matrix. Edits are stored automatically. **Active** shows the scene feeding the outputs. Editing another scene leaves playback alone; edits to the active scene are heard immediately.

**Steps** sets the sequence length, from 1 to 16. Clock advances through scenes 1–Steps. Reset returns to scene 1 and takes priority over a simultaneous clock. Both inputs trigger at 2 V and reset below 0.2 V.

Patched **Scan** overrides Clock and Reset. 0–10 V selects across scenes 1–Steps in equal voltage bands; voltages outside that range are clipped. **EOC** emits a 10 V, 1 ms pulse when Clock wraps from the last step to scene 1. It stays silent while Scan is patched. Reset does not emit EOC.

Scene contents, the editing scene, and the active scene are saved with the patch. Randomize and wiggle affect the editing scene.
