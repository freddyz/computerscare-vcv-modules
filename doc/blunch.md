# Blunch

Blunch is a text-based clock and trigger sequencer with up to 16 independent channels. Write clock rates, durations, sequences, rests, and random choices as formulas. Each channel has its own page of formula lines.

## Quick start

1. Connect **out** to a trigger or clock input.
2. Select **Ch 1** in the top-right channel selector.
3. Write `120bpm` and press **Ctrl+Enter** or **Cmd+Enter** to submit and start the line.
4. Try `[300 400]ms` for alternating 300 ms and 400 ms intervals.
5. Add another formula on a new line. With automatic Line advance, Blunch plays each line in turn and loops the page.

The **Run** button enables playback. New modules start with one output channel and one to four short example lines in channel 1.

## Pages, lines, and blocks

- **Channel/page:** one channel's complete set of formula lines.
- **Line:** one formula, containing one or more clock blocks.
- **Block:** a clock value with any repeats, rests, or other modifiers attached to it.

With automatic Block and Line advance enabled, Blunch finishes each block, continues through the line, and then moves to the next line. After the last line, the page repeats.

## Basic language reference

### Clock values and units

| Formula   | Meaning                                           |
| --------- | ------------------------------------------------- |
| `120bpm`  | 120 beats per minute: one pulse every 0.5 seconds |
| `2hz`     | Two pulses per second                             |
| `500ms`   | One pulse every 500 milliseconds                  |
| `.5s`     | One pulse every half second                       |
| `2min`    | One pulse every two minutes                       |
| `1:30min` | A one-minute, thirty-second interval              |
| `120`     | A number without a unit uses Default number unit  |

Decimals can begin with a dot: `.5s` and `0.5s` mean the same thing. The context menu’s **Default number unit** selects BPM, seconds, milliseconds, Hz, minutes, millihertz, or kilohertz for bare numbers. BPM is the initial setting. Explicit units always override it. The setting applies when a line is next submitted; existing Next and Now formulas keep their units. Clock values must be positive. Units are case-insensitive. Additional unit spellings include `rpm` for BPM, `cps` for Hz, `mhz` for millihertz, `khz` for kilohertz, `sec`/`seconds`, and `m`/`minutes`.

### Sequential values and brackets

Separate clock blocks with spaces or commas:

```text
300ms 400ms 500ms
```

Brackets group a sequence and let its values share a unit:

```text
[300 400 500]ms
```

Both examples play 300 ms, 400 ms, then 500 ms. An explicit unit inside a group takes precedence over its shared unit: `[300ms 2hz]s` keeps those two explicit units.

### Nested sequences: parentheses

Inside parentheses, each item takes a turn. A nested sequence supplies its next item each time its turn comes around. Shorter sequences wrap while longer sequences continue.

```text
(.5 .7 (.3 .4))s
```

This plays:

```text
.5s .7s .3s   .5s .7s .4s
```

Then the pattern repeats. The inner `.3 .4` sequence advances once per outer pass; it does not play both values at once.

For two changing sequences:

```text
((1 2) (3 4 5))s
```

The full pattern is `1, 3, 2, 4, 1, 5, 2, 3, 1, 4, 2, 5` seconds. It repeats when both inner sequences return to their beginnings.

A parenthesized sequence immediately following a block alternates with that preceding block. For example, `500ms (300ms 400ms)` plays `500, 300, 500, 400` ms.

Use brackets for a straight sequence; use nested parentheses when items should advance on successive turns. A bracketed sequence plays all its values when its turn arrives: `(1 (2 3 [3 2 1]))hz` plays `1, 2, 1, 3, 1, 3, 2, 1`, then repeats.

### Repeats: `@`

