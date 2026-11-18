# Beekapoo

Beekapoo groups modules into a smaller panel. Expose the controls and jacks you need; the grouped modules keep playing.

## Quick start

1. Select two or more modules.
2. Right-click the selection and choose **Create Beek-A-Poo**. Undo restores the original modules and selection.
3. External connections appear on Beekapoo. Internal cables are hidden.
4. Press **Edit**, then **Expose**. Click parameters or jacks to add or remove them from the panel. In Expose mode, translucent green rectangles outline all exposed parameters, inputs and outputs. Ports with external connections remain locked and exposed. Hovering a control turns its overlay bright yellow.
5. Press **Done**. Drag either panel edge to resize; labels appear as space allows.

You can also add Beekapoo first, select the modules, and choose **Abstract selected modules** from its context menu.

## Editing

**Fade outside modules (global)** in the context menu dims modules outside the subpatch while editing. Set it from 0% (none) to 90%; the default is 45%. This preference applies to every Beekapoo and is remembered across Rack sessions.

**Edit** opens the original modules in the real rack. The first opening places them about four rows below Beekapoo, skipping occupied space. Reopening keeps their arrangement. Scrolling, shortcuts, selection, module movement, and patching work normally. Black computerscare logos on yellow markers identify modules in the subpatch; **Members** toggles these marks. **Panel** shows or hides the Beekapoo panel preview. **Near** and **Far** move the whole group close to Beekapoo or four rows away while preserving its layout.

**New: Inside** makes newly added modules join Beekapoo after placement. Switch to **New: Outside** to add modules to the main patch instead. Switching back does not capture modules already added outside. Select members and press **Remove** in the editing toolbar to release them into free space near Beekapoo. They leave the editing area, keep their cables, and support undo; the editor stays open. Deleting a member removes that module and its mappings; the rest of the group stays open. Expanders and nested Beekapoo groups are excluded. **Panel** shows or hides a preview of the compact panel. **Done** or Escape hides the group and returns to your previous view. Saving while editing preserves that session, including the return position.

**Eject** releases all contained modules into the rack and deletes Beekapoo. It reuses Beekapoo’s position when the complete group fits, otherwise choosing the closest free space above, below, or beside it. The group keeps its arrangement; other modules stay put. Undo restores the complete group. Ordinary Delete still deletes Beekapoo and its contained modules and cables. The **+** button appears when eligible modules outside the group are selected; press it to add them. **Add selected modules** and **Remove selected members** are also available in the context menu. Group operations, placement and exposure changes support undo/redo.

## Panel

Rack’s **Bypass** command bypasses every contained module. Switching it off un-bypasses every contained module, including any that were previously bypassed individually. Each module uses its normal bypass routing.

**Panel arrangement** groups controls by type or by source module. **Show controls** hides or shows parameters, inputs, outputs, or controls from one module. Hiding a control preserves its mapping and connections. Hover for the full control and module name. Port tooltips show the normal live voltage readings. At small widths, exposed parameter knobs fill available space without labels. Both Wiggle buttons and all module CV jacks stay visible at the minimum width. Randomize and Initialize buttons appear as the panel widens; Amount is hidden at small widths. Edit, Eject, all jacks, and the computerscare logo remain visible. Module actions keep their own box, separate from the exposed jacks.

Cables patched on Beekapoo’s front panel remain visible there, including connections between its own jacks. Internal wiring made while editing is hidden when you finish. Front-panel cable actions leave hidden wiring alone. **Disconnect cables** removes outside connections and visible front-panel cables; internal wiring stays connected.

## Copies and saved selections

**Duplicate** creates an independent copy, including contained modules, their settings, and internal cables. **Duplicate with cables** also copies incoming external connections. Cmd/Ctrl+D and Cmd/Ctrl+Shift+D over Beekapoo use the same options. Contained MIDI Map, stoermelder CV/MIDI mapping, and PatchMaster mappings using the standard mapping format are redirected to the copied modules. Other mapping formats may need to be reassigned.

Rack’s **Save selection as** saves a `.vcvs` selection file. Select Beekapoo to include its contained modules and internal wiring. Load that selection to reuse the group. Selection files do not bundle external sample files or other module assets; keep those files available separately. Loading a Beekapoo preset replaces its contents; Undo restores the previous group and its connections. If a preset cannot load, the current group stays intact.

## Randomization

The action grid has **Exposed** and **All** sets, each with Wiggle, Randomize, and Initialize buttons and trigger inputs. Exposed affects visible mapped parameters. Randomize and Wiggle ignore their normal randomization exclusions, but skip momentary buttons. All uses the grouped modules’ normal randomization behavior. **Amount** sets the wiggle range. Cmd/Ctrl+R randomizes visible mapped parameters.

Trigger inputs respond to rising signals of at least 1 V. These actions run at UI refresh rate, so rapid repeated triggers may combine. Manual button actions support undo.

Groups, exposed controls, panel arrangement, visibility, and width are saved with the patch. The former Peekaboo and Patch Abstractor names remain compatible with saved patches.
