# Phlooper

Phlooper plays up to sixteen independent audio loops. Small differences in loop length or speed let their repetitions drift apart. Each loop can store different mono or stereo audio.

## Quick start

1. Connect audio to **L / mono**, optionally adding **R** for stereo.
2. Press **Record** to begin; press again to finish. The first recording sets the shared duration, up to 60 seconds.
3. Choose the number of loops and adjust **Offset** to hear them drift.
4. Choose **Time ms**, **Length %**, **Speed %**, or **Beats** above the display.

A mono input feeds every loop. Polyphonic inputs feed matching loops; missing channels are silent. An unpatched R copies the corresponding L channel. The first recording fills all sixteen loops, including those outside the selected loop count.

## Recording and transport

- **Record:** overdub addressed loops after the first recording. **Rec mix** sets how much input replaces the stored audio. The context menu’s default **Blend** mode progressively replaces it; **Add** retains the old audio and adds input at the selected amount.
- **Erase:** silence the portion beneath moving heads while held. Erasing takes priority over recording.
- **Restart:** return addressed heads to their region’s starting point, or their captured position while held.
- **Stop:** freeze playback, recording, and erasing; release to resume from the same position.
- **Mute:** silence loops while heads, recording, and EOC continue. Dry input remains audible according to **Out mix**.

Panel buttons affect all active loops. Mono gate/trigger cables affect all loops; polyphonic cables address matching loops, with missing channels inactive. Hold is global and uses channel 1 only.

Each row has **M** (Mute), **S** (Solo), **P** (Pause), and direction controls. Multiple loops can be soloed; Mute takes priority. **Ctrl** hides these controls and gives the waveform the full display width.

Directions are **Forward**, **Reverse**, **Forward then Reverse**, and **Reverse then Forward**. Restart begins the first leg of the chosen direction. While held, it restores the position and direction captured when Hold was enabled. Changing direction modes keeps the current audio position. Ping-pong modes retain the current travel direction; Forward and Reverse use their chosen direction. The direction menu sets every loop together and shows **Mixed** when they differ. Recording and erasing follow playback direction.

**1-shot** restarts active loops when enabled. Forward or Reverse plays one traversal; alternating directions play both legs, then stop. Restart rearms a shot. Disabling 1-shot resumes looping.

## Regions and phasing

**Start** and **Length** select a region of the original recording. Regions can cross its end. Length applies immediately; ordinary Start changes take effect when each loop next wraps or restarts. Brackets show requested points immediately.

**Offset** adds an increment for each successive loop:

| Mode     | Effect                                                          |
| -------- | --------------------------------------------------------------- |
| Time ms  | Add milliseconds to the period without changing pitch           |
| Length % | Add a percentage of the selected length without changing pitch  |
| Speed %  | Change playback speed and pitch                                 |
| Beats    | Add fractions of the recorded clock beat without changing pitch |

Positive Time/Length/Beats offsets extend the period with silence; negative offsets shorten it and truncate audio. Speed-mode offsets range from 5% to 800% playback speed. Loop periods stay between 1 ms and 60 seconds. New modules start in Time mode with −5 ms per loop.

**Speed** multiplies all playback rates from ¼× to 4×, with 1× at the center, changing pitch in every mode.

**Ends** aligns region ends instead of starts. It keeps loop 1’s region and shifts other starts backward or forward to match that end. Per-channel Start CV still shifts its corresponding region. Ends changes the audio regions, not the phase relationship of playback.

### Record / erase clock

Patch **Clock** to quantize Record and Erase starts and stops. Both the panel controls and gate inputs wait for the next adjusted clock pulse. Without a clock cable, they respond immediately.

The boxy purple knob divides the clock to the left and multiplies it to the right: **÷16, ÷12, ÷8, ÷6, ÷4, ÷3, ÷2, ×1, ×2, ×3, ×4, ×6, ×8, ×12, ×16**. Multiplication learns the interval between two incoming pulses before adding subdivisions. Division begins on the first incoming pulse.