| Formula            | Meaning                                                 |
| ------------------ | ------------------------------------------------------- |
| `120bpm@4`         | Play four pulses at 120 BPM before advancing            |
| `120bpm@2s`        | Keep this clock for two seconds                         |
| `120bpm@{2\|4\|8}` | Choose a repeat count of 2, 4, or 8                     |
| `120bpm@(2 4)`     | Alternate repeat counts of 2 and 4 on successive visits |
| `[300 400]ms@2`    | Repeat each value twice: 300, 300, 400, 400 ms          |

A repeat attached to a group applies to its individual blocks. It does not mean repeating the entire group as a single unit.

### Rests: `~`

Put a tilde before a value or group to keep its timing but silence its output:

```text
500ms ~500ms
```

This alternates a pulse with a silent interval. `~500ms@4` rests for four clock intervals. `~@2s` is a two-second rest without a clock value.

### Probability: `?`

`120bpm?75` gives the block a 75% chance of playing; otherwise it rests for the same timing. A bare `?` means 50%.

Put probability before repeat and total-length modifiers:

```text
120bpm?75@4
```

### Random choices and ranges: `{}`

| Formula             | Meaning                                    |
| ------------------- | ------------------------------------------ |
| `{90\|120\|150}bpm` | Choose one of three tempos                 |
| `{90-150}bpm`       | Choose a value in the range                |
| `{300ms\|.5s\|2hz}` | Choose between clocks with different units |
| `{2hz\|~2hz}`       | Choose a clock or a rest                   |

Choices are redrawn as the block is revisited. The **Seed** control makes those choices repeatable when its combined knob/CV value is positive.

### Total length: `#`

Use `#` to set how long a block or group occupies before moving on:

| Formula          | Meaning                                                |
| ---------------- | ------------------------------------------------------ |
| `120bpm#8`       | Run for eight clock ticks                              |
| `[300 400]ms#4s` | Cycle this group for four seconds                      |
| `120bpm#4x`      | Run until four rising edges arrive at external clock X |

In `{120bpm|240bpm#2}#4s`, choosing `240bpm#2` holds that choice for two pulses; the whole formula still lasts four seconds. Faster clock values produce more pulses within that same duration.

Total lengths can also use random choices, such as `120bpm#{4|8}`. Modifiers go in the order **probability, repeat, total length**: `120bpm?75@2#4s`.

### External clocks: `w`, `x`, `y`, `z`

The letters refer to the four external clock inputs:

- `x` follows external clock X.
- `x@4` stays on X for four pulses.
- `120bpm@4x` runs its internal clock until four X pulses have elapsed for the repeat.
- `[120 90]bpm#4x` cycles the group until four X pulses have elapsed in total.

External clocks can be used in sequences and random choices just like internal clock values.

## Editing and selecting channels

Use the channel selector to choose **All** or an individual channel. Selecting a channel does not change the output polyphony.

**All view** shows one formula row per channel. Tab, Shift+Tab, or Enter opens the channel under the cursor. Tab from an individual channel returns to All view. Ctrl+[ and Ctrl+] move between individual channel pages.

Direct editing in All view is allowed when a channel is stopped/paused or has only one nonblank formula line. A playing channel with multiple formula lines is read-only there and has no editing caret; open its channel page to edit it. Trailing blank lines do not count as extra formulas.

Stopped channels show their edited text immediately, even if they have never played. Playing multi-line channels show their current active formula. Channels beyond the selected polyphony use faded italic text and a lighter grey background; they remain selectable and editable.

### Draft, Next, and Now

| View      | Meaning                                                    |
| --------- | ---------------------------------------------------------- |
| **Draft** | Your editable formulas. Typing does not change playback.   |
| **Next**  | Submitted formulas waiting for the selected Wait boundary. |
| **Now**   | The formulas currently used for playback.                  |

Press **Ctrl/Cmd+Enter** or click **Play** to stage all Draft changes for the focused channel, including deleted lines. Changes become active at the selected **Wait** boundary without jumping away from a surviving active line. If the active line was deleted, playback continues with a remaining formula. **Immediate** applies the submission straight away. Further typing changes Draft only; it cannot overwrite Next. Starting an empty channel activates its submitted formula because there is no running boundary to wait for.

