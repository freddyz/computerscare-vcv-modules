# Peekaboo

Peekaboo groups modules into a smaller panel. Expose the controls and jacks you need; the grouped modules keep playing.

## Quick start

1. Add Peekaboo.
2. Select the modules to group, then choose **Abstract selected modules** from Peekaboo’s context menu.
3. External connections appear on Peekaboo. Internal cables are hidden.
4. Press **Show**, choose **Expose**, and click parameters or jacks without external connections to add or remove them from the panel.
5. Drag either panel edge to resize. Labels appear as space allows; hover for the full control and module name.

## Editing the group

**Show → Adjust subpatch** lets you change controls, move modules, and patch internal cables. Yellow jacks have external connections and are locked.

Use **Add module** or double-click empty space to open the module browser. Shift-click or Cmd/Ctrl-click modules to select several. Right-click a module and choose **Remove from abstraction** to return it, or the selection, to the main rack without deleting it.

**Expand** returns the group to the rack in a selected block. **Collapse** hides it again. **End** releases the group. Grouping, movement, and removal support undo/redo.

## Randomization

The action grid has **Exposed** and **All** sets, each with Wiggle, Randomize, and Initialize buttons and trigger inputs. Exposed affects mapped parameters; All affects the grouped modules. **Amount** sets the wiggle range. Cmd/Ctrl+R randomizes exposed parameters.

Trigger inputs respond to rising signals of at least 1 V. These actions run at UI refresh rate, so rapid repeated triggers may combine. Manual button actions support undo.

Groups, exposed controls, panel width, and subpatch positions are saved with the patch. Expanders and nested Peekaboo groups are not supported.
