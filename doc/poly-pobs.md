# Poly Pobs

16 editable polyphonic CV bands. Each band output has up to 16 channels.

## Outputs

- Alpha-Papa band outputs: 16 polyphonic outputs
- Poly Channels: sets the number of channels sent by every band output, from 1-16

Each output channel value is set by the main knobs, then shaped by both the band and channel scale/offset settings:

`output voltage = value × band scale × channel scale + band offset + channel offset`

## Focus

Click a band letter to focus a band. The 16 main knobs edit channels 1-16 for that band, and the scale/offset knobs edit that band.

Click a channel number to focus a channel. The 16 main knobs edit Alpha-Papa for that channel, and the scale/offset knobs edit that channel.

Inactive channel controls are dimmed when they are above the selected Poly Channels count.

## Randomize

- RAND ALL: randomizes all band/channel values
- INIT ALL: initializes all band/channel values and all band/channel scale/offset values
- Channel randomize input: each polyphonic input channel randomizes the matching channel across all bands
- Output randomize input: each polyphonic input channel randomizes the matching band across all channels
- Randomize all input: any high input channel randomizes all band/channel values

## Right-Click Options

- Main Knob Range: choose Unipolar (0V to 10V) or Bipolar (-10V to 10V)
- Randomization > Randomize Chance: chance that each individual band/channel value changes
- Randomization > Mode: Replace chooses a new value; Wiggle moves the current value
- Randomization > Randomize Minimum/Maximum: clamps replacement values and final wiggle results
- Randomization > Wiggle Amount Min/Max: voltage range added to the current value in Wiggle mode

INIT ALL and randomize actions do not reset the randomization settings.
