# Toly Pensor

A mix matrix with sixteen polyphonic inputs **a–p** and sixteen polyphonic outputs **A–P**. Every input channel can mix into every output channel: 256 sources and 256 destinations.

New modules connect matching ports and channels at unity gain: a1 to A1, a2 to A2, and so on. Other connections start at zero.

## Editing connections

The letter and number buttons beside the jacks choose an **input port**, **input channel**, **output port**, and **output channel**. Inputs run down the left: jack, lowercase port letter, channel number. Outputs run down the right: channel number, uppercase port letter, jack.

Each selector column has an **All** button. With no All selected, you edit one connection. One All shows sixteen knobs; two show a 16 × 16 grid. The selected dimensions become the grid axes, in order: input port, input channel, output port, output channel. Three or four All selections show a message until you choose a smaller slice. **None** hides the controls while routing continues.

For input **a / All** and output **A / All**, the grid mixes input channels into output channels. For input **All / 4** and output **All / 7**, it mixes input ports into output ports, connecting channel 4 to channel 7.

Connection gain ranges from **−2 to +2**. Drag vertically; Ctrl/Cmd-drag makes finer changes. Hover to see the complete route and value. Double-click restores that connection’s initial value. Right-click to enter an exact value or choose zero, unity, or inverted unity. All edits support Undo/Redo, even after changing views.

Faded controls indicate a missing input channel, an unpatched output, or an output channel outside the selected polyphony. They remain editable. With nothing patched, all controls appear active.

## Scale and offset

The pairs on the left adjust individually selected input ports, input channels, output ports, and output channels. A pair is hidden when its selector is All; its saved settings still apply. Scale ranges from **−4 to +4**; offset ranges from **−10 to +10 V**.

Input adjustments apply before mixing; output adjustments apply afterward. On each side, port and channel scales multiply together, and their offsets add independently. A channel setting applies to that channel number across all ports; a port setting applies to all channels in that port.

Unpatched inputs and absent input channels contribute nothing, including their input offsets. Output offsets can generate a voltage even with no input signal.

## Output polyphony

**Poly** sets the same channel count for every output. **Auto** uses the largest connected input’s channel count, or sixteen when no inputs are connected. Unused outputs do not process audio. Input signals are not automatically repeated into missing polyphonic channels.

## Randomize, Wiggle, and Initialize

Five groups act on the selected **input port**, **input channel**, **output port**, **output channel**, or **Everything**.

- **Rnd:** Replace connection gains with random values within ±Rnd range, up to ±2. Rnd % controls the probability for each connection.
- **Wig:** Add a small random change within ±Wig range, keeping gains within ±2. Wig % controls the probability for each connection.
- **Init:** Restore matching routes to unity and all other routes to zero within the chosen scope. Also reset that scope’s scale and offset. Everything resets all four sets of scale/offset values.

When a scope is set to All, its buttons act on every connection. Init resets only that scope’s scale/offset pairs; Everything resets all pairs.

Randomize and Wiggle change connection gains, leaving scale and offset alone. Buttons support Undo/Redo. Trigger inputs perform the same actions without adding undo entries.

Each action has its own trigger input. A rising edge at 2 V triggers the action; 0 V rearms it. Mono acts on the index currently selected for that scope. Poly channels address indices 1–16 independently. Everything uses channel 1 and always affects the entire matrix.

Routing and scale/offset settings save with the patch. Changing views does not change the mix. There is no automatic normalization: summing many signals can produce high voltages.
