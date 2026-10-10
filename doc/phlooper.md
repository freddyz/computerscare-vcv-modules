# Phlooper

A bank of up to sixteen independent loops, recording corresponding channels from the mono or polyphonic left/right inputs. Small differences in loop length or speed let the repetitions gradually drift apart.

## Recording and playback

Connect audio to L / mono, optionally adding R for stereo. Mono audio cables feed all loops; polyphonic cables feed corresponding loops. Click Record to start and again to stop, or hold a Record gate high. The first recording establishes the buffer duration, up to 60 seconds, and seeds all sixteen loops. Subsequent recording overdubs only the addressed loops; each has its own audio storage.

Record mix blends retained loop audio toward incoming audio. Overdub mode is selected in the context menu and defaults to Blend. Blend mode replaces progressively; Add retains the old audio and adds the incoming signal at Record mix level. Internal overdub values are bounded to ±20 V. While overdubbing, each addressed loop monitors the selected Blend/Add mix immediately. While erasing, addressed loops contribute silence. Other loops keep playing, and Output mix still controls the dry input.

Erase removes the portion beneath each addressed head while held high. Stop freezes a head in place; Mute silences playback while the head keeps moving. A Restart rising edge resets the addressed heads to Start. Panel buttons operate on all active loops. A monophonic control cable broadcasts to all loops; a polyphonic cable addresses loops by channel, with missing channels inactive.

Start and Length are percentages of the original recording. A region can wrap across the original buffer boundary. Restart begins at Start. Length changes apply while playing; Start changes latch independently when each head next wraps; a Restart adopts the new Start immediately for addressed heads. Output mix blends input with the average of the active loop outputs. Stereo is retained; a mono recording is sent equally to left and right.

Each loop has Mute, Solo, Pause, and direction buttons. The direction button cycles F (Forward), R (Reverse), and B (Forward then Reverse). Reverse plays the selected region backward; B alternates direction at each end. Restart begins F and B at the region start, and R at its end. End-of-loop triggers fire at each traversal boundary. Recording and erasing follow the selected direction. The direction dropdown above the visual sets all sixteen loops together and shows Mixed when their modes differ. Direction modes and the current B leg are saved with the patch.

The Ctrl button above the visual hides the row buttons and expands the visual across the display. Hidden controls retain their settings. This preference is saved with the patch.

The controls above the visual select Time/Length/Speed and set all loop directions. Zoom toggles between the full recording and all visible loop regions with 10% padding on each side. Zoom follows Start, Length, and CV changes and handles regions crossing the recording boundary. While dragging a knob, the zoom stays fixed and refits when the knob is released. Waveform sampling focuses on the zoomed time range to show smaller loops in more detail. Zoom affects only the view; playback continues normally.

The context menu's Visualization setting switches between Line and Waveform. Waveform shows a sampled overview of each loop's stored audio beneath the region and playhead markers, including stereo peaks without cancelling opposite-phase channels. Its bounded cache refresh runs incrementally while audio processes; very short transients in long recordings can be missed by overview sampling.

## Input reference

All polyphonic inputs support up to 16 channels. “Broadcast” means a one-channel cable applies to all loops; a cable with two or more channels addresses matching loops.

| Input | Voltage range and behavior | Channel mapping / unpatched behavior |
| --- | --- | --- |
| L / mono | Audio voltage, scaled by Input gain (0–4×) and VCA. ±5 V is the nominal audio reference; initial capture does not clamp audio, while overdubs clamp stored values to ±20 V. | Broadcast mono; poly channels record corresponding loops; missing channels are silent. Unpatched is silent. |
| R | Same audio scaling and range as L. | Broadcast mono; poly channels record corresponding loops; missing channels are silent. Unpatched copies each corresponding L channel. |
| Record | High at ≥1 V; low below 1 V. Starts initial recording or overdubs addressed loops. Panel Record also enables recording. | Broadcast mono; poly addresses individual loops, missing channels inactive. Initial recording establishes the shared duration and seeds all buffers from their audio inputs. |
| Erase | High at ≥1 V; low below 1 V. Erases under moving heads; takes precedence over overdubbing. | Broadcast mono; poly addresses individual loops, missing channels inactive. |
| Restart | Rising edge at ≥1 V, rearmed at ≤0 V. Resets addressed heads; holding high does not repeatedly restart. | Broadcast mono; poly addresses individual loops, missing channels inactive. A high signal on initial connection arms the trigger; it must go low then high to trigger. |
| Stop | High at ≥1 V; low below 1 V. Freezes addressed heads and suspends their recording/erasing. | Broadcast mono; poly addresses individual loops, missing channels inactive. Combines with global Stop and per-loop Pause. |
| Mute | High at ≥1 V; low below 1 V. Silences loop playback while heads, recording, and EOC continue; dry monitoring remains audible according to Output mix. | Broadcast mono; poly addresses individual loops, missing channels inactive. Combines with global/per-loop Mute and Solo. |
| Hold | High at ≥1 V; low below 1 V. Enables phase Hold, also enabled by the panel latch. | Channel 1 only; global behavior. Unpatched leaves the button in control. |
| Start CV | Adds 10 percentage points per volt to Start; result clamped to 0–100%. Playback adopts Start at wrap or Restart. | Broadcast mono; poly offsets individual loops, missing channels use the knob alone. |
| Length CV | Adds 10 percentage points per volt to Length; result clamped to 0.1–100%. Applies immediately; Time/Length Hold uses loop 1’s length. | Broadcast mono; poly offsets individual loops, missing channels use the knob alone. |
| Offset CV | Replaces knob spacing: ±10 V maps to ±100 ms in Time mode or ±20 percentage points in Length/Speed modes. Voltage is clamped to ±10 V; 0 V means zero offset. | Each supplied channel addresses one loop; **mono addresses loop 1 only**. Missing channels use knob spacing. |
| Speed CV | 1 V/octave added to Overall Speed: +1 V doubles, −1 V halves. Combined knob/CV rate clamped to ¼–4×, then multiplied by Speed-mode Offset. | Channel 1 only; controls all loops. Unpatched uses the knob. |
| Rec mix CV | Adds 10 percentage points per volt to Record mix; result clamped to 0–100%. | Channel 1 only; controls all loops. Unpatched uses the knob. |
| Out mix CV | Adds 10 percentage points per volt to Output mix; result clamped to 0–100% (dry to loops). | Channel 1 only; controls all loops. Unpatched uses the knob. |
| VCA | 0–10 V maps to 0–100% input level, multiplied by Input gain. Negative voltages give silence; above 10 V gives full level. Affects recording and dry monitoring. | Broadcast mono; poly controls individual loops, missing channels give silence. Unpatched passes full level. |