The Record light pulses yellow while Record or Erase waits to start and orange while waiting to stop. Erase pulses yellow while a change is queued. Short requests released before a clock pulse cancel themselves. Purple marks in the waveform show adjusted clock pulses captured during recording or erasing; they save with the patch.

### Beats offset

Choose **Beats** in the phase-mode menu to adjust each loop’s duration by a fraction of the recorded clock beat, keeping its pitch unchanged. One beat is one division between recorded clock marks: −1/12 shortens loop 2 by one-twelfth of that division, loop 3 by two-twelfths, and so on. Positive values lengthen successive loops; negative values shorten them. The clock divider/multiplier used while recording determines the captured beat; changing the live clock afterwards does not retime it. For audio recorded or imported without a clock, the full recording is one beat.

Offset selects signed fractions from **1/128 to 4 beats**, including **1/16, 1/12, 2/13, 2/11, 5/16, 7/22, 4/7, 5/7**, and zero. The Offset tooltip shows the chosen fraction. Length shows beats and mixed fractions, such as **2 5/16 beats**, in Beats mode. Offset CV selects the same steps: mono adds a shared offset, poly addresses each loop. Recorded beat timing saves with the patch.

In Beats mode, the main **Length** snaps to multiples of the selected fraction. **Start always moves freely**, so you can position the loop precisely. The sign affects loop phasing; the grid uses the fraction’s magnitude. At zero Offset, Length also moves freely. Dragged start/end handles snap to the same fraction grid, anchored at the freely positioned Start knob. At zero Offset, handle dragging is free too.

### Hold

**Hold** stops the drift. In Time/Length/Beats modes it preserves the current spacing between heads and matches their periods to loop 1. In Speed mode it preserves pitches and fits temporary regions so every loop repeats in exactly the same time. The shortest current repeat time becomes the shared period.

Releasing Hold restores the selected regions. Start, Length, and Offset changes wait until release in Speed Hold; overall Speed continues to scale the held loops together. Smaller yellow brackets show the original regions while white brackets show the held regions.

### Waveform and loop points

The display shows stored audio, playheads, loop brackets, and the original recording’s boundaries. Audio repeats beyond those boundaries so extended loop points stay on a continuous timeline.

Drag a bracket to edit a loop’s start or end while keeping its opposite point fixed. Edited points turn yellow and take effect immediately. Right-click a bracket to restore that point to its knob-defined position. Knobs and mono CV continue adjusting custom regions relative to their normal layout.

Polyphonic Start, Length, or Offset CV temporarily takes over the regions and locks all handles. Unpatching restores custom edits. Hold also locks editing. **Reset custom loop points** in the context menu clears all edits; importing or clearing audio does the same. Custom points support Undo/Redo and save with the patch.

**Zoom** fits the selected regions with padding; press again for the full recording. Zoom waits until a knob or bracket drag ends before refitting. The waveform is an overview, so very short transients in long recordings may not appear.

## Inputs

Inputs support up to sixteen channels unless marked mono. For per-loop controls, a mono cable broadcasts to all loops and polyphonic channels address corresponding loops.

