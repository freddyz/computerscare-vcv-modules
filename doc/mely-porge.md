# Mely Porge

Polyphonic input processor with 16 insert channels

## Main

- Main input: polyphonic input signal
- Main attenuverter: scales the main input
- Main offset: offsets the main input
- Polyphony: Set either "auto" (when turned all the way to the left, number of output channels will be equal to number of input channels) or set explicitly the output number of channels in the polyphonic signal, from 1-16
- Main output: processed polyphonic output

## Channels

Each numbered channel has an insert input, attenuverter, mode button, and offset knob. The output channel uses the matching numbered channel's controls. Channels repeat every 16 output channels.

Click a mode button to set a channel mode:

- Add: sum the main input with the channel input
- Insert: pass the main input unless the channel input is patched, then replace it
- Crossfade: crossfade between the main input and channel input using the attenuverter knob
- VCA bipolar: multiplication; main input is the signal, channel input is bipolar gain
- VCA unipolar: multiplication; main input is the signal, channel input is unipolar gain

## Right-Click Options

- Set all to: set every channel mode at once
- Insert Normalization: choose whether insert inputs are independent, or normalized from a patched polyphonic insert input
- Main Input Polyphonic Wrap Mode: choose how main input channels are mapped when the output has more channels than the main input

## Insert Normalization

With Normalize polyphonic enabled, a polyphonic insert patched into one channel can fill following insert channels. A new patched insert starts a new run. The first column normalizes through channels 1-8, and can continue into the second column if the source has enough channels.

When Multiple inserts break normalization is off, a later shorter insert does not stop an earlier longer insert from continuing after the later insert ends. Turn it on to make a patched insert stop earlier normalization past that point.

### Quirks

- Mely Porge doesn't clip output voltages.
- In Insert mode, the global offset still applies when an insert is active.