Speed-mode Hold captures Start, Length, and Offset into fitted regions; changes to those inputs apply on release. Overall Speed CV continues to scale all held pitches together. Gate inputs without a cable are inactive.

## Phasing

Loop 1 has the selected nominal period. Each subsequent loop adds another increment of Offset:

- Time ms: add Offset milliseconds to the period, preserving pitch.
- Length %: add Offset percent of the selected length per loop, preserving pitch.
- Speed %: add Offset percentage points to playback speed per loop, changing pitch.

Positive Time ms and Length % offsets extend periods with silence; negative offsets shorten periods and truncate audio. Speed mode sets the playback rate from 5% to 800%; each loop's direction is selected separately. Periods are bounded between 1 ms and 60 seconds. Loop joins use short fades; accelerated playback uses a windowed-sinc low-pass interpolator. Start and Length CV use 10 V for a full-range shift; Offset CV uses 10 ms per volt in Time mode and 2 percentage points per volt in Length/Speed modes.

## Files and patches

Load mono/stereo WAV from the context menu or drop a file onto the module. PCM 8/16/24/32-bit and 32/64-bit float WAV are supported, including WAVE_FORMAT_EXTENSIBLE. Multichannel WAV uses its first two channels. Files longer than 60 seconds are truncated; all sixteen loops receive the imported audio and restart together. WAV amplitudes of 1 map to 5 V.

Rack patch storage saves sixteen independent 48 kHz float WAV files, including inactive loops, and restores them with the patch. Mono recordings save as mono WAV; a stereo overdub turns that loop stereo. Saving retains the current audio, playheads, latched starts, and Record latch state. On load, loop starts adopt the current controls and CV during the first 2 ms while restored playhead positions are retained. Subsequent Start changes latch at each loop wrap. Clear recording in the context menu empties the buffer, resets playheads and Hold’s captured regions, and returns Start to 0% and Length to 100% (patched CV still applies); Erase only removes audio under moving heads.

Storage reserves roughly 369 MB of virtual address space per module at the maximum stereo capacity, with memory pages touched as recording progresses. Playback and recording allocate no memory. File decoding and writing take place outside the audio callback. Replacing a file briefly passes dry input while installing its buffers. Saving copies small chunks under a nonblocking audio-thread lock; audio passes dry if a snapshot copy overlaps the callback. Save after stopping overdubs for an exact consistent recording snapshot.

Overall Speed multiplies all playback rates from ¼× to 4×, with 1× at the
center. It changes pitch in every phase mode without changing the first
recording duration. EOC sends a 10 V, 1 ms trigger when a head wraps, with
one polyphonic channel per active loop. Muted loops still emit EOC; stopped
loops do not. Restarting a head does not emit EOC.

The display shows the original buffer duration in seconds, each loop’s
start/end brackets, and playheads moving from their latched starts. Brackets follow requested points immediately, while playheads retain their latched starts until wrap.
Length’s tooltip includes the selected duration in seconds below its percent
value. Dropdowns stay visually pressed while their menus are open.

The Stop button toggles all active loops between stopped and playing. Heads
resume from their held positions. Individual Stop gates remain effective
when the button is released; the button state is saved with the patch.

