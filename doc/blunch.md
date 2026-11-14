# Blunch

Blunch is a text-based clock and trigger sequencer. Offers somewhat of a "live coding" language for generating trigger sequences. Fully polyphonic, with up to 16 independent clocks. A simple starter formula would be `109bpm`, which will emit a 109bpm clock. It can get as complicated as you like, eg: `90bpm@3 120bpm@4` which emits 3 triggers at 90bpm, and then 4 at 120. `{3hz|5hz}#4` will randomly choose between 3hz and 5hz each clock, and will emit an "End-of-Block" (EOB) trigger every 4 clocks.

## Quick start

1. Connect **out** to a trigger or clock input.
2. Select **Ch 1** in the top-right channel selector.
3. Write `120bpm` and press **Ctrl+Enter** or **Cmd+Enter** to submit and start the line.
4. Try `[300 400]ms` for alternating 300 ms and 400 ms intervals.
5. Add another formula on a new line. With automatic Line advance, Blunch plays each line in turn and loops the page.

The **Run** button enables or disables playback.

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

Decimals can begin with a dot: `.5s` and `0.5s` mean the same thing. The context menu’s **Default number unit** selects BPM, seconds, milliseconds, Hz, minutes, millihertz, or kilohertz for bare numbers. BPM is the initial setting. Explicit units always override it. The setting applies when a line is next submitted; existing Next and Now formulas keep their units. Clock values must be positive. Units are case-insensitive. `s`, `sec`, and `seconds` are equivalent; `m`, `min`, and `minutes` are equivalent. Additional unit spellings include `rpm` for BPM, `cps` for Hz, `mhz` for millihertz, and `khz` for kilohertz.

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

Equivalent to: `[1 3 2 4 1 5 2 3 1 4 2 5]s`. It repeats when both inner sequences return to their beginnings.

A parenthesized sequence immediately following a block alternates with that preceding block. For example, `500ms (300ms 400ms)` plays `500, 300, 500, 400` ms.

Use brackets for a straight sequence; use nested parentheses when items should advance on successive turns. Inside brackets, a nested parenthesized group advances once per pass: `[90 100 110 (120 144)]` plays `90 100 110 120`, then `90 100 110 144`. A bracketed sequence plays all its values when its turn arrives: `(1 (2 3 [3 2 1]))hz` plays `1, 2, 1, 3, 1, 3, 2, 1`, then repeats.

### Repeats: `@`

| Formula             | Meaning                                                            |
| ------------------- | ------------------------------------------------------------------ |
| `120bpm@4`          | Play four pulses at 120 BPM before advancing                       |
| `120bpm@2s`         | Keep this clock for two seconds                                    |
| `120bpm@{2\|4\|8}`  | Randomly choose a repeat count of 2, 4, or 8                       |
| `120bpm@(2 4)`      | Alternate repeat counts of 2 and 4 on successive visits            |
| `[300 400]ms@2`     | Repeat each value twice: 300, 300, 400, 400 ms                     |
| `120bpm@(2 (3 4))`  | Alternate repeat counts 2, 3, 2, 4 on successive visits            |
| `[120 91 62]@[2 3]` | Repeat each clock twice on the first pass, three times on the next |

Square brackets also work for repeat sequences: `120bpm@[2 4]` is equivalent to `120bpm@(2 4)`. `[120 91 62]@[2 3]` gives each clock two pulses on its first visit, three on its next, then alternates.

Nested repeat arguments follow the same interleaving rules as clock sequences: `@(2 (3 4))` selects **2, 3, 2, 4**. A bracketed child plays its whole sequence on its turn: `@(2 [3 4])` selects **2, 3, 4**.

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

| Formula             | Meaning                                             |
| ------------------- | --------------------------------------------------- |
| `{90\|120\|150}bpm` | Randomly choose one of three tempos                 |
| `{90-150}bpm`       | Randomly choose a value in the range                |
| `{300ms\|.5s\|2hz}` | Randomly choose between clocks with different units |
| `{2hz\|~2hz}`       | Randomly choose a clock or a rest                   |

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

In All view, right-click a row to play or stop that channel, or copy its displayed formula. In channel view, right-click a formula to play from that line or copy it.