**Ctrl/Cmd+Shift+Enter** also stages the channel page. With **Wait: Page**, the submitted page changes take effect together when the page wraps. With other Wait modes, changes take effect at the selected boundary on the playing line, or when playback reaches another queued line. An invalid line prevents the whole page submission; correct the red-marked text and try again.

The display’s left edge shows a green focus marker. Steady yellow ticks mark Draft edits; a slow yellow pulse marks Next waiting to take over. Both can appear together, per line in a channel page or summarized per channel in All view. Changed Draft text is tinted yellow. Play stays raised while changes exist: steady yellow for Draft, pulsing yellow for Next. Clicking Play stages the whole channel’s Draft; it does not cancel an existing Next submission.

Click the small **Draft / Next / Now** label in the display, press **F6**, or use **Formula view** in the context menu to switch versions. Next and Now are read-only. Unchanged lines in Next show Now in faded text. Hold **F7** to peek at Now; release it to return. Typing returns to Draft. Switching views never replaces your edits.

The **x** button to the right of Stop acts on the selected channel:

- With Draft edits, discard them and restore Next, keeping Next queued.
- With no Draft edits, unstage Next and restore Now.
- **Shift-click** discards both Draft and Next at once.

**Esc** does the same for the focused line in a channel page, or the selected channel in All view. With no changes on the focused line, it returns to All view.

Undo with **Cmd/Ctrl+Z**; redo with **Cmd/Ctrl+Shift+Z** or **Cmd/Ctrl+Y**. Undo changes Draft only. Submit restored text to apply it to playback.

Patches save Draft, Next, and Now separately. Reopening a patch keeps unfinished edits separate from playback and retains queued submissions. Wait uses its current setting, including if you change it after submitting.

## Controls

- **Run:** master playback enable.
- **Block / Line / Page:** momentary reset buttons for the current block, line, or whole channel page. Their inputs perform the same resets.
- **Time:** speeds up or slows down internal timing. Each +1 doubles the speed; each −1 halves it.
- **Seed:** positive values give repeatable random choices. Zero, or a negative combined value from CV, uses fresh randomness. The knob itself ranges from 0 to 10. Seed knob/CV changes are latched independently by each channel when its seeded pattern restarts: after its counted or timed repeat, or at the end of an enclosing `#` group. Resets and starting a pattern use the latest seed. With Seed CV patched, pattern restarts wait 1 ms before sampling the seed and choosing the first value. This lets a sequencer clocked by EOB, EOL, or EOP supply its new seed for the upcoming pattern.
- **Polyphony:** choose 1–16 output channels, or Auto. Auto extends to the highest channel with an activated formula. New modules default to Auto.
- **Edit/Ctrl:** switch between formula editing and keyboard/mouse control of channels.

The module starts at 16 HP. Drag either panel edge to resize it in 1 HP steps. The width is saved with the patch. Editor font, spacing, and line wrapping are available in the context menu. Text height and line height are independent: changing the glyph height does not move the rows. **Steady line height** is on by default, sizing 16 rows to the display. Turn it off to size rows from the font size and text height. **Editor line height** adjusts spacing only when Steady line height is off; the slider is disabled while it is on. Disabled channels retain alternating background stripes.

## Inputs

| Input                   | Behavior                                                                                                                                                   |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Run**                 | In Gate mode, above 1 V permits playback; low pauses it. In Trigger toggle mode, rising edges toggle playback. Unpatched permits playback while Run is on. |
| **Block / Line / Page** | Mono triggers (channel 1); rising edges around 1 V reset the corresponding playback scope. Resets follow the Run/transport state.                          |
| **Time CV**             | Mono, channel 1. Adds to Time at 1 V/octave. Combined range is −5 to +5: 1/32× to 32× speed.                                                               |
| **Seed CV**             | Mono, channel 1. Adds one seed unit per volt to the knob. Zero or negative totals select fresh randomness.                                                 |
| **W / X / Y / Z**       | Mono clocks (channel 1), rising edges around 1 V; referenced by letter in formulas or selected in the context menu.                                        |

