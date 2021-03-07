# Moly Patrix

Polyphonic matrix mixer

<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/moly-patrix.jpg" width="500" alt="Moly Patrix" />

## Knobs
- Global input scale: attenuverter for all input channels
- Global input offset: offset voltage for all input channels
- Input channel attenuverters: Left column, individual attenuverters for each input channel
- Polyphony: Set either "auto" (when turned all the way to the left, number of output channels will be equal to number of input channels) or set explicitly the output number of output channels in the polyphonic signal, from 1-16
- Grid: 256 attenuverters for controling how much of channel Y input gets to channel X output
- Output channel attenuverters: Right column, individual attenuverters for each output channel.  Note that the orientation of this is transposed.  Outputs in the grid are rows, but displayed here as columns.  Change the polyphony knob to see visually how the orientation is setup
- Global output offset: offset voltage for all output channels
- Global output scale: attenuverter for all output channels

## Inputs
- Polyphonic input: input signal
- Input trim CV: control the input scale.  When connected, the input scale knob acts as an attenuverter for this input.  Connect a polyphonic signal to control each input channel's scaling individually
- Output trim CV: control output scale.  Wen connected, the output scale knob acts as an attenuverter for this input.  Polyphonic signal can scale each output channel individually.

### Quirks
- Moly Patrix doesn't do any clipping of signals, so it is happy to output voltages of 100s of volts.  Use the global output or input attenuation knobs to keep everything under control