You can edit a channel in All view when it is stopped or paused, has only one formula line, or **Line advance** is **Off**. Otherwise, open its channel page to edit. Channels above the selected polyphony are faded but remain editable.

### Editing during playback

With **Auto-stage valid edits** on, valid edits are submitted automatically. **Wait** selects when they enter playback. Invalid edits leave playback unchanged.

For example, play `120bpm`, then change it to `90bpm`. With **Wait → Next visit**, the current visit finishes before the new tempo starts. Editing another line does not jump to it.

To prepare a change manually, turn Auto-stage off. Edit the formula, then right-click and choose **Stage changes**, or press **Ctrl/Cmd+Shift+Enter** to stage the channel page. With **Wait → Page**, the changes start together when the page repeats. Correct any red-marked errors before submitting.

To start a chosen line immediately, select it and click **Play**. In All view, Play starts the selected channels at their displayed lines. **Ctrl/Cmd+Enter** also submits and starts playback, using Wait for pending changes.

Turning **Run** back on applies valid edits immediately and resumes the remembered channels. **Ctrl/Cmd+Space** applies valid edits and starts all channels.

### Draft, Next, and Now

| View      | Meaning                              |
| --------- | ------------------------------------ |
| **Draft** | The formulas you are editing.        |
| **Next**  | Submitted formulas waiting to start. |
| **Now**   | The formulas currently playing.      |

Press **F7** to cycle through these views. Next and Now are read-only; typing returns to Draft. Patches save all three views.

**Unstage changes** removes Next but keeps your Draft. **Revert changes** restores Draft from Next, or from Now if nothing is waiting.

The **x** button beside Stop discards Draft edits first. If there are no Draft edits, it cancels Next. **Shift-click** discards both. **Esc** does the same for the focused line, or the selected channel in All view. With no changes on the focused line, Esc returns to All view.

Use the usual copy, cut, paste, and select-all shortcuts. In All view, copied or pasted lines span successive channels. Open a playing multi-line channel to cut or replace its text.

Undo with **Cmd/Ctrl+Z**; redo with **Cmd/Ctrl+Shift+Z** or **Cmd/Ctrl+Y**. Undo changes Draft. Auto-stage and Wait control when the restored text enters playback.

## Controls

- **Run:** master playback enable.
- **Block / Line / Page:** momentary reset buttons for the current block, line, or whole channel page. Their inputs perform the same resets.
- **Time:** speeds up or slows down internal timing. Each +1 doubles the speed; each −1 halves it.
- **Seed:** positive values give repeatable random choices. Zero, or a negative combined value from CV, uses fresh randomness. The knob itself ranges from 0 to 10 and shows **Random** at zero. `$` blocks use any signed net seed, including zero; `!` blocks always use fresh randomness. Seed knob/CV changes are latched independently by each channel when its seeded pattern restarts: after its counted or timed repeat, or at the end of an enclosing `#` group. Resets and starting a pattern use the latest seed. With Seed CV patched, pattern restarts wait 1 ms before sampling the seed and choosing the first value. This lets a sequencer clocked by EOB, EOL, or EOP supply its new seed for the upcoming pattern.
- **Polyphony:** choose 1–16 output channels, or Auto. Auto extends to the highest channel with an activated formula. New modules default to Auto.
- **Edit/Set:** switch between formula editing and Settings (F8).

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
| **W / X / Y / Z**       | Clock: channel 1, rising edges around 1 V. Scan: 0–10 V sampled at advance boundaries; mono broadcasts, poly addresses matching channels.                                               |

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

## Channel selection and transport

Each channel’s started/stopped state is saved with the patch. Master Run and Run CV still control whether started channels can play.

The header’s **Play** button applies Draft edits and starts the selected channels. **Stop** stops them. In All view, click or drag across rows to select channels; Shift+click or Shift+Up/Down extends the selection. Ctrl/Cmd+click toggles a channel in the selection. Shift and drag selections highlight text in green, including playing multi-line channels whose text cannot be edited. A plain click focuses a row without selecting its text. The left wall flashes with each playing channel’s clock; selected rows stay yellow and flash brighter.

