## Blunch

Text-based polyphonic trigger sequencer.

### Run

The **Run** button is the master transport enable. When Run is off, Blunch is stopped and the Run input is ignored.

When Run is on, the **Run input** can control playback:

- With no cable connected, Blunch runs normally.
- In **Gate** mode, high voltage runs playback and low voltage stops playback. Gate is the default mode.
- In **Trigger toggle** mode, each rising edge toggles playback on or off.

Right click the module and use **Run input** to choose **Gate** or **Trigger toggle**.

The Run input supports polyphony. A monophonic cable controls all active channels. A polyphonic cable controls matching channels individually, so channel 1 controls sequencer channel 1, channel 2 controls sequencer channel 2, and so on. If the Run input has fewer polyphonic channels than Blunch is outputting, channels without a matching Run input channel stop while the cable is connected.

### Panel

Blunch defaults to 16 HP. Drag the left or right edge horizontally to resize the module; the pointer changes to a horizontal resize cursor over an edge. Width snaps to 1 HP steps, with a minimum of 16 HP, and is saved with the patch. Its editor occupies the area between the title and bottom controls. Time and Seed have knobs above their labels and tiny CV jacks below, beside Run in the input/control box. Line advance, Block advance, and Wait are selected in the context menu. External clocks W/X/Y/Z use tiny jacks in a 2×2 grid to the right of the Run/reset controls. Run and the manual Block, Line, and All reset buttons sit directly above their corresponding inputs. Manual Block, Line, and All reset buttons invoke the corresponding reset inputs without needing a cable; they follow the same Run/transport rules. The output box contains out, block, EOL, EOP, and the polyphony selector.

Phase, beside out, emits a 0–10 V clock-position ramp per active channel. Internal clocks use their exact phase; external clocks estimate phase from the last measured trigger interval, resetting on each rising edge and holding at 10 V if a clock stops. Two edges are needed to measure an external period.

Undo text edits with Cmd/Ctrl+Z; redo with Cmd/Ctrl+Shift+Z (or Cmd/Ctrl+Y). Edit history is retained separately for each channel and editor view, including across submit/view refreshes. Undo changes the editor text; submit the restored text to update the running sequence.

Use the title dropdown to select All (the channel overview) or an individual channel. Channels beyond the selected output polyphony are greyed but remain selectable for editing; selecting them does not increase the output channel count.

Time and Seed CV inputs are mono, using channel 1 and adding directly to their knobs. Time is 1 V/octave: +1 V doubles time speed, −1 V halves it; combined control is bounded to −5…+5 (1/32×…32×). Seed CV adds one seed unit per volt. A positive combined Seed is deterministic; zero or a negative combined Seed uses fresh random values. The Seed knob ranges from 0 to +10; negative CV can bring the combined Seed below zero. CV affects future random draws without changing stored knob values.
