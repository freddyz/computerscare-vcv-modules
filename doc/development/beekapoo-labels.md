# Compact labels

Beekapoo keeps full names in tooltips. Panel captions try the original name,
then shorter forms, before reducing text size. Words never split across lines.
Input/output captions have reserved space and are not silently dropped.

The vocabulary was checked against 35,086 literal control-label declarations
in 272 plugins in the local Rack Library checkout (17,060 distinct strings).
This covers `configInput`, `configOutput`, `configParam`, `configSwitch` and
`configButton`; generated runtime names and panel-only SVG lettering are not
included in that count.

Edit the phrase table and shortening stages in `src/BeekapooLabels.hpp`.
Longest matching phrases win. Replacements match whole words and ignore case.
The same alternatives serve input, output and parameter captions.

| Rule or family                | Examples, from longer to shorter                                                               |
| ----------------------------- | ---------------------------------------------------------------------------------------------- |
| 1. Frequency                  | Frequency → Freq → Fq                                                                          |
| 2. Modulation phrases         | Frequency modulation → FM; amplitude modulation → AM; phase modulation → PM                    |
| 3. Modulation type            | Linear frequency modulation → Lin FM → LFM; exponential → Exp FM → EFM                         |
| 4. Pulse width                | Pulse width / pulse width modulation → PW                                                      |
| 5. Filter responses           | High-pass filter → HP; low-pass → LP; band-pass → BP; all-pass → AP                            |
| 6. Cutoff                     | Cutoff frequency → Cutoff Freq → Cutoff                                                        |
| 7. Resonance                  | Resonance → Res                                                                                |
| 8. Shelves and notch          | High shelf → Hi Shelf → HS; band reject → Notch                                                |
| 9. Oscillators and amplifiers | Low frequency oscillator → LFO; voltage controlled oscillator → VCO; amplifier → VCA           |
| 10. Pitch units               | 1V/Octave pitch → Pitch; volts per octave → V/Oct                                              |
| 11. Tuning                    | Octave → Oct; semitones → Semi → St; fine tune → Fine; detune → Det                            |
| 12. Envelope stages           | Attack → Atk; decay → Dec; sustain → Sus; release → Rel                                        |
| 13. Stage endpoints           | End of cycle → EOC; end of attack → EOA; end of fall → EOF                                     |
| 14. Level                     | Amplitude → Amp; volume → Vol; level → Lvl                                                     |
| 15. Attenuation               | Attenuverter → Attv; attenuation → Atten                                                       |
| 16. Clocks and events         | Clock → Clk; trigger → Trig → Trg; reset → Rst; retrigger → Rtrg                               |
| 17. Synchronization           | Synchronization / synchronize → Sync                                                           |
| 18. Feedback                  | Feedback → Fdbk → FB                                                                           |
| 19. Effect mix                | Dry/wet → Mix; stereo width → Width                                                            |
| 20. Effects                   | Delay → Dly; reverb → Rvb; freeze → Frz                                                        |
| 21. Time and location         | Length → Len; duration → Dur; position → Pos; offset → Ofs                                     |
| 22. Clock ratios              | Division → Div; multiplier → Mult; preserve numeric ratios such as 3/2                         |
| 23. Dynamics                  | Threshold → Thresh → Thr; compression → Comp; ratio → Rat                                      |
| 24. Probability               | Probability → Prob → Prb; random/randomize → Rand → Rnd                                        |
| 25. Sequencing                | Sequence/sequencer → Seq; pattern → Patt; density → Dens                                       |
| 26. Direction                 | Left/right → L/R; forward → Fwd; reverse → Rev; clockwise/counterclockwise → CW/CCW            |
| 27. Scope                     | External/internal → Ext/Int; polyphonic/monophonic → Poly/Mono                                 |
| 28. Polarity                  | Positive/negative → Pos/Neg; inverted → Inv; preserve + and − signs                            |
| 29. Waveforms                 | Triangle → Tri; square → Sqr; sawtooth → Saw; waveform → Wave; wavetable → WT                  |
| 30. Sample controls           | Sample and hold → S&H; track and hold → T&H; sample rate → SR; bit depth → Bits                |
| 31. Performance               | Velocity → Vel; aftertouch → AT; pitch bend → Bend                                             |
| 32. Common modifiers          | Amount → Amt; depth → Dep; selection → Sel; harmonics → Harm                                   |
| 33. Measurement units         | Hertz → Hz; milliseconds → ms; beats per minute → BPM; decibels → dB                           |
| 34. Number attachment         | Channel 12 → Ch12; Channel #12 → Ch12; Track 3 → Trk3                                          |
| 35. Numbered CV               | Channel 12 CV → Ch12 CV → CV12; never discard the number                                       |
| 36. Port context              | Left clock input → L Clk; retain a lone In/Out and numbered In3/Out4                           |
| 37. Separator normalization   | FREQUENCY_MODULATION and Frequency-modulation both match Frequency modulation                  |
| 38. Explanatory clauses       | Hold - overrides control when patched → Hold; full instruction stays in tooltip                |
| 39. Unknown vocabulary        | Try removing internal vowels while retaining word starts, numbers and punctuation              |
| 40. Last-resort initials      | Use initials only after readable forms fail; retain FM, CV, L/R, units and numeric identifiers |

No suffix dots or ellipses. Abbreviations do not alter saved names, tooltips or
source-module labels. A very long unfamiliar name in an extremely small cell
can still require initials; widening the panel restores the fuller name.
Fitting is cached until layout changes, with no additional DSP work.
