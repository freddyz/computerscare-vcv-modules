
## Debug
Polyphonic volt meter, sample sample & hold, and noise source

<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/computerscare-debug-basic.png" width="300" alt="Comptuerscare Debug" />

### Inputs:
**Trigger (trg):** When a trigger signal is detected, the voltage at the Input will be placed at the top of the list of voltages, and the voltage at the bottom will be discarded from the display.

**Input (in):** Any sort of signal you wish to probe.

**Clear:** Reset the list of voltages back to all zeros.

### Buttons:
**Trigger (trg):** Manual Trigger

**Clear (clr):** Manual Clear

### Modes of Operation

There are 3 clock modes (single, internal, poly), and 3 input modes (single, internal, poly).  Single uses a single channel.  Poly uses the poly channels.  Internal clock mode operates Debug at audio rate, and Internal input mode uses a uniform random generator with the range selectable via the right-click menu.  The default range for Internal input mode is 0-10v.


**Single Clock, Single Input:**
Selected channel clock signal will place the current value of selected input channel signal at line 1 and will discard the last line in the display.

**Single Clock, Internal Input:**
Internal random generator will replace each of the 16 lines of display

**Single Clock, Poly Input:**
Selected channel clock signal will place current value of each input channel in the corresponding line of the display.  (channel 4 of input gets recorded on line 4 of the display)

**Internal Clock, Single Input:**
"Realtime" updates whichever input channel is selected.  All other output channels remain at their existing values.

**Internal Clock, Internal Input:**
16 channel independent noise source.

**Internal Clock, Poly Input:**
"Realtime" 16-channel poly volt meter

**Poly Clock, Single Input:**
Each clock signal from poly clock input will update the corresponding display/output line with the current value of the selected input channel

**Poly Clock, Internal Input:**
Each clock signal from poly clock will update corresponding display line (a.k.a. output channel) with a random value from the internal generator.

**Poly Clock, Poly Input:**
16-channel sample-and-hold, clocked by poly clock


*Inspired by ML Modules Volt Meter*


~~~~
⼛ೊ ⼛蠍ೊ ೊ  ⼛ೊ蠍ʬ     ⼛
    ʬ    ʬ  蠍⼛     蠍ೊ蠍ʬ蠍⼛ 
 蠍 ⼛ೊ⼛   蠍蠍ೊʬ     蠍  ⼛⼛

⼛ೊ ⼛蠍ೊ ೊ  ⼛ೊ蠍ʬ     ⼛
    ʬ    ʬ  蠍⼛     蠍ೊ蠍ʬ蠍⼛ 
 蠍 ⼛ೊ⼛   蠍蠍ೊʬ     蠍  ⼛⼛

⼛ೊ ⼛蠍ೊ ೊ  ⼛ೊ蠍ʬ     ⼛
    ʬ    ʬ  蠍⼛     蠍ೊ蠍ʬ蠍⼛ 
 蠍 ⼛ೊ⼛   蠍蠍ೊʬ     蠍  ⼛⼛
~~~~
