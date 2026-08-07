# VolyPector

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

- RAND: replace-randomizes all band/channel values
- WIG: wiggles all band/channel values from their current positions
- INIT: initializes all band/channel values and all band/channel scale/offset values
- Randomize Output Channel input: each polyphonic input channel randomizes the matching channel across all bands
- Randomize Output Band input: each polyphonic input channel randomizes the matching band across all channels
- Randomize all input: any high input channel randomizes all band/channel values
- Wiggle Output Channel input: each polyphonic input channel wiggles the matching channel across all bands
- Wiggle Output Band input: each polyphonic input channel wiggles the matching band across all channels
- Wiggle all input: any high input channel wiggles all band/channel values
- Initialize Output Channel input: each polyphonic input channel initializes the matching channel across all bands
- Initialize Output Band input: each polyphonic input channel initializes the matching band across all channels
- Initialize all input: any high input channel initializes all band/channel values and all band/channel scale/offset values
- Randomize Probability and Range Scale affect RAND and randomize trigger inputs
- Wiggle Probability and Range Scale affect WIG and wiggle trigger inputs

CV adds `voltage / 10` to each Probability or Range Scale knob.

Range Scale narrows RAND's full knob range. Wiggle Range Scale narrows WIG's +/-2V movement.

Rack initialize resets the visible Probability and Range Scale knobs to 100%.

## Right-Click Options

- Knob Range: choose Unipolar (0V to 10V) or Bipolar (-10V to 10V)
