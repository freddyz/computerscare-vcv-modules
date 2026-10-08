# Phlooper

A bank of up to sixteen independent loops, initially seeded by the same mono or stereo recording. Small differences in loop length or speed let the repetitions gradually drift apart.

## Recording and playback

Connect audio to L / mono, optionally adding R for stereo. Audio inputs use only channel 1. Click Record to start and again to stop, or hold a Record gate high. The first recording establishes the buffer duration, up to 60 seconds, and seeds all sixteen loops. Subsequent recording overdubs only the addressed loops; each has its own audio storage.

Record mix blends retained loop audio toward incoming audio. Blend mode replaces progressively; Add retains the old audio and adds the incoming signal at Record mix level. Internal overdub values are bounded to ±20 V.

Erase removes the portion beneath each addressed head while held high. Stop freezes a head in place; Mute silences playback while the head keeps moving. A Restart rising edge resets the addressed heads to Start. Panel buttons operate on all active loops. A monophonic control cable broadcasts to all loops; a polyphonic cable addresses loops by channel, with missing channels inactive.

Start and Length are percentages of the original recording. A region can wrap across the original buffer boundary. Restart begins at Start. Length changes apply while playing; Start changes move the source region immediately. Output mix blends input with the average of the active loop outputs. Stereo is retained; a mono recording is sent equally to left and right.

## Phasing

Loop 1 has the selected nominal period. Each subsequent loop adds another increment of Offset:

- Time ms: subtract Offset milliseconds from the period, preserving pitch.
- Length %: subtract Offset percent of the selected length per loop, preserving pitch.
- Speed %: add Offset percentage points to playback speed per loop, changing pitch.

Positive time/length offsets shorten periods and truncate audio. Negative values extend periods with silence. Speed stays forward and is bounded from 5% to 800%. Periods are bounded between 1 ms and 60 seconds. Loop joins use short fades; accelerated playback uses a windowed-sinc low-pass interpolator. Start and Length CV use 10 V for a full-range shift; Offset CV uses 2 offset units per volt.

## Files and patches

Load mono/stereo WAV from the context menu or drop a file onto the module. PCM 16/24/32-bit and 32-bit float WAV are supported. Files longer than 60 seconds are truncated; all sixteen loops receive the imported audio and restart together. WAV amplitudes of 1 map to 5 V.

Rack patch storage saves sixteen independent 48 kHz float WAV files, including inactive loops, and restores them with the patch. Mono recordings save as mono WAV; a stereo overdub turns that loop stereo. Saving while recording captures the current audio rather than the record-button state. Clear recording in the context menu empties the buffer; Erase only removes audio under moving heads.

Storage reserves roughly 369 MB of virtual address space per module at the maximum stereo capacity, with memory pages touched as recording progresses. Playback and recording allocate no memory. File decoding and writing take place outside the audio callback. Replacing a file briefly passes dry input while installing its buffers. Saving copies small chunks under a nonblocking audio-thread lock; audio passes dry if a snapshot copy overlaps the callback. Save after stopping overdubs for an exact consistent recording snapshot.
