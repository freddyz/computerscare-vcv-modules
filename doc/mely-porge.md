# Mely Porge

Polyphonic input processor with 16 insert channels

## Inputs/Outputs

- Main input: polyphonic input signal
- Main attenuverter: scales the main input
- Main offset: offsets the main input
- Polyphony: Set either "auto" (when turned all the way to the left, number of output channels will be equal to the greater of: number of input channels and the furthest patched or normalized insert input) or set explicitly the output number of channels in the polyphonic signal, from 1-16
- Main output: polyphonic output

## Channels

Each numbered channel has an insert input, attenuverter, mode button, and offset knob.

Click a mode button to set a channel mode:

- Add: sum the main input with the channel input
- Insert: pass the main input unless the channel input is patched, then replace it. The offset knob will always offset the final voltage, whether the insert it patched or not.
- Crossfade: crossfade between the main input and channel input using the attenuverter knob
- VCA bipolar: multiplication; main input is the signal, channel input is bipolar gain. Offset knob applies an offset to the gain.
- VCA unipolar: multiplication; main input is the signal, channel input is unipolar gain. Offset knob applies an offset to the gain.
- Minimum: output the lesser of the main input and the channel input. Offset knob applies to the channel input before comparison.
- Maximum: output the greater of the main input and the channel input. Offset knob applies to the channel input before comparison.
- Compare: output 10V when the channel input exceeds the main input, otherwise 0V. Offset knob applies to the channel input before comparison.

In Minimum, Maximum, and Compare modes, the channel attenuverter and offset shape the channel side of the comparison (`channel input × attenuverter + offset`); the offset is not re-applied to the output. With nothing patched into a channel, the offset knob alone sets a fixed clamp level (Minimum: ceiling, Maximum: floor) or comparator threshold. Compare always outputs a clean 0V/10V gate.

## Right-Click Options

- Set all to: set every channel mode at once
- Insert Normalization: choose whether insert inputs are independent, or normalized from a patched polyphonic insert input
- Main Input Polyphonic Wrap Mode: choose how main input channels are mapped when the output has more channels than the main input

## Insert Normalization

With `Normalize polyphonic` enabled, a polyphonic insert patched into one channel can fill following insert channels. A new patched insert starts a new run. The first column normalizes through channels 1-8, and can continue into the second column if the source has enough channels.

When `Multiple inserts break normalization` is off, a later shorter insert does not stop an earlier longer insert from continuing after the later insert ends. Turn it on to make a patched insert stop earlier normalization past that point.

### Quirks

- Mely Porge doesn't clip output voltages.