Run supports polyphony: a mono cable controls every active channel; poly channels control matching sequencer channels. Channels without a matching Run CV channel pause while that cable is connected. Turning the Run button off stops all channels regardless of its input.

## Outputs

All outputs use the selected polyphony, with one signal per sequencer channel.

| Output    | Signal                                                            |
| --------- | ----------------------------------------------------------------- |
| **out**   | 10 V triggers (1 ms) or gates with adjustable length              |
| **phase** | A 0–10 V ramp showing position within the current clock interval  |
| **EOB**   | End of Block: a 10 V trigger when playback moves to another block |
| **EOL**   | End of Line: a 10 V trigger when the current line completes       |
| **EOP**   | End of Page: a 10 V trigger when the page wraps to its beginning  |

For an external clock, phase estimates the period from incoming edges. It needs two edges to establish a period and holds at 10 V if the clock stops. Playback highlights appear only for channels that are playing.

## Control mode

With no changes, the header’s **Play** button starts or stops the focused channel and stays pressed while it plays. With Draft edits, it stages them; while Next is waiting, it pulses yellow. **Stop** stops that channel. The green segment on the display’s left wall marks the focused row.

Click **Edit/Ctrl** to enter Control mode. Click anywhere on Blunch to give it keyboard focus; a subtle orange stripe on the left marks focus. Hovering alone does not activate Control shortcuts. It opens All view, disables text editing, and selects the cursor’s channel. Selected rows have a green background; the focused row has a thin yellow outline. At least one channel stays selected.

| Control                | Action                                                                                               |
| ---------------------- | ---------------------------------------------------------------------------------------------------- |
| Up / Down              | Move focus; a single-row selection follows the cursor, while a multiple-row selection stays selected |
| Shift+Up / Down        | Extend or shrink a range from the anchor row                                                         |
| Ctrl/Cmd+Enter         | Add or remove the focused channel from the selection                                                 |
| Click                  | Replace the selection with the clicked channel                                                       |
| Ctrl/Cmd+click         | Add or remove the clicked channel                                                                    |
| Shift+click            | Select a range to the clicked channel                                                                |
| Space                  | Stop the selection if all selected channels are running; otherwise start the selection               |
| R                      | Reset selected channel pages to their first formula, keeping stopped channels stopped                |
| Enter in All view      | Open the focused channel                                                                             |
| Tab / Shift+Tab        | Switch between All and channel view                                                                  |
| Esc                    | Return to All view without clearing selection                                                        |
| Hold backtick or tilde | Show Control-mode shortcuts                                                                          |

Selection stays intact when switching views. To replace it, click a row. Removing the final selected row keeps that row selected. Control commands do not change the output polyphony or override the Run input. Click **Ctrl** to return to editing; in Edit mode, mouse and keyboard text selection work normally.

## Playback display

Choose **Visual → Blinkers** in the context menu:

- **Full:** trigger flashes and yellow outlines.
- **Minimal** (default): subtly uneven polygon flashes without outlines.
- **Text:** the active clock text is yellow, flashes green on each trigger, and fades back to yellow, with no background or outlines.
- **Off:** no playback blinks, text flashes, or outlines.
- **Border:** the polygon outline flashes and fades on each trigger, disappearing between triggers.

**Visual → Bars** independently shows or hides timing progress bars in any blinker mode.

Display style and default number unit are saved with the patch.

## Context-menu timing options

- **Block advance:** Automatic advances when the current block finishes. Off holds the current block. External W/X/Y/Z advances on the selected input.
- **Line advance:** Automatic advances after the current line. Off keeps it looping. External W/X/Y/Z advances lines on the selected input.
- **Wait:** applies submitted text immediately, or waits for the next Clock, Block, Line, Page boundary, or an external W/X/Y/Z edge.
- **Run input:** choose Gate or Trigger toggle behavior.

