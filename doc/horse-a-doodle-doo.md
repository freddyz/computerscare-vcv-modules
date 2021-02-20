# Horse A Doodle Doo

Trigger & CV sequencer with a mantra: turn one knob, get an interesting sequence.  Horse a Doodle Doo is not random.  The same parameter values will always produce the same trigger & CV sequence

## Pattern: 
Selects a unique trigger & CV sequence.  Small changes in the pattern value lead to large changes in the trigger & CV sequences.  Enable tooltips to see a visualization of each poly channel sequence and current active step.

### Pattern Spread: 
How much the pattern changes through the polyphony.  If this is set to 0%, every polyphonic channel will output the exact same trigger & CV sequence.

## Length: 
Select how many steps are in the sequence. 

## Length Spread: 
How the length of each polyphonic channel's sequence is "spread out" across all channels.  If this is set positive, higher poly channels will have more steps.  If this is set negative, higher poly channels will have fewer steps down to a minimum of 2 steps.  If set to 0%, all poly channels will have the same number of steps.  Enable tooltips to see the calculated length of each poly channel.

## Density:
Control how dense the trigger & CV sequence is.  Setting this to 0% will output no triggers and the CV will never change.  100% will output a trigger on every clock step and the CV will likely change each step

## Density Spread: 
How the density changes across the poly channels.  If this is set positive, higher poly channels will have higher density, eventually reaching a channel with 100% density meaning every step outputs a trigger.  If this is set negative, higher poly channels will have lower density (eventually reaching 0%, meaning that channel's sequence will never output triggers or change CV).  If set to 0%, all poly channels will have the same density.  Enable tooltips to see the calculated density value for each poly channel.

All sections include a CV input and attenuverter for explicily setting the pattern, length, and density of each poly sequence.

## Modes of operation (mode knob, or right click menu):
-Each channel outputs independent pulse & CV sequence
-All channels triggered by Ch. 1 sequence: The CV of every channel will only change when ch1's does.  All channels triggers are set to ch1
-Trigger Cascade: Each channel is triggered by the previous channel's trigger sequence
-EOC Cascade: Each channel is triggered by the previous channel's EOC

Enable tooltips and hover over the main "pattern" knob for a visualization of how these different modes work

## Right Click options
-CV Offset: Global offset voltage for all CV sequences
-CV Scale: Global scale for all CV sequences
-CV Phase: Change this to keep the same trigger sequences but change the CV sequence