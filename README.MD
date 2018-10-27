# computerscare modules for VCV Rack.


 ┭ ۳┭┭  ۳۳┭┭ ┭  ┭  ┭┭┭  ┭۳┭۳۳┭
۳  ۳┭ ۳۳۳┭۳  ┭  ┭  ┭ ┭۳  ┭   
┭┭   ┭۳ ┭ ┭ ┭ ┭┭   ۳۳۳┭۳ ۳┭┭  ۳ ┭۳ ۳

 ┭ ۳┭┭  ۳۳┭┭ ┭  ┭  ┭┭┭  ┭۳┭۳۳┭
۳  ۳┭ ۳۳۳┭۳  ┭  ┭  ┭ ┭۳  ┭   
┭┭   ┭۳ ┭ ┭ ┭ ┭┭   ۳۳۳┭۳ ۳┭┭  ۳ ┭۳ ۳

 ┭ ۳┭┭  ۳۳┭┭ ┭  ┭  ┭┭┭  ┭۳┭۳۳┭
۳  ۳┭ ۳۳۳┭۳  ┭  ┭  ┭ ┭۳  ┭   
┭┭   ┭۳ ┭ ┭ ┭ ┭┭   ۳۳۳┭۳ ۳┭┭  ۳ ┭۳ ۳



# Laundry Soup
Laundry Soup is a trigger sequencer that takes text as input.  If you connect a clock signal to the "clk" input, and type "1" in the top text box, the top "out" output will send a trigger each clock.  "2" sends a trigger every 2 clock signals.  "31" sends a trigger on the 1st and 4th clock signals, and then repeats this pattern forever or until reset.

![ComputerscarePatchSequencer](./doc/laundry-soup-basic-2.png)


### Text Input:
Lets represent a clock signal like this:

`xxxxxxxx`

If the value

`2`

is input into a text field, the output signal will be:

`x-x-x-x-`

You can see this illustrated on the scope in the screenshot above.




The input and output of the 2nd row is:

~~~~
31 ---> x--x  (repeating after 4 beats)
~~~~


In The 3rd row:`2131@8`, the "@8" means: repeat the pattern every 8 clock signals.  In this case the original pattern has length 7 (2 + 1 + 3 + 1 = 7), so normally it would repeat every 7 clock signals.

~~~~
2131   ---> x-xx--x  (repeating after 7 beats)
2131@8 ---> x-xx--xx (repeating after 8 beats)
~~~~
           


The 4th row's input is `8-4`.  This means the pattern "8", offset by 4 triggers.  When triggered by 16th notes, I find this pattern useful for triggering a backbeat snare drum.

~~~~
8    ---> x-------
8-1  ---> -x------
8-4  ---> ----x---
~~~~
 

The 5th row shows how multiple patterns can be combined.  `311@16,2@16` means: `311` for 16 beats, and then `2` for 16 beats.
~~~~
311@16      ---> x--xxx--xxx--xxx
  2@16      --->                 x-x-x-x-x-x-x-x-
   
311@16,2@16 ---> x--xxx--xxx--xxxx-x-x-x-x-x-x-x-
~~~~


The 6th row shows a way to "skip" beats.  `@4` all by itself means: wait 4 beats with no trigger.  This is another way to generate the backbeat pattern as the 4th row example.
~~~~
@4,4    ---> ----x---
~~~~

 
 


### Input Jacks:

![ComputerscarePatchSequencer](./doc/laundry-soup-inputs-1.png)

**Global Clock (clk):** Increases each row's absoluteStep by 1.

**Global Reset (rst):** Resets each row's absoluteStep to 0.

**Individual Clock (clk):** Increases the row's absoluteStep by 1.  If a cable is plugged in, the row ignores the Global Clock.

**Individual Reset (rst):** Resets the row's absoluteStep to 0.  If a cable is plugged in, the row ignores the Global Reset.


### Output Jacks:

![ComputerscarePatchSequencer](./doc/laundry-soup-outputs-1.png)

**Individual Row Output (out):** The output of the row's pattern.  Outputs 10 volts if:
~~~~
A) The clock signal is sent to the Global Clock or Individual Clock input jack
           AND
B) The pattern ought to trigger on this absoluteStep
~~~~

**Individual Row First Step Output (one):** Sends a trigger on the 1st step of the row's pattern.


### Displays:

**absoluteStep Display:** Shows the absoluteStep of the row.  The absoluteStep will increase by 1 every clock signal, until 
~~~~
A) The end of the pattern is reached
           OR
B) A reset trigger is sent to the Global Reset or Individual Reset input jack
~~~~


*Inspired by Frank Buss's Formula, Ryan Kirkbride's FoxDot, SuperCollider*


莅  氊  莅 氊 氊莅氊 氊氊莅莅  莅氊 莅氊
莅 氊氊   氊      莅莅   莅 氊莅  莅
  氊氊  莅 莅氊 氊  莅 氊 氊氊  氊  

莅  氊  莅 氊 氊莅氊 氊氊莅莅  莅氊 莅氊
莅 氊氊   氊      莅莅   莅 氊莅  莅
  氊氊  莅 莅氊 氊  莅 氊 氊氊  氊  

