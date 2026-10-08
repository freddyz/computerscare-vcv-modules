# Interleaver

Two mono audio inputs, one output, seven operations, and three timing choices. The dark display shows the output waveform and the **effective** operation, timing, routing and source, including CV changes.

## Operations

| Mode               | Behavior                                                                                                                                                                                                                        |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Zero splice        | Alternate full cycles bounded by rising zero crossings.                                                                                                                                                                         |
| Half-cycle splice  | Alternate segments bounded by both rising and falling zero crossings. Follow modes match the replacement half's polarity to the anchor half.                                                                                    |
| Equal-value splice | Switch between continuous delayed inputs when their voltages intersect or closely match and their slopes are similar. A 20 ms timeout allows switching when no suitable match exists; a 2 ms crossfade protects the transition. |
| Cycle weave        | Balance determines the proportion of B cycles: 0% selects A, 100% selects B. A deterministic accumulator distributes the cycles, rather than randomly clustering them. Used with Cycle routing.                                 |
| Cycle repeat       | Capture one cycle per source run and repeat it until the run ends. A count of 3 repeats the captured A cycle three times, then switches to B.                                                                                   |
| Reverse splice     | Play captured cycles backward, switching sources according to the routing pattern.                                                                                                                                              |
| Direct switch      | Switch between the continuous delayed inputs with a 2 ms crossfade. Gate/trigger/timer requests act immediately, without waiting for a captured cycle.                                                                          |

**Join** softens captured cycle edges from 0–1 ms, capped at one eighth of the captured cycle. Equal-value and Direct switch use their own continuous 2 ms crossfade. Input and output DC filters reduce DC from offsets or asymmetric half-cycle selections.

## Timing

- **Follow A:** A supplies the delayed timing grid. Selected B cycles fit A's cycle duration, changing B's pitch.
- **Follow B:** B supplies the grid; selected A cycles fit B's duration.
- **Free:** captured cycles play at their original speed. Neither input stays synchronized with the output. Weave in Free keeps both sources' captured cycle lengths.

Equal-value and Direct switch always use continuous delayed signals without fitting their cycles. Timing chooses the source of their Cycle routing clock: A, B, or the currently selected source for Free.

## Routing and patterns

| Route   | Source selection                                                                                                              |
| ------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Cycles  | A count cycles, then B count cycles (1–8 each). Half-cycle mode counts half-cycles. Weave uses Balance instead of the counts. |
| Gate    | Switch jack low selects A; high selects B. Counts and Balance do not change the selection.                                    |
| Trigger | Each rising edge on Switch toggles A/B. Holding the input high produces only one toggle.                                      |
| Timer   | A count × Interval milliseconds on A, then B count × Interval on B. Interval ranges from 1–2000 ms.                           |

For **3 A, then 1 B**, set A count to 3 and B count to 1. In Cycle routing these are cycle counts. In Timer routing with Interval at 100 ms, they request 300 ms of A, then 100 ms of B.

Captured-cycle operations apply external requests at the next segment boundary, so Gate, Trigger and Timer transitions are quantized to cycles (half-cycles in Half mode). Very fast timers can request multiple changes within one cycle; only the latest selection takes effect. Use Direct switch for immediate timer or external switching. Equal-value waits for a matching point, up to 20 ms.

Switch and Reset use Schmitt thresholds: high at 1 V, low at 0.1 V. They are read every sample. **Reset** restarts the pattern on A and resets the timer, without clearing audio history. Gate routing continues to obey the held gate level. Reset does not change the operation or timing.

## CV

Every main control has its own CV input and a small bipolar **+/−** amount knob. CV adds to the main knob's value; the knob remains the base setting.

At full positive amount, +10 V adds the entire parameter range; −10 V subtracts it. Negative amounts reverse the direction, and zero disables that CV input. Values clamp to their legal ranges; non-finite CV is ignored. Selectors and counts round to discrete values.

| Control           | Full CV span          |
| ----------------- | --------------------- |
| Mode              | 6 selector steps      |
| Timing            | 2 selector steps      |
| Route             | 3 selector steps      |
| A count / B count | 7 counts              |
| Balance           | 100 percentage points |
| Buffer            | 180 ms                |
| Join              | 1 ms                  |
| Interval          | 1999 ms               |

Controls update every 32 samples; gate/trigger/reset edges are handled every sample. Mode, timing and routing changes wait for a boundary when possible, with a 20 ms fallback for silence or slow waveforms and a short output fade.

**Buffer** sets a 20–200 ms lookback window. Follow and continuous-signal modes use it as output latency; Free uses it as startup lookback and a recent-history window. Buffer changes are smoothed and slew limited to one eighth of a sample per processed sample. The read clock therefore always moves forward, avoiding abrupt jumps, backwards reads and out-of-range history access. A full 180 ms change takes roughly 1.4 seconds; movement can bend pitch like a changing delay. Join and Balance use 10 ms smoothing; Interval uses 20 ms smoothing and maintains timer phase.

## Limits

Audio and CV use the first polyphonic channel. Bypass passes A through.

Captured segments must be at least two samples (or the 20 kHz capture limit at higher sample rates) and no longer than 200 ms. Follow modes pass the delayed anchor through if a complete segment is unavailable; unavailable replacement segments fall back to the anchor. External routing can select the other delayed input when the anchor has no captured segments. Free uses the other source when necessary and becomes silent if no recent segment is available. Cycles may repeat if playback outruns capture. Repeat holds a captured cycle for its run, bounded by the fixed sample-history capacity.

Cycle fitting uses linear interpolation. Large pitch differences, reverse playback and splice edges can create aliasing and strong timbral changes. These safeguards protect buffer access and transitions; they do not make the effect bandlimited or transparently time-stretched.

Storage is fixed. Audio processing never allocates, and replacement-cycle selection checks at most sixteen metadata entries. Parameter and audio-port IDs from the original module are preserved, though the panel is now 24 HP to fit the CV controls.
