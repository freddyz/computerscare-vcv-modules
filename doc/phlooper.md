# Phlooper

A bank of up to sixteen independent loops, initially seeded by the same mono or stereo recording. Small differences in loop length or speed let the repetitions gradually drift apart.

## Recording and playback

Connect audio to L / mono, optionally adding R for stereo. Audio inputs use only channel 1. Click Record to start and again to stop, or hold a Record gate high. The first recording establishes the buffer duration, up to 60 seconds, and seeds all sixteen loops. Subsequent recording overdubs only the addressed loops; each has its own audio storage.

Record mix blends retained loop audio toward incoming audio. Overdub mode is selected in the context menu and defaults to Blend. Blend mode replaces progressively; Add retains the old audio and adds the incoming signal at Record mix level. Internal overdub values are bounded to ±20 V. While overdubbing, each addressed loop monitors the selected Blend/Add mix immediately. While erasing, addressed loops contribute silence. Other loops keep playing, and Output mix still controls the dry input.

Erase removes the portion beneath each addressed head while held high. Stop freezes a head in place; Mute silences playback while the head keeps moving. A Restart rising edge resets the addressed heads to Start. Panel buttons operate on all active loops. A monophonic control cable broadcasts to all loops; a polyphonic cable addresses loops by channel, with missing channels inactive.

Start and Length are percentages of the original recording. A region can wrap across the original buffer boundary. Restart begins at Start. Length changes apply while playing; Start changes latch independently when each head next wraps; a Restart adopts the new Start immediately for addressed heads. Output mix blends input with the average of the active loop outputs. Stereo is retained; a mono recording is sent equally to left and right.

Each loop has Mute, Solo, Pause, and direction buttons. The direction button cycles F (Forward), R (Reverse), and B (Forward then Reverse). Reverse plays the selected region backward; B alternates direction at each end. Restart begins F and B at the region start, and R at its end. End-of-loop triggers fire at each traversal boundary. Recording and erasing follow the selected direction. The direction dropdown above the visual sets all sixteen loops together and shows Mixed when their modes differ. Direction modes and the current B leg are saved with the patch.

The Ctrl button above the visual hides the row buttons and expands the visual across the display. Hidden controls retain their settings. This preference is saved with the patch.

The controls above the visual select Time/Length/Speed and set all loop directions. Zoom toggles between the full recording and all visible loop regions with 10% padding on each side. Zoom follows Start, Length, and CV changes and handles regions crossing the recording boundary. While dragging a knob, the zoom stays fixed and refits when the knob is released. Waveform sampling focuses on the zoomed time range to show smaller loops in more detail. Zoom affects only the view; playback continues normally.

The context menu's Visualization setting switches between Line and Waveform. Waveform shows a sampled overview of each loop's stored audio beneath the region and playhead markers, including stereo peaks without cancelling opposite-phase channels. Its bounded cache refresh runs incrementally while audio processes; very short transients in long recordings can be missed by overview sampling.

## Phasing

Loop 1 has the selected nominal period. Each subsequent loop adds another increment of Offset:

- Time ms: add Offset milliseconds to the period, preserving pitch.
- Length %: add Offset percent of the selected length per loop, preserving pitch.
- Speed %: add Offset percentage points to playback speed per loop, changing pitch.

Positive Time ms and Length % offsets extend periods with silence; negative offsets shorten periods and truncate audio. Speed mode sets the playback rate from 5% to 800%; each loop's direction is selected separately. Periods are bounded between 1 ms and 60 seconds. Loop joins use short fades; accelerated playback uses a windowed-sinc low-pass interpolator. Start and Length CV use 10 V for a full-range shift; Offset CV uses 10 ms per volt in Time mode and 2 percentage points per volt in Length/Speed modes.

## Files and patches

Load mono/stereo WAV from the context menu or drop a file onto the module. PCM 8/16/24/32-bit and 32/64-bit float WAV are supported, including WAVE_FORMAT_EXTENSIBLE. Multichannel WAV uses its first two channels. Files longer than 60 seconds are truncated; all sixteen loops receive the imported audio and restart together. WAV amplitudes of 1 map to 5 V.

Rack patch storage saves sixteen independent 48 kHz float WAV files, including inactive loops, and restores them with the patch. Mono recordings save as mono WAV; a stereo overdub turns that loop stereo. Saving retains the current audio, playheads, latched starts, and Record latch state. On load, loop starts adopt the current controls and CV during the first 2 ms while restored playhead positions are retained. Subsequent Start changes latch at each loop wrap. Clear recording in the context menu empties the buffer; Erase only removes audio under moving heads.

Storage reserves roughly 369 MB of virtual address space per module at the maximum stereo capacity, with memory pages touched as recording progresses. Playback and recording allocate no memory. File decoding and writing take place outside the audio callback. Replacing a file briefly passes dry input while installing its buffers. Saving copies small chunks under a nonblocking audio-thread lock; audio passes dry if a snapshot copy overlaps the callback. Save after stopping overdubs for an exact consistent recording snapshot.

Overall Speed multiplies all playback rates from ¼× to 4×, with 1× at the
center. It changes pitch in every phase mode without changing the first
recording duration. EOC sends a 10 V, 1 ms trigger when a head wraps, with
one polyphonic channel per active loop. Muted loops still emit EOC; stopped
loops do not. Restarting a head does not emit EOC.

The display shows the original buffer duration in seconds, each loop’s
latched start/end ticks, and playheads moving from their latched starts. A
faint vertical line marks the requested Start while changes wait for wrap.
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

Start and Length CV are polyphonic offsets from their knobs: ±10 V adds or subtracts 100 percentage points, clamped to the valid range. Mono affects all loops; poly channels address individual loops, with missing channels using the knob alone. Start changes still latch at each loop boundary; Hold uses loop 1’s CV-controlled length for every loop.

Each display row has M (mute), S (solo), and P (pause) toggles. Multiple loops can be soloed together; mute still takes precedence. Pause freezes its playhead and resumes from that position. Manual toggles combine with incoming mute/stop gates and save with the patch.

With the pointer over Phlooper: Space toggles Stop, H toggles Hold, R toggles Record, T restarts all loops, and E erases while held. Modifier shortcuts remain available to Rack.

Small IN/OUT meters show stereo RMS level (5 V = 0 dB) with a green peak line held for 0.4 seconds. They use 8–18 randomly sized segments, with the longest three times the shortest, and a 20 ms level response. Their upper segments indicate high levels; lit segments glow according to Rack’s halo brightness setting.

Speed CV is a polyphonic 1 V/octave offset from the knob, clamped to ¼–4× per channel. A mono cable controls all loops; channels beyond a polyphonic cable use the knob alone. Speed-mode Hold captures these per-channel rates when engaged; subsequent Speed CV changes take effect on release. Rec mix and Out mix CV are mono offsets: ±10 V adds/subtracts 100 percentage points, clamped to 0–100%. Each knob/button shares one label with the jack directly below it.

Small input/output Gain knobs display dB, from silence (−∞ dB) through +12.04 dB (4×), defaulting to unity (0 dB). Input gain affects recording and dry monitoring; output gain applies to the final stereo mix. Changes are smoothed, and meters reflect the adjusted levels.
