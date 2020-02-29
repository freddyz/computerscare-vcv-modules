
# Oh Peas! Quad Quantenuverter
4-Channel attenuverter, offsetter, quantizer


<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/ohpeas-basic-1.png" width="500" alt="Oh Peas! Quad Quantenuverter" />

### Text Input:
The scale for quantization is programmed via the text field.  Oh Peas! expects numbers that mean “how many steps to the next note”. For example, the major triad “scale” (C, E, G) is programmed by entering `43` (meaning: 4 half steps, and then 3 half steps).  The minor triad (C, E flat, G) = `34` (3 half steps, then 4 half steps). The major scale would be `221222` (whole, whole half whole whole whole). The final half step back to the root note is always “implied” so you can safely skip it. Right click to see and select some pre-programmed common scales.

### Input Jacks:

**Main Input (input, _A_)**

**Range CV (range, _B_):** Multiplies the input value, attenuverted by the small Range CV knob

**Offset CV (offset, _C_):** Offset for the input value, attenuverted by the small Offset CV knob

### Knobs:
**Number of Divisions (divisions):** How many equal divisions the octave is split into.  Default is 12.

**Transpose:** Amount to add or subtract to final output signal.  Values between -NumDivisions and +NumDivisions, which is equivalent to +/- 1 octave.  The default is 0, which means that an input voltage of 0v outputs the note C.

**Range CV Attenuverter ("range", small knob, _b_):** Attenuverter for the Range CV input.  Scales Range CV between -1 and +1

**Range Knob ("range", large knob, _a_):** Attenuverter for Main Input signal.  Scales Main Input between -2 and +2

**Offset CV Attenuverter ("offset", small knob, _c_):** Attenuverter for the Range CV input.  Scales Range CV between -1 and +1

**Offset Knob ("offset", large knob, _d_):** Constant offset to add after "range" section is applied.  +/- 5 volts.




### Output Jacks:

**Attenuverted, Offset Output (out):** The attenuverted and offset main input signal.  The formula is: 

~~~~
<INPUT> * (<range knob> + <RANGE CV> * <range attenuverter>) +<offset knob> + <OFFSET CV> * <offset attenuverter>
~~~~

or... a bit easier to read using the italic letters above in the input/knob descriptions

~~~~
A*(a + B*b) + C*c + d
~~~~

**Quantized Output (quantized):** The attenuverted and offset main input signal, quantized to the desired scale.  The quantization works a bit differently than other quantizers.  Oh Peas first figures out how many steps are in the defined scale, and then equally distributes the input signal between the allowed scale notes.  This is easiest to understand by feeding a sawtooth LFO wave into the input, and noticing that the quantized output produces the scale values each with an equal amount of time.  Most quantizers seem to map the input voltage to the closest allowed scale note which makes a sawtooth wave spend more time on the notes that are farther away from their neighbors.  One implication of this is that plugging the V/Oct of a midi keyboard into Oh Peas will mean some notes on the keyboard will not be quantized to the same output note of Oh Peas!  In fact, that's why it's called "Oh Peas!"


~~~~
ЧxЧ-ፌፌ--   Ч  x䒜-᳹淧ፌ-   -  -淧xx-
 ፌ᳹ 䒜ፌ   x-䒜ፌፌ  ᳹-      ᳹xx   淧 
xx ᳹䒜  淧   -- ፌxЧ -᳹   xxx-x䒜x᳹ 
   淧x- --ፌ᳹xx䒜䒜 䒜 䒜x Ч  -- ᳹x - 
_==-=-=-__==-=-=-__==-=-=-__==-=-=-__==-=-=-_
ЧxЧ-ፌፌ--   Ч  x䒜-᳹淧ፌ-   -  -淧xx-
 ፌ᳹ 䒜ፌ   x-䒜ፌፌ  ᳹-      ᳹xx   淧 
xx ᳹䒜  淧   -- ፌxЧ -᳹   xxx-x䒜x᳹ 
   淧x- --ፌ᳹xx䒜䒜 䒜 䒜x Ч  -- ᳹x - 
~~~~
