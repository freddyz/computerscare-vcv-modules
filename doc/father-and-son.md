
## Father & Son Patch Sequencer

Patch matrix with 16 scenes.  If multiple buttons are active for a single row, the 2 active input signals will be summed.

<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/patch-sequencer-basic-2.png" width="500" alt="Father & Son Patch Sequencer" />

### Buttons:
**Patch Matrix:** Grid of 100 buttons.  When the button is lit in green, it means that the input column and output row is connected for the step that is being edited.  When the button is lit in red, it means that the input column and output row is connected for the currently active step.

**Clock (clk):** Moves to the next active step.  If the sequence length is set to greater than 1, the red lights in the patch matrix will change.

**Reset (rst):** Resets the patch sequencer to step 1.

**Previous/Next Editing Step (< and >):** Move to edit the previous or next step.  Note that this will always cycle from 1 to 16 no matter what number of steps the knob is set to.

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


~~~~
ඦ蔩 蔩 ඦ蔩槑ඦඦ        蔩 钧 钧     槑  ඦ
钧   钧钧蔩槑ඦ       ඦ  槑 蔩 蔩猤  
 猤  槑钧猤钧      猤  蔩   ඦ钧     钧

ඦ蔩 蔩 ඦ蔩槑ඦඦ        蔩 钧 钧     槑  ඦ
钧   钧钧蔩槑ඦ       ඦ  槑 蔩 蔩猤  
 猤  槑钧猤钧      猤  蔩   ඦ钧     钧

ඦ蔩 蔩 ඦ蔩槑ඦඦ        蔩 钧 钧     槑  ඦ
钧   钧钧蔩槑ඦ       ඦ  槑 蔩 蔩猤  
 猤  槑钧猤钧      猤  蔩   ඦ钧     钧
~~~~ 