Ctrl/Cmd+Enter applies and starts the selection; Ctrl/Cmd+, stops it. Tab switches between All and channel view. Escape returns to All. Selection stays intact when switching views. Ordinary text editing remains available wherever the row is editable.

## Settings

Press **F8** with Blunch focused or hovered, or click **Edit** to open Settings. Settings fill the display while playback continues. Settings navigation is keyboard-only. Use **Up/Down** to move, **Enter/Space** to open or select, and **Escape** to go back. Choices take effect immediately; `[x]` marks the selected option. Escape at the top, F8, or clicking **Set** returns to Edit.

Settings include line/block advance, Wait, Run input, live apply, channel sync, default units, and Stage page. Appearance and editor options remain in the context menu. Entering and leaving Settings uses a 300 ms character transition. Formula text and selection are preserved.

### Help

Open **Settings → Help** for Overview, Controls, and Language. Use Up/Down to scroll descriptions. Language topics contain examples from this reference; highlight an example and press Enter to copy the formula, then Escape or F8 back to the editor and paste it. Copying an example does not change or start a formula.

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
- **Wait:** applies submitted text immediately, at a Clock/Block/Line/Page boundary, on an external W/X/Y/Z edge, or on each edited line’s Next visit.
- **Line scan / Block scan:** Off advances sequentially. W/X/Y/Z samples 0–10 V to select a line/block at the configured advance boundary.
- **Auto-stage valid edits:** stages valid edits automatically; Wait controls activation.
- **Run input:** choose Gate or Trigger toggle behavior.

## Channel sync

The top **Sync** dropdown and context menu’s **Channel sync** share the same settings. **Free** is the default: channels run independently.

Choose a source channel, its **End of Block**, **End of Line**, or **End of Page** boundary, and where followers restart: **Current block**, **Current line**, or **Page beginning**. Each source boundary restarts the other playing channels. Longer formulas are interrupted; shorter ones can loop between restarts. Stopped channels stay stopped. If the source is stopped or outside the output polyphony, followers continue independently.

For example, use `120bpm#4s` on Channel 1, choose Channel 1’s End of Line, and restart followers at Current line. Their lines restart together every four seconds, regardless of their individual rates or random choices. This aligns restart points; it does not stretch formulas to fit.

Sync uses Now and leaves Draft untouched. Next retains its usual Wait behavior. A patched Seed CV keeps its 1 ms sampling grace period without adding time to the shared pattern boundary. Sync settings are saved with the patch.

## Keyboard shortcuts

**F8** opens or closes Settings.

Hold **backtick** while Blunch is hovered or focused to show its shortcut popup. Release to hide it. Editing is blocked while the popup is visible. **Shift+backtick types `~`**, the rest operator.

| Shortcut             | Action                                                                              |
| -------------------- | ----------------------------------------------------------------------------------- |
| Ctrl/Cmd+Enter       | All: apply and start selected channels; channel view: submit and start focused line |
| Ctrl/Cmd+Shift+Enter | Stage the changed lines on the page                                                 |
| F7                   | Cycle Draft / Next / Now                                                            |
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

## Scanning lines and blocks

In Playback settings, choose **Line scan** or **Block scan**, then W, X, Y, or Z.
0–10 V selects across the available lines or blocks. The existing **Line advance**
and **Block advance** settings determine when the voltage is sampled: Automatic
waits for completion; External waits for that clock; Off holds the current selection.
Voltage changes between boundaries do not interrupt playback. Mono scan voltage
controls every channel; polyphonic scan voltage addresses matching channels.
Blank lines are skipped. Scan Off keeps sequential advancement.

## Bake Time into formulas

Choose **Bake Time into formulas** in the context menu (also in Settings →
Playback) to fold the Time knob into all channels’ Draft, Next, and Now formulas,
then reset Time to zero. CV is ignored and remains patched. At Time +1, for example,
`100bpm` becomes `200bpm` and `500ms` becomes `250ms`. Timed `@` and `#` limits
also scale; repeat counts, probabilities, and external-clock references do not.
Values use six significant digits without trailing zeros. Colon times become
seconds. Correct invalid drafts before baking. Playback keeps its position and
current random choices. The action supports Undo/Redo.