莅  氊  莅 氊 氊莅氊 氊氊莅莅  莅氊 莅氊
莅 氊氊   氊      莅莅   莅 氊莅  莅
  氊氊  莅 莅氊 氊  莅 氊 氊氊  氊    
    
  
## Father & Son Patch Sequencer

Patch matrix with 16 scenes.  If multiple buttons are active for a single row, the 2 active input signals will be summed.

![ComputerscarePatchSequencer](./doc/patch-sequencer-basic-2.png)

### Buttons:
**Patch Matrix:** Grid of 100 buttons.  When the button is lit in green, it means that the input column and output row is connected for the step that is being edited.  When the button is lit in red, it means that the input column and output row is connected for the currently active step.

**Next Active Step:** Moves to the next active step.  If the sequence length is set to greater than 1, the red lights in the patch matrix will change.

**Reset:** Resets the patch sequencer to step 1.

**Previous/Next Editing Step:** Move to edit the previous or next step.  Note that this will always cycle from 1 to 16 no matter what number of steps the knob is set to.

### Knobs:
**Number of Steps:** Choose between 1 and 16 steps.

### Input Jacks:

**Input Column:** 10 input jacks for any sort of signal you want.

**Clock (clk):** Moves to the next active step.  If the currently active step is equal to the number-of-steps, it will go back to step 1.  Does the same thing as the 'Next Active Step Button'.

**Reset (rst):** Resets the patch sequencer to step 1.  Does the same thing as the 'RST' button.


**Randomize (shuf):** Randomizes the patch matrix.  Does the same thing as the 'randomize' selection from the right-click menu.  There are some randomization options available via the right-click menu:

### Randomization Options:
![ComputerscarePatchSequencerRightClickOptions](./doc/patch-sequencer-right-click-options.png)

**Only Randomize Active Connections:** Only input rows/output columns with patch cables connected will be randomized.  Default is un-checked.

## Which Step to Randomize (3 options):

**Edit step:** The patch matrix for the step that is currently being edited will be randomized

**Active step:** The patch matrix for the step that is currently active will be randomized

**All steps:** All patch matrices for all steps will be randomized

## Output Row Randomization Method (4 options):

**One or none:** 70% chance that one randomly-selected input button will be enabled

**Exactly one:** Exactly one randomly-selected input button will be enabled

**Zero or more** Each input button has a 20% chance of being enabled

**One or more:** One randomly-selected input button will be enabled, and the rest have a 20% chance each of being enabled


### Output Jacks:

**Output Row:** 10 output jacks which output the sum of the signals in that particular column.



*Inspired by Strum's Patch Matrix, Bidoo's ACnE Mixer, and Fundamental Sequential Switch*


ඦ蔩 蔩 ඦ蔩槑ඦඦ        蔩 钧 钧     槑  ඦ
钧   钧钧蔩槑ඦ       ඦ  槑 蔩 蔩猤  
 猤  槑钧猤钧      猤  蔩   ඦ钧     钧

ඦ蔩 蔩 ඦ蔩槑ඦඦ        蔩 钧 钧     槑  ඦ
钧   钧钧蔩槑ඦ       ඦ  槑 蔩 蔩猤  
 猤  槑钧猤钧      猤  蔩   ඦ钧     钧

ඦ蔩 蔩 ඦ蔩槑ඦඦ        蔩 钧 钧     槑  ඦ
钧   钧钧蔩槑ඦ       ඦ  槑 蔩 蔩猤  
 猤  槑钧猤钧      猤  蔩   ඦ钧     钧
 


## Debug
A simple module for debugging.  Displays a list of the last 16 voltages read from IN, with the most recent one at the top.

![ComputerscareDebug](./doc/computerscare-debug-basic.png)

### Inputs:
**Trigger (trg):** When a trigger signal is detected, the voltage at the Input will be placed at the top of the list of voltages, and the voltage at the bottom will be discarded from the display.

**Input (in):** Any sort of signal you wish to probe.

**Clear:** Reset the list of voltages back to all zeros.

### Buttons:
**Trigger (trg):** Manual Trigger

**Clear (clr):** Manual Clear

*Inspired by ML Modules Volt Meter*


⼛ೊ ⼛蠍ೊ ೊ  ⼛ೊ蠍ʬ     ⼛
    ʬ    ʬ  蠍⼛     蠍ೊ蠍ʬ蠍⼛ 
 蠍 ⼛ೊ⼛   蠍蠍ೊʬ     蠍  ⼛⼛

⼛ೊ ⼛蠍ೊ ೊ  ⼛ೊ蠍ʬ     ⼛
    ʬ    ʬ  蠍⼛     蠍ೊ蠍ʬ蠍⼛ 
 蠍 ⼛ೊ⼛   蠍蠍ೊʬ     蠍  ⼛⼛

⼛ೊ ⼛蠍ೊ ೊ  ⼛ೊ蠍ʬ     ⼛
    ʬ    ʬ  蠍⼛     蠍ೊ蠍ʬ蠍⼛ 
 蠍 ⼛ೊ⼛   蠍蠍ೊʬ     蠍  ⼛⼛





