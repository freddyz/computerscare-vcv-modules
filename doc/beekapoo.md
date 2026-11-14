# Beekapoo

Beekapoo groups modules into a smaller panel. Expose the controls and jacks you need; the grouped modules keep playing.

## Quick start

1. Add Beekapoo.
2. Select the modules to group, then choose **Abstract selected modules** from Beekapoo’s context menu.
3. External connections appear on Beekapoo. Internal cables are hidden.
4. Press **Edit**, then **Expose**. Click parameters or jacks to add or remove them from the panel. Green-and-white markers show exposed controls. Yellow markers identify ports with external connections; those remain exposed. Hovering a control shows a translucent highlight.
5. Press **Done**. Drag either panel edge to resize; labels appear as space allows.

## Editing

**Edit subpatch** reveals the original modules in the real rack, about four rows below Beekapoo. Occupied rows are skipped. Scrolling, shortcuts, selection, module movement, and patching work normally.

Modules added while editing join this Beekapoo after placement. Deleting a member removes that module and its mappings; the rest of the group stays open. Expanders and nested Beekapoo groups are excluded. **Panel** shows or hides a preview of the compact panel. **Done** or Escape hides the group and returns to your previous view. Saving while editing preserves that session, including the return position.

**Expand** returns the group to the rack in a selected block. **Collapse** hides it again. **End** releases the group without deleting its modules. Deleting Beekapoo deletes its contained modules and cables; Undo restores the complete group. Use **Add selected modules** or **Remove selected members** to change membership. Group operations support undo/redo.

## Panel

**Panel arrangement** groups controls by type or by source module. **Show controls** hides or shows parameters, inputs, outputs, or controls from one module. Hiding a control preserves its mapping and connections. Hover for the full control and module name. Port tooltips show the normal live voltage readings. At small widths, exposed parameter knobs fill available space without labels. Module action buttons and Amount are hidden; their CV jacks stay visible. Edit, Expand, End, all jacks, and the computerscare logo remain visible. Module actions keep their own box, separate from the exposed jacks.

Cables patched on Beekapoo’s front panel remain visible there, including connections between its own jacks. Internal wiring made while editing is hidden when you finish.

## Copies and saved selections

**Duplicate** creates an independent copy, including contained modules, their settings, and internal cables. **Duplicate with cables** also copies incoming external connections. Cmd/Ctrl+D and Cmd/Ctrl+Shift+D over Beekapoo use the same options.

Rack’s **Save selection as** saves a `.vcvs` selection file. Select Beekapoo to include its contained modules and internal wiring. Load that selection to reuse the group. Selection files do not bundle external sample files or other module assets; keep those files available separately.

## Randomization

The action grid has **Exposed** and **All** sets, each with Wiggle, Randomize, and Initialize buttons and trigger inputs. Exposed affects visible mapped parameters; All affects the grouped modules. **Amount** sets the wiggle range. Cmd/Ctrl+R randomizes visible mapped parameters.

Trigger inputs respond to rising signals of at least 1 V. These actions run at UI refresh rate, so rapid repeated triggers may combine. Manual button actions support undo.

Groups, exposed controls, panel arrangement, visibility, and width are saved with the patch. The former Peekaboo and Patch Abstractor names remain compatible with saved patches.
