# Windchimes

Windchimes places up to sixteen wind-driven chime sets around a listener. Each set has its own tuning, material, sound, and position. Connect one audio output for mono, a front or rear pair for stereo, or all four for quad.

## Quick start

1. Connect **Front L** and **Front R** to your audio output.
2. Raise **Strength** to move the chimes, or press **Strike** to push the selected set.
3. Click a set in the scene and adjust its tuning and sound.
4. Use **+ Add** to add another set. Drag it around the listener to place it.
5. Turn Wind **Mix** down for chimes without audible wind.

## Scene and selection

The listener is at the center. Top is front; bottom is rear. Drag a set outward to make it quieter, darker, and more distant. Distance also adds up to 80 ms of sound-travel delay. Click **2D/3D** to change the view.

Click a set to select it. Shift-click adds or removes sets from the selection; Ctrl/Cmd+A selects all enabled sets while the scene is focused. The knobs and buttons edit the selected sets together. Mixed menu values display **Mixed**.

- **+ Add:** add a set, up to sixteen. Each set has 1–12 tubes.
- **Remove**, **Delete**, or **Backspace:** remove the selected set.
- **Strike**, or double-click a set: push its striker. Sound starts when it contacts a tube.
- **Solo / Mute:** isolate or silence selected sets. Motion continues, and existing effect tails decay. Mute takes priority over Solo.
- **Random:** randomize all Chime Model knobs, including Tubes, Level, Swing, and Sail. Material stays unchanged; Level stays between 30% and 100%.
- **Wiggle:** nudge every Chime Model knob by up to 5% of its range; Tubes changes by one. Material stays unchanged and Level stays between 30% and 100%.

A set’s right-click menu also offers **Stop**, **Duplicate**, and **Divide**. Stop pauses its motion and fades its sound; Strike resumes it. Duplicate copies its settings nearby. Divide duplicates it with a gentle sound variation. Stop is temporary; sound settings and Solo/Mute are saved with the patch. Panel edits, moves, and sound changes support Undo/Redo.

## Scene randomization

The top **Randomize all** button replaces the scene with 2–8 chime sets in random
positions. It randomizes their tuning, tube counts, level, and motion settings.
Each set gets a preset, a gently wiggled preset, or a completely random sound.
New sets start unmuted and unsoloed. Wind, effects, and output Gain stay unchanged.
One Undo restores the previous scene settings.

Top **Random** and **Wiggle** change the selected chimes’ sounds without replacing
the scene, moving sets, or changing tuning. With no selection they affect all
enabled chimes. **Shift-click** affects all chimes regardless of selection;
right-click either button to choose **Selected chimes** or **All chimes**.

## Tuning

| Control  | Use                                                                       |
| -------- | ------------------------------------------------------------------------- |
| Scale    | Choose the notes used by the tubes                                        |
| Root     | Set the root in semitones, C through B                                    |
| Register | Shift by −2 to +2 octaves                                                 |
| Fine     | Tune by −100 to +100 cents                                                |
| EDO      | Divide the octave into 5–24 equal steps; scale notes use the nearest step |
| Spread   | Step through the scale: 1 uses consecutive notes, 2 skips one, up to 4    |

**V/oct** adds transposition, limited to ±5 V. Mono affects all chimes; polyphonic channels transpose matching chime sets. Missing channels add zero.

Root, Register, Fine, EDO, and Spread have additive CV. Each +10 V spans the control’s full range: 11 semitones, 4 octaves, 200 cents, 19 divisions, or 3 scale degrees respectively. Negative CV subtracts. Results clamp to the control’s limits; discrete controls round to whole steps. Mono affects every set; poly channels address matching sets, with missing channels adding zero. Fine CV changes tuning continuously; it is not an audio-rate FM input.

## Chime model

The material menu selects **Metal**, **Wood**, or **Plastic**. The preset menu offers sounds for that material; changing a sound control displays **Custom**. Presets leave tuning, position, level, and motion settings unchanged.

