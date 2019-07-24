# Computerscare Modules Polyphonic Utilities
<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/computerscare-poly-utilities.png" width="600" alt="Computerscare Polyphonic Utilities" />


## Knoly Pobs

16 knobs, polyphonic output, range: 0 - 10 volts.


## Boly Puttons

16 latch buttons.  On/off voltages selectable via right-click menu.  Plugging in a polyphonic signal to the A/B inputs will override the selected on/off values and use the CV signals instead.  If a monophonic signal is plugged into the "A" input, its voltage will be used for ALL "off/up" buttons.  If an n-channel signal is plugged into the "A" input, the "off/up" button voltages will match the "A" channels up to n, and then re-start at "A" channel 1.  Behavior is the same for the "B" input.  Note that this behavior is different than the recommended method of handling polyphonic signals.


## Roly Pouter

Re-route the channels of a polyphonic signal.  If you select an input channel that is higher than the highest available channel of the input signal, the output voltage will be 0.


## Toly Pools

* Display input channel count
* CV output of the input channel count (1 - 16 channels is linearly mapped to 0 - 10 volts)
* Knob and CV for setting output channel count (0 - 10 volts linearly mapped to 1-16 output channels)
* Knob and CV for rotating the polyphonic signal (0 - 10 volts sets rotation of 0-15 channels.  For example: rotation of "1" will move input channel 2 -> output channel 1, input channel 3->output channel 2, ...  input channel 16 -> output channel 1)


## Soly Pequencer

Sequentially output the individual channel voltages of a polyphonic signal.  Connecting a polyphonic signal to the clock input allows for multiple independent sequences.  Polyphonic reset channels beyond the number of channels of the clock signal are ignored.  I think that the clock/reset buttons will apply to all channels.
