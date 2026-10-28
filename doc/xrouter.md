# XRouter

16 polyphonic inputs and outputs. Each output channel takes one input channel, or blank (0 V). One input can feed several outputs. Initial routing is a to A, b to B, and so on, preserving channel numbers.

The display always shows the full matrix. Rows correspond to outputs A–P; columns are output channels 1–16. Each cell names its source: `a4` means input a, channel 4. `--` means blank. Unavailable sources, unpatched outputs, and destinations beyond the output’s polyphony appear faded.

The outside polyphony knob sets all outputs. `~` means their settings differ. Set individual outputs under **Output polyphony** in the context menu. Auto uses enough channels to include the highest destination with an available source. Missing input channels produce 0 V; mono inputs are not repeated across channels.

## Editing

Click a cell to choose a source from the larger 16×16 input grid. Hover highlights the choice. Click **X** to leave it blank. Escape or clicking outside cancels.

Right-click a cell to enter a source code such as `a5` or `i8`. Enter applies; **X** or `--` clears it. Invalid codes stay in the field until corrected.

The top **Grid / Knobs** button switches the display. In Knobs mode, drag a knob vertically to browse all 257 settings: blank, then a1 through p16. Mod slows dragging; Shift speeds it up. Clicking a knob does not open the source picker. Right-click still accepts text. Hover a knob or grid cell to see its route, such as `a4 -> N2`. Edits support undo/redo.

## Randomization

Hover over the module and press Mod+R (Cmd+R on macOS, Ctrl+R elsewhere), or choose **Randomize routes** in the context menu. It randomizes the whole matrix, including faded routes. Polyphony settings stay unchanged. Undo restores the routes.

Choose a mode under **Randomization**:

- **New sources**: choose new input jack/channel pairs.
- **Shuffle existing sources**: rearrange routes, preserving source pairs, duplicates, and blanks.
- **Keep jacks, randomize channels**: keep each route’s input jack.
- **Keep channels, randomize jacks**: keep each route’s input channel number.

Options restrict choices to connected input channels, prevent repeated sources, limit changes to active output channels, or include blank. Without replacement, excess destinations become blank when the source pool runs out. With no eligible sources, nothing changes unless Include blank is enabled. Source-pool, replacement, and blank options do not alter Shuffle existing sources.
