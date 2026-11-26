# Beekapoo

Turn a patch into a module.

## Create

- From selected modules
- Random subpatch from the module browser
- Combine Beeks and other modules

## Edit

- Edit the original modules and cables
- Expose parameters, inputs, and outputs
- Add or remove modules
- Eject versus delete

## Panel

The visual look is completely determined by the manufacturers, modules, and internal cables of the subpatch.

- Resize and arrange controls
- Rename
- Light and dark themes
- Patching, polyphony, and MIDI mapping

## Randomization

- Wiggle and Randomize: exposed versus all
- Generate new subpatch
- Grow subpatch
- Module sets
- Lock parameters or whole modules

## Parameter steps

Save parameter snapshots and recall them manually or with a clock. Steps change parameter values, not modules or cables.

- **Capture New** adds a snapshot. **Update** replaces the current snapshot. **Recall** restores it.
- Edits affect the live patch without overwriting a snapshot. Changing steps replaces unsaved edits.
- **Delete** removes the current step. **Remove all steps** keeps the current sound as a single step.
- **Clock** enables external clock and scan. Manual advance and reset work independently. Scan is off by default; choose continuous or clocked operation.
- Steps include all inner parameters except momentary buttons. Locked and MIDI-mapped parameters are left unchanged during recall, randomize and wiggle.
- Saving the Rack patch preserves both the live parameter values and the snapshots.

## Actions CV

Each channel controls one action. A mono cable controls only channel 1.

| Channel | Action                      |
| ------- | --------------------------- |
| 1       | Wiggle exposed              |
| 2       | Randomize exposed           |
| 3       | Wiggle all                  |
| 4       | Randomize all               |
| 5       | Generate new subpatch       |
| 6       | Grow                        |
| 7       | Clock                       |
| 8       | Reset                       |
| 9       | Scan: 0–10 V selects a step |

## Save and share

- Save Beek / Load Beek
- Duplicate, with or without cables
- Presets and selections
- Samples and required plugins

## Shortcuts

Hold **~** for keyboard shortcuts.

- **Q / W**: wiggle all / exposed. **T / R**: randomize all / exposed.
- **Space**: clock on/off. **Z / X**: previous / next step. **B**: reset.
- **N**: Capture New. **S**: save current step. **V**: Recall. **Shift+D**: Delete step.
- **E**: Edit; **E / X** in Edit: Expose; **Esc**: Done.
- In Edit: **L** Lock, **P** Panel, **N** New Inside/Outside, **F** Far, **Shift+F** Near.
- **1–9, 0**: exposed buttons. **[ / ]**: switch banks of ten. Hold for momentaries.
