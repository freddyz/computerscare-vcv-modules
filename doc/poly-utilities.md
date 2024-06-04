# Computerscare Modules Polyphonic Utilities
<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/computerscare-poly-utilities.png" width="600" alt="Computerscare Polyphonic Utilities" />

## Knoly Pobs

16 knobs, polyphonic output, Default range: 0 - 10 volts.  Global scale & offset knobs to adjust all output channel values simultaniously.


## Boly Puttons

16 latch buttons.  On/off voltages selectable via right-click menu.  Plugging in a polyphonic signal to the A/B inputs will override the selected on/off values and use the CV signals instead.  If a monophonic signal is plugged into the "A" input, its voltage will be used for ALL "off/up" buttons.  Number of output channels will be the maximum of: number of channels from "A" input, number of channels from "B", input, or the manual poly channels knob setting


## Roly Pouter

Re-route the channels of a polyphonic signal.  If you select an input channel that is higher than the highest available channel of the input signal, the output voltage will be 0.


## Soly Pequencer

Sequentially output the individual channel voltages of a polyphonic signal.  Connecting a polyphonic signal to the clock input allows for multiple independent sequences.  Polyphonic reset channels beyond the number of channels of the clock signal are ignored.  The clock/reset buttons apply to all channels.


## Toly Pools

* Display input channel count
* CV output of the input channel count (1 - 16 channels is linearly mapped to 0 - 10 volts)
* Knob and CV for setting output channel count (0 - 10 volts linearly mapped to 1-16 output channels).  CV value is multiplied by 1.6 and added to knob value
* Knob and CV for rotating the polyphonic signal (-10v thru +10v sets rotation of -16 to +16 channels.  For example: rotation of "1" will move input channel 2 -> output channel 1, input channel 3->output channel 2, ...  input channel 16 -> output channel 1).  CV value is multiplied by 1.6 and added to knob value.
* The default output polyphony setting is "A": Automatic which will set the output polyphony equal to the input polyphony
* Different rotation modes via context menu:
- "Repeat Input Channels": If the output polyphony is set higher than input polyphony (channel count), the input channels will be repeated to fill the output
- "Rotate Through Maximum of Output, Input Channels": If the output polyphony is higher than the input polyphony, all input channels will be used and the remainder will be filled with 0v
- "Rotate Through 16 Channels (Legacy)": The input signal will be padded to 16 channels polyphony with 0v always.  This is the "old" Toly Pools behavior, and results in a lot of 0v signals.


### Poly Channels Knob
Most Poly Utilities have the small poly channels knob which sets how many poly channels the output signal has.  Puttons, Pouter, and Pequencer all support "automatic" mode for this knob when turned all the way to left.  When in this position, the poly output channels will be set to equal the poly input channels.