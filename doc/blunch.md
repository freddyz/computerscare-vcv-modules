# Blunch

Blunch is a text-based clock and trigger sequencer with up to 16 independent channels. Write clock rates, durations, sequences, rests, and random choices as formulas. Each channel has its own page of formula lines.

## Quick start

1. Connect **out** to a trigger or clock input.
2. Select **Ch 1** in the top-right channel selector.
3. Write `120bpm` and press **Ctrl+Enter** or **Cmd+Enter** to submit and start the line.
4. Try `[300 400]ms` for alternating 300 ms and 400 ms intervals.
5. Add another formula on a new line. With automatic Line advance, Blunch plays each line in turn and loops the page.

The **Run** button enables playback. New modules start in Auto polyphony, with only channel 1 active and one to four short example lines.

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

Use brackets for a straight sequence; use nested parentheses when items should advance on successive turns. Inside brackets, a nested parenthesized group advances once per pass: `[90 100 110 (120 144)]` plays `90 100 110 120`, then `90 100 110 144`. A bracketed sequence plays all its values when its turn arrives: `(1 (2 3 [3 2 1]))hz` plays `1, 2, 1, 3, 1, 3, 2, 1`, then repeats.

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

### Skip probability: `??`

`120bpm??75` plays with a 75% chance; otherwise it skips straight to the next block without spending time on a rest. Bare `??` means 50%.

```text
1s? 2s?
1s 2s??
1s !2s??25
1s $2s??75
```

The first line always takes three seconds, including any rests. In the second, a skipped `2s` leaves consecutive one-second triggers. `$` and `!` work as with `?`. A failed check skips the current block and its remaining repeats. An enclosing fixed `#` duration still controls the length of its group. If every block skips, output stays silent and retries on a later clock; it cannot spin indefinitely.

### Random choices and ranges: `{}`

| Formula             | Meaning                                    |
| ------------------- | ------------------------------------------ |
| `{90\|120\|150}bpm` | Choose one of three tempos                 |
| `{90-150}bpm`       | Choose a value in the range                |
| `{300ms\|.5s\|2hz}` | Choose between clocks with different units |
| `{2hz\|~2hz}`       | Choose a clock or a rest                   |

Choices are redrawn as the block is revisited. The **Seed** control makes those choices repeatable when its combined knob/CV value is positive.

### Random mode: `$` and `!`

Prefix a block with `$` to make its choices, ranges, and probability always seeded. Prefix it with `!` for fresh randomness regardless of Seed:

```text
${80|120|160}bpm
${100-140}bpm
$120bpm?50
!{80|120|160}bpm
!{100-140}bpm
!120bpm?50
```

`$` uses the signed combined Seed knob/CV value: zero and negative values are valid seeds, and different negative seeds give different patterns. It follows the same seed-latching boundaries as ordinary seeded patterns. `!` ignores Seed. Without a prefix, behavior is unchanged: positive Seed is repeatable; zero or negative Seed uses fresh randomness.

A prefix on a group applies to its blocks, including random repeat and length arguments. An inner prefix overrides it; blocks outside the group are unaffected:

```text
$[120bpm?50 !{80|160}bpm]
```

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

**All view** shows one formula row per channel. Tab or Shift+Tab switches between All view and the channel under the cursor. Escape also returns to All view. Enter in All view does not navigate. Ctrl+[ and Ctrl+] move between individual channel pages.

Right-click a row in All view to play or stop that channel, or copy its displayed formula. These actions target the clicked row. In channel view, right-click a formula to play from that line or copy it. The playing target is labeled and disabled. Standard Cut, Copy, and Paste work on the current text selection; Copy line copies the clicked formula.

Direct editing in All view is allowed when a channel is stopped/paused or has only one nonblank formula line. A playing channel with multiple formula lines is read-only there and has no editing caret; open its channel page to edit it. Trailing blank lines do not count as extra formulas.

Stopped channels show their edited text immediately, even if they have never played. Playing multi-line channels show their current active formula. Channels beyond the selected polyphony use faded italic text and a lighter grey background; they remain selectable and editable.

### Draft, Next, and Now

| View      | Meaning                                                    |
| --------- | ---------------------------------------------------------- |
| **Draft** | Your editable formulas. Typing does not change playback.   |
| **Next**  | Submitted formulas waiting for the selected Wait boundary. |
| **Now**   | The formulas currently used for playback.                  |

In All view, press **Ctrl/Cmd+Enter** or click **Play** to stage Draft changes and start the selected channels, including deleted lines. In channel view, **Ctrl/Cmd+Enter** submits the channel’s Draft page and starts the line under the cursor, even with Line advance off; the selected Wait boundary still applies. All-view and Play-button submissions preserve the active line when it still exists. If the active line was deleted, playback continues with a remaining formula. **Immediate** applies the submission straight away. Further typing changes Draft only; it cannot overwrite Next. Starting an empty channel activates its submitted formula because there is no running boundary to wait for.

Turning **Run** back on applies the latest valid Draft/Next changes immediately, bypassing Wait, and resumes the remembered channels. **Ctrl/Cmd+Space** does the same and starts all channels. Invalid Drafts remain editable and do not start that channel.

**Ctrl/Cmd+Shift+Enter** also stages the channel page. With **Wait: Page**, the submitted page changes take effect together when the page wraps. With other Wait modes, changes take effect at the selected boundary on the playing line, or when playback reaches another queued line. An invalid line prevents the whole page submission; correct the red-marked text and try again.

Enable **Apply valid edits while typing** in the context menu to submit edits automatically once the page validates. The Wait setting still determines when they become active; invalid edits leave playback unchanged.

The left wall shows selection and playback. A narrow yellow marker pulses at its inner edge when Draft edits or waiting Next changes exist, per line in channel view or per channel in All view. Right-click a row for **Stage changes** or **Unstage changes**. Unstage withdraws Next while keeping Draft and Now. **Revert changes** discards Draft edits and restores Next, or Now if nothing is queued. Changed Draft text is tinted yellow. Play stays raised while changes exist: steady yellow for Draft, pulsing yellow for Next. Applying Draft does not cancel an existing Next submission.

Press **F6** to switch between Draft, Next, and Now. Next and Now are read-only. Unchanged lines in Next show Now in faded text. Hold **F7** to peek at Now; release it to return. Typing returns to Draft. Switching views never replaces your edits.

The **x** button to the right of Stop acts on the selected channel:

- With Draft edits, discard them and restore Next, keeping Next queued.
- With no Draft edits, unstage Next and restore Now.
- **Shift-click** discards both Draft and Next at once.

**Esc** does the same for the focused line in a channel page, or the selected channel in All view. With no changes on the focused line, it returns to All view.

Use **Cmd/Ctrl+C**, **X**, and **V** to copy, cut, and paste selected text in either view. **Cmd/Ctrl+A** selects all text. In All view, selections can span channels; pasted lines go into successive channels, and cutting keeps the channel rows in place. Playing multi-line channels expose only their current formula in All view: you can copy it, but open the channel to cut or replace its text. An All-view cut or paste touching a read-only row leaves the selection unchanged.

Undo with **Cmd/Ctrl+Z**; redo with **Cmd/Ctrl+Shift+Z** or **Cmd/Ctrl+Y**. Undo changes Draft only. Submit restored text to apply it to playback.

Patches save Draft, Next, and Now separately. Reopening a patch keeps unfinished edits separate from playback and retains queued submissions. Wait uses its current setting, including if you change it after submitting.

## Controls

- **Run:** master playback enable.
- **Block / Line / Page:** momentary reset buttons for the current block, line, or whole channel page. Their inputs perform the same resets.
- **Time:** speeds up or slows down internal timing. Each +1 doubles the speed; each −1 halves it.
- **Seed:** positive values give repeatable random choices. Zero, or a negative combined value from CV, uses fresh randomness. The knob itself ranges from 0 to 10 and shows **Random** at zero. `$` blocks use any signed net seed, including zero; `!` blocks always use fresh randomness. Seed knob/CV changes are latched independently by each channel when its seeded pattern restarts: after its counted or timed repeat, or at the end of an enclosing `#` group. Resets and starting a pattern use the latest seed. With Seed CV patched, pattern restarts wait 1 ms before sampling the seed and choosing the first value. This lets a sequencer clocked by EOB, EOL, or EOP supply its new seed for the upcoming pattern.
- **Polyphony:** choose 1–16 output channels, or Auto. Auto extends to the highest channel with an activated formula. New modules default to Auto.
- **Edit/Ctrl:** switch between formula editing and keyboard/mouse control of channels.

### Gate length

The **Gate** knob ranges from −10 to 100 and defaults to 50%. Negative values produce 1 ms triggers; 0% produces no gate; positive values set the high time as a percentage of each clock interval. At 120 BPM, 50% gives a 250 ms gate. Random durations use their selected interval, and Speed of time scales internal gate lengths too.

At 0% the clock output stays low; at 100%, consecutive playing steps can join into one sustained gate. Rests and stopped channels stay low. Changes to Gate length apply on the next pulse. EOB, EOL and EOP remain short triggers.

External clocks use the last measured interval. Until two rising edges establish that interval, the output uses a short 1 ms pulse. Irregular external clocks are estimates; an arriving edge replaces the previous gate.

Gate length CV adds 10 percentage points per volt. The combined value is capped at 100%; any negative total selects Trigger. Mono CV affects every channel; polyphonic CV addresses matching channels, with missing channels adding zero. The tooltip shows channel 1’s combined value as **Gate length: Trigger** or a percentage.

## Inputs

| Input                   | Behavior                                                                                                                                                                                |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Run**                 | In Gate mode, above 1 V permits playback; low pauses it. In Trigger toggle mode, rising edges toggle playback. Unpatched permits playback while Run is on.                              |
| **Block / Line / Page** | Mono triggers (channel 1); rising edges around 1 V reset the corresponding playback scope. Resets follow the Run/transport state.                                                       |
| **Time CV**             | Mono, channel 1. Adds to Time at 1 V/octave. Combined range is −5 to +5: 1/32× to 32× speed.                                                                                            |
| **Seed CV**             | Mono, channel 1. Adds one seed unit per volt to the knob. Zero or negative totals select fresh randomness for unprefixed blocks. `$` uses the signed total as a seed; `!` ignores it.   |
| **Gate length**         | Adds 10 percentage points per volt to Gate. Mono broadcasts; poly addresses matching channels. Negative totals produce triggers; zero is silent; positive totals give gates up to 100%. |
| **W / X / Y / Z**       | Mono clocks (channel 1), rising edges around 1 V; referenced by letter in formulas or selected in the context menu.                                                                     |

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

The header’s **Play** button applies Draft edits and starts the selected channels. **Stop** stops them. In All view, click or drag across rows to select channels; Shift+click or Shift+Up/Down extends the selection. Shift and drag selections highlight text in green, including playing multi-line channels whose text cannot be edited. A plain click focuses a row without selecting its text. The left wall flashes with each playing channel’s clock; selected rows stay yellow and flash brighter.

Click **Edit/Ctrl** to enter Control mode. Click anywhere on Blunch to give it keyboard focus; a subtle orange stripe on the left marks focus. Hovering alone does not activate Control shortcuts. It opens All view, disables text editing, and selects the cursor’s channel. Text selections have a green background; the focused row has a thin yellow outline. At least one channel stays selected.

| Control                | Action                                                                                               |
| ---------------------- | ---------------------------------------------------------------------------------------------------- |
| Up / Down              | Move focus; a single-row selection follows the cursor, while a multiple-row selection stays selected |
| Shift+Up / Down        | Extend or shrink a range from the anchor row                                                         |
| Ctrl/Cmd+Enter         | Apply Draft and start the selected channels                                                          |
| Click                  | Replace the selection with the clicked channel                                                       |
| Ctrl/Cmd+click         | Add or remove the clicked channel                                                                    |
| Shift+click            | Select a range to the clicked channel                                                                |
| Ctrl/Cmd+,             | Stop the selected channels                                                                           |
| Space                  | Stop the selection if all selected channels are running; otherwise start the selection               |
| R                      | Reset selected channel pages to their first formula, keeping stopped channels stopped                |
| Enter in All view      | Stay in All view                                                                                     |
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

| Shortcut             | Action                                                                              |
| -------------------- | ----------------------------------------------------------------------------------- |
| Ctrl/Cmd+Enter       | All: apply and start selected channels; channel view: submit and start focused line |
| Ctrl/Cmd+Shift+Enter | Stage the changed lines on the page                                                 |
| F6                   | Cycle Draft / Next / Now                                                            |
| Hold F7              | Peek at Now                                                                         |
| Enter in All view    | Stay in All view                                                                    |
| Tab / Shift+Tab      | Switch between All and channel view                                                 |
| Esc                  | Cancel / return to All                                                              |
| Ctrl+[ / Ctrl+]      | Previous / next channel                                                             |
| Ctrl/Cmd+,           | Stop the selected channels                                                          |
| Ctrl+/               | Toggle Run                                                                          |
| Ctrl+.               | Stop all                                                                            |
| Ctrl/Cmd+Space       | Apply Draft and start all, bypassing Wait                                           |

The transport/navigation shortcuts labeled Ctrl use the Control key on macOS too. Standard text editing uses the usual platform modifier.

## Randomize

Rack Randomize replaces the selected line with a short clock formula. In All view it acts on the selected channel's displayed line. Polyphony and other controls stay as they are.

Examples favor 50–180 BPM and equivalent Hz or millisecond timings, with occasional sequences, repeats, random choices, and longer periods in seconds. Saved patches restore their existing formulas and settings.

For custom recipe collections, see [Random-template customization](development/blunch-random-templates.md).

## Panel layout

Choose **Editor → Theme** in the context menu: **Mauer** is light, **Garnett** uses solar colors, **Moss** keeps the original dark appearance, and **Puckett** is a cool dark variant. Themes change colors only and save with the patch. Existing patches use Moss.

The module starts at 16 HP. Drag either panel edge to resize it in 1 HP steps. The width is saved with the patch. Editor font, spacing, and line wrapping are available in the context menu. Text height and line height are independent: changing the glyph height does not move the rows. **Steady line height** is on by default, sizing 16 rows to the display. Turn it off to size rows from the font size and text height. **Editor line height** adjusts spacing only when Steady line height is off; the slider is disabled while it is on. Disabled channels retain alternating background stripes.

Choose **Panel → Max visual** in the context menu to expand the display vertically and collect the input/output jacks in a dense rail of tiny ports. Hover a jack for its label. Hidden controls keep their settings. Use **Normal UI** above the display, or **Panel → Normal**, to restore the controls and original module width. Cables remain connected, and the layout is saved with the patch.