Offset CV is polyphonic and sets explicit per-loop offsets, replacing the
knob’s evenly spaced offset for each supplied channel. −10 V to +10 V maps
to −100 to +100 ms in Time mode, or −20% to +20% in Length/Speed modes; 0 V
means no offset. Missing channels retain the knob’s spacing. A mono cable
addresses loop 1 only. Values outside ±10 V are clamped.

Hold latches phase: the button toggles it, or a gate of at least 1 V holds it. In Time and Length modes, it preserves the current offsets between playheads and matches their periods to loop 1. In Speed mode, it keeps each channel's current playback rate and pitch and captures temporary in/out points proportional to those rates. All held channels then share exactly the same repeat time and one phase clock. The shortest current channel repeat time becomes the shared repeat time. Engaging Hold keeps the current source positions, including reverse playback. Overall Speed scales held rates together; per-loop Pause and Restart still work. Releasing Hold restores the selected regions and resumes drift. Fitted regions and their shared clock are saved with the patch. Start, Length, and Offset changes take effect after releasing Speed-mode Hold.

Patch saves retain every playhead and its latched Start, so reopening resumes
the phase relationship from the saved positions. Record is a latching button;
the nearby red light indicates recording from the button or a Record gate.
The button’s latch state is saved with the patch.

Start and Length CV are polyphonic offsets from their knobs: ±10 V adds or subtracts 100 percentage points, clamped to the valid range. Mono affects all loops; poly channels address individual loops, with missing channels using the knob alone. Start changes still latch at each loop boundary; Time/Length Hold uses loop 1’s CV-controlled length for every loop; Speed Hold fits each channel’s region.

Each display row has M (mute), S (solo), P (pause), and F/R/B (direction) controls. Multiple loops can be soloed together; mute still takes precedence. Pause freezes its playhead and resumes from that position. Manual toggles combine with incoming mute/stop gates and save with the patch.

With the pointer over Phlooper: Space toggles Stop, M toggles Mute, H toggles Hold, R toggles Record, T restarts all loops, E erases while held, Z toggles Zoom, and C toggles per-channel controls. Controls with shortcuts show their key in the hover tooltip. Hold the tilde/backtick key to display all shortcuts beside their controls. Modifier shortcuts remain available to Rack.

Small IN/OUT meters show stereo RMS level (5 V = 0 dB) with a green peak line held for 0.4 seconds. They use 8–18 randomly sized segments, with the longest three times the shortest, and a 20 ms level response. Their upper segments indicate high levels; lit segments glow according to Rack’s halo brightness setting.

Overall Speed CV is a mono 1 V/octave offset from the knob, clamped to ¼–4×. Rec mix and Out mix CV are mono offsets: ±10 V adds/subtracts 100 percentage points, clamped to 0–100%. The main knobs and buttons share labels with their CV/gate jacks.

Small input/output Gain knobs display dB, from silence (−∞ dB) through +12.04 dB (4×), defaulting to unity (0 dB). Input gain affects recording and dry monitoring; output gain applies to both mixed and polyphonic outputs. Changes are smoothed, and meters reflect the adjusted levels.

Audio inputs broadcast a mono cable to all loops. Polyphonic cables record channel 1 into loop 1, channel 2 into loop 2, and so on; missing channels are silent. An unconnected right input copies the corresponding left channel. Initial recording and overdubbing both use this mapping. Outputs default to a stereo mix of the active loops.

Output mode in the context menu defaults to Mix (stereo mixdown). Polyphonic sends one loop per channel on both outputs, using the selected loop count. Output gain applies in both modes.

Input VCA multiplies input gain before recording and overdubbing. With no CV cable, it passes full level. Patched VCA CV uses 0–10 V for 0–100%, multiplied by Input gain; mono CV controls all loops and polyphonic CV addresses individual loops. Zero passes no input into the buffer.

Meters follow the loudest instantaneous channel on each side for polyphonic inputs and outputs, avoiding cancellation between channels. Mix output mode meters the actual stereo mix.

Import WAV to all loops always copies the same file into all sixteen loops; dropping a WAV does the same. Export WAV lets you save any individual loop, or all sixteen (including inactive loops) as separate files named `<chosen name>-loop-1.wav` through `-loop-16.wav`. Exports contain the full stored buffers as 48 kHz float WAV, preserving mono/stereo, without playback gain, pitch, direction, or region cropping. Stop recording/overdubbing before export for a consistent snapshot.

Import WAV to loop lets you replace any individual loop while retaining the other audio and playheads. The existing shared buffer duration is preserved: shorter files are padded with silence and longer files trimmed. On an empty module, the imported file establishes the duration and other loops start silent. Finish initial recording before importing into an individual loop.

Display playhead/region snapshots publish every 64 audio samples; audio and EOC timing remain sample-rate accurate. Empty buffers bypass playback calculations, and fully faded muted/stopped voices skip interpolation while preserving head and recording behavior.

The Mute button and jack sit after Stop. The button latches mute for all loops while their playheads, recording, and EOC timing continue. Dry monitoring remains controlled by Output mix.