| Input       | Range and behavior                                                                                                                                                                                                                                   |
| ----------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| L / mono, R | Audio, scaled by input Gain and VCA. ±5 V is the nominal reference. Missing poly channels are silent; unpatched R copies L                                                                                                                           |
| Record      | Gate high at ≥1 V enables recording; the panel latch also enables it                                                                                                                                                                                 |
| Erase       | Gate high at ≥1 V erases under addressed heads                                                                                                                                                                                                       |
| Restart     | Rising edge at ≥1 V, rearmed at ≤0 V. A cable initially high must go low then high                                                                                                                                                                   |
| Stop        | Gate high at ≥1 V freezes addressed heads; combines with panel Stop and row Pause                                                                                                                                                                    |
| Mute        | Gate high at ≥1 V silences addressed loops; combines with panel/row Mute and Solo                                                                                                                                                                    |
| Hold        | Mono gate high at ≥1 V enables global Hold; combines with the panel latch                                                                                                                                                                            |
| Start CV    | Adds 10 percentage points per volt, clamped to 0–100%. Missing poly channels use the knob                                                                                                                                                            |
| Length CV   | Adds 10 percentage points per volt, clamped to 0.1–100%. Missing poly channels use the knob                                                                                                                                                          |
| Offset CV   | ±10 V maps to ±100 ms in Time mode or ±20 percentage points in Length/Speed mode, or 40 signed fraction steps in Beats mode. Mono adds to every loop’s knob spacing; poly replaces spacing for supplied channels. Missing channels keep knob spacing |
| Speed CV    | Mono, 1 V/octave; +1 V doubles speed, −1 V halves it. Combined Speed knob/CV is limited to ¼–4× before Speed-mode Offset                                                                                                                             |
| Rec mix CV  | Mono; adds 10 percentage points per volt, clamped to 0–100%                                                                                                                                                                                          |
| Out mix CV  | Mono; adds 10 percentage points per volt, clamped to 0–100%                                                                                                                                                                                          |
| VCA         | 0–10 V controls 0–100% input level, multiplied by input Gain. Negative CV gives silence; above 10 V gives full level. Mono controls all loops; missing poly channels give silence. Unpatched passes full level                                       |

Unpatched gates are inactive. Gate inputs go low below 1 V. Unpatched CV adds zero, except VCA’s full-level normalization. VCA affects recording and dry monitoring. Time/Length/Beats Hold uses loop 1’s Length CV; Speed Hold captures its regions until released.

## Outputs and levels

**Out mix** blends dry input with the average loop signal. The context menu’s **Output mode** defaults to **Mix**, giving stereo mixdown. **Polyphonic** sends one loop per channel on both outputs, using the selected loop count.

Input and output **Gain** range from silence to +18 dB, with 0 dB at unity. Input Gain affects recording and dry monitoring; output Gain affects mixed and polyphonic audio. Overdubs are bounded to ±20 V.

- **EOC:** one 10 V, 1 ms trigger per traversal boundary, with a channel per active loop. Muted loops still trigger; stopped loops do not. Restart does not produce EOC.
- **Phase:** one 0–10 V channel per active loop, from region beginning to end. Forward ramps upward; Reverse downward. Pause holds the voltage; completed one-shots keep their endpoint. Empty buffers and initial recording output 0 V.

Gain and audio output mode do not affect Phase or EOC. Meters follow the loudest channel on each side for polyphonic audio; Mix mode meters the actual stereo mix. Peak lines hold briefly above the moving level.

## WAV files and patches

Use **Import WAV to all loops** or drop a WAV onto the module. Both copy the file into all sixteen loops and restart them. Mono/stereo PCM and floating-point WAV are supported; multichannel files use their first two channels. Files longer than 60 seconds are truncated. A WAV amplitude of 1 corresponds to 5 V.

**Export WAV** saves one loop or all sixteen as separate files, including inactive loops. Exports contain the full stored audio as 48 kHz float WAV, preserving mono/stereo, without playback gain, speed, direction, or region cropping. Stop recording before saving or exporting for a consistent snapshot.

Patches save all audio, loop settings, playhead positions, custom regions, and held regions. Record always loads off. **Clear recording** empties all loops and resets Start to 0%, Length to 100%, custom points, and held regions; patched CV still applies.

Randomize preserves loop count, 1-shot, Ctrl, Zoom, Out mix, and custom loop points.

## Keyboard shortcuts

With the pointer over Phlooper: **Space** toggles Stop, **M** toggles Mute, **H** toggles Hold, **R** toggles Record, **T** restarts, **E** erases while held, **Z** toggles Zoom, and **C** toggles row controls.

Hover tooltips show shortcuts in brackets. Hold backtick/tilde while hovering over or interacting with Phlooper to show them beside their controls.