| Control       | Use                                                                       |
| ------------- | ------------------------------------------------------------------------- |
| Tubes         | Number of tubes, 1–12                                                     |
| Level         | Volume of the selected sets                                               |
| Swing         | Motion persistence; higher settings let the striker and sail swing longer |
| Sail          | Wind-catching area; larger sails respond more strongly                    |
| Decay         | Ringing time                                                              |
| Bright        | Brightness of the ringing sound                                           |
| Hardness      | Soft or firm striker attack                                               |
| Weight        | Striker mass and inertia                                                  |
| Shape         | Material-dependent bar, block, shell, or tube character                   |
| Body          | Resonance and muting                                                      |
| Inharmonicity | Spacing of overtones while retaining the tuned fundamental                |

Each control has CV beside its knob. CV adds 10 percentage points per volt, clamped to 0–100%. Tubes instead adds 1.1 tubes per volt, rounded and clamped to 1–12. Mono CV offsets all sets; polyphonic channels address matching sets. Missing channels add zero. CV does not overwrite stored knob or preset values.

**Strike**, **Wiggle**, and **Random** accept rising-edge triggers at about 1 V, rearmed at 0 V. A held high signal triggers once. Mono triggers act on every enabled set; poly triggers address individual sets, independently of the current selection. CV-triggered sound changes alter the stored sound settings.

**Solo** and **Mute** accept gates: high at 1 V or more, low below 1 V. While patched, the gates replace the manual button states. Mono controls all sets; poly controls corresponding sets, with missing channels low. Unpatching restores the manual states.

Rack’s Randomize action changes the selected sets’ sound controls and preserves tuning, material, and position. Level stays between 30% and 100%.

## Wind

Wind is shared across the scene. Each chime responds according to its position, motion, and sail.

- **Strength:** steady wind level. Light breezes produce rare chimes; stronger wind increases activity and brings out buffeting and brighter air.
- **Gusts:** variation in wind strength.
- **Turb:** uneven movement and changing wind direction.
- **Mix:** audible wind level and presence in the main outputs.
- **Tone:** emphasizes low buffeting at the bottom; adds more frequent bright swells at the top while keeping the low body.
- **Texture:** adds overlapping air swells and soft whistles, with slightly longer tails.
- **Gust:** briefly boost the current wind. Gusts scale with Strength; at zero there is no push. The jack accepts a mono rising-edge trigger at about 1 V.

All six knobs have additive mono CV: +10 V adds the full control range, negative CV subtracts, and results clamp to 0–100%. Only the first channel of a polyphonic cable is used.

## Reverb and delay

Reverb **Mix** blends the chimes and audible wind with a shared room. **Size** changes its dimensions and tail length.

Delay **Mix**, **Time**, and **Feedback** affect the chimes and their reverb. Audible wind bypasses delay. Time ranges from 10 ms to 2 seconds; Feedback reaches 95%.

Patch **Clock** to select divisions or multiples of its period: 1/8, 1/6, 1/4, 1/3, 1/2, 3/4, 1, 3/2, 2, or 4. The delay remains capped at 2 seconds. Two rising edges establish tempo; stopping the clock holds it. Unpatch to return to seconds. Clock uses channel 1 and triggers around 1 V. Changing the connected output arrangement clears old echoes.

All reverb and delay knobs have additive mono CV, using the same ±10 V full-range mapping as Wind. Time CV follows the knob’s free-time or clock-division range.

## Outputs

- Wind **CV:** 0–10 V wind strength.
- Wind **Audio:** dry mono wind, independent of Wind Mix. Tone, Texture, weather, and Gain still apply.
- **Front L / Front R / Rear L / Rear R:** spatial audio. A front or rear pair carries the whole scene in stereo; any single jack carries mono. Other combinations redistribute the scene across the connected speakers.
- **Gain:** final audio level, including the dedicated Wind Audio output. It has no CV input.

The context option **Exclude wind from quad** removes wind from the main audio outputs and their reverb, leaving it available at Wind Audio. This setting is saved with the patch.

## Keyboard shortcuts

With the pointer over the scene, **M** toggles Mute, **S** toggles Solo, **W** wiggles, **R** randomizes, and **X** strikes the selection. **Space** makes a gust. Each acts once per press.

**Tab / Shift+Tab** selects the next/previous enabled set while hovering over the module. Navigation wraps and replaces the current selection.
