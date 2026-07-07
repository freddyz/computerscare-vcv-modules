## Debug

Polyphonic voltmeter, sample & hold, and random voltage source.

<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/images/computerscare-debug-basic.png" width="300" alt="Computerscare Debug" />

### Inputs

**Clock (trg):** Clock or trigger input. In Single-Channel clock mode, the selected clock input channel triggers updates. In Polyphonic clock mode, each clock input channel triggers the matching display/output channel.

**Value (in):** Signal to monitor or sample. In Single-Channel input mode, the selected value input channel is used. In Polyphonic input mode, each value input channel feeds the matching display/output channel.

**Reset (clr):** Clears all displayed and output values to 0v.

### Output

**Main:** Polyphonic output of the displayed values. The output channel count follows the active clock/input mode combination.

### Buttons

**Trigger (trg):** Manual clock trigger. It acts like a clock event for the active clock mode.

**Clear (clr):** Manual reset. Clears all displayed and output values to 0v.

### Right-Click Options

**Show Clock Blinkers:** Shows or hides clock activity indicators behind the display lines.

**Clock Mode:**

- **Single-Channel:** Use one selected clock input channel.
- **Internal:** Update continuously at audio rate without a clock input.
- **Polyphonic:** Use each matching clock input channel.

**Input Mode:**

- **Single-Channel:** Read one selected value input channel.
- **Internal:** Generate random values.
- **Polyphonic:** Read each matching value input channel.

**Random Generator Range:** Sets the voltage range for Internal input mode and module randomization. Ranges include 0v to +10v, -5v to +5v, 0v to +5v, 0v to +1v, -1v to +1v, -10v to +10v, -2v to +2v, and 0v to +2v.

**Visual:** Sets bar mode, text opacity, and bars opacity.

**Text:** Sets whether text displays voltages, notes with sharps/flats, or MIDI note numbers with cents offsets.

### Modes of Operation

There are 3 clock modes and 3 input modes. The clock mode decides when values are updated. The input mode decides where the new values come from. The small channel selector knobs choose the active channel for Single-Channel clock and input modes.

**Single-Channel Clock, Single-Channel Input:**
Each trigger on the selected clock channel samples the selected value channel. The new sample is inserted at the top of the display, and the existing values shift down one line.

**Single-Channel Clock, Internal Input:**
Each trigger on the selected clock channel fills all 16 display/output lines with new random values from the selected random generator range.

**Single-Channel Clock, Polyphonic Input:**
Each trigger on the selected clock channel samples all value input channels into their matching display/output lines.

**Internal Clock, Single-Channel Input:**
Continuously updates the selected display/output line from the selected value input channel. Other lines keep their previous values.

**Internal Clock, Internal Input:**
Continuously generates 16 channels of random voltage from the selected random generator range.

**Internal Clock, Polyphonic Input:**
Continuously monitors all value input channels as a polyphonic voltmeter.

**Polyphonic Clock, Single-Channel Input:**
Each clock input channel updates its matching display/output line with the selected value input channel.

**Polyphonic Clock, Internal Input:**
Each clock input channel updates its matching display/output line with a new random value from the selected random generator range.

**Polyphonic Clock, Polyphonic Input:**
Each clock input channel samples its matching value input channel, making a polyphonic sample & hold.

### Randomize

Rack's module Randomize command sets all 16 displayed/output values to random voltages using the selected Random Generator Range. It does not randomize the clock mode, input mode, clock blinkers, or channel selector knobs.

_Inspired by ML Modules Volt Meter_