## Channel sync

The top **Sync** dropdown and context menu’s **Channel sync** share the same settings. **Free** is the default: channels run independently.

Choose a source channel, its **End of Block**, **End of Line**, or **End of Page** boundary, and where followers restart: **Current block**, **Current line**, or **Page beginning**. Each source boundary restarts the other playing channels. Longer formulas are interrupted; shorter ones can loop between restarts. Stopped channels stay stopped. If the source is stopped or outside the output polyphony, followers continue independently.

For example, use `120bpm#4s` on Channel 1, choose Channel 1’s End of Line, and restart followers at Current line. Their lines restart together every four seconds, regardless of their individual rates or random choices. This aligns restart points; it does not stretch formulas to fit.

Sync uses Now and leaves Draft untouched. Next retains its usual Wait behavior. A patched Seed CV keeps its 1 ms sampling grace period without adding time to the shared pattern boundary. Sync settings are saved with the patch.

## Keyboard shortcuts

Hold **backtick** while Blunch is hovered or focused to show its shortcut popup. Release to hide it. Editing is blocked while the popup is visible. **Shift+backtick types `~`**, the rest operator.

| Shortcut             | Action                                      |
| -------------------- | ------------------------------------------- |
| Ctrl/Cmd+Enter       | Stage and start the channel’s Draft page    |
| Ctrl/Cmd+Shift+Enter | Stage the changed lines on the page         |
| F6                   | Cycle Draft / Next / Now                    |
| Hold F7              | Peek at Now                                 |
| Enter in All view    | Open the channel under the cursor           |
| Tab / Shift+Tab      | Switch between All and the selected channel |
| Esc                  | Cancel / return to All                      |
| Ctrl+[ / Ctrl+]      | Previous / next channel                     |
| Ctrl+,               | Stop the selected channel                   |
| Ctrl+/               | Toggle Run                                  |
| Ctrl+.               | Stop all                                    |
| Ctrl+Space           | Start all                                   |

The transport/navigation shortcuts labeled Ctrl use the Control key on macOS too. Standard text editing uses the usual platform modifier.

## Randomize

Rack Randomize replaces the selected line with a short clock formula. In All view it acts on the selected channel's displayed line. Polyphony and other controls stay as they are.

Examples favor 50–180 BPM and equivalent Hz or millisecond timings, with occasional sequences, repeats, random choices, and longer periods in seconds. Saved patches restore their existing formulas and settings.

For custom recipe collections, see [Random-template customization](development/blunch-random-templates.md).

## Panel layout

Choose **Panel → Max visual** in the context menu to expand the display vertically and collect the input/output jacks in a dense rail of tiny ports. Hover a jack for its label. Hidden controls keep their settings. Use **Normal UI** above the display, or **Panel → Normal**, to restore the controls and original module width. Cables remain connected, and the layout is saved with the patch.

## Clock output gates

Choose **Clock output → Trigger** for the usual 1 ms pulses, or **Gate** for a variable high time. **Gate length** sets 0–100% of each clock interval. At 120 BPM, 50% gives a 250 ms gate. Random durations use their selected interval, and Speed of time scales internal gate lengths too.

At 0% the clock output stays low; at 100%, consecutive playing steps can join into one sustained gate. Rests and stopped channels stay low. Changes to Gate length apply on the next pulse. EOB, EOL and EOP remain short triggers.

External clocks use the last measured interval. Until two rising edges establish that interval, the output uses a short 1 ms pulse. Irregular external clocks are estimates; an arriving edge replaces the previous gate.

Enable **Apply valid edits while typing** in the context menu to submit edits automatically once the page validates. The Wait setting still determines when they become active; invalid edits leave playback unchanged.

The **Gate** knob sets gate length as a percentage of each step. Gate length CV adds to the knob: 10 V adds 100 percentage points, clamped to 0–100%. Mono CV affects all channels; polyphonic CV addresses each channel. Trigger mode keeps its fixed short pulse.
