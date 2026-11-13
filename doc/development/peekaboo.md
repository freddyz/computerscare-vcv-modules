# Peekaboo

On the `patch-abstractor` branch, add **Peekaboo** from the module browser.
Select the modules to hide, right-click Peekaboo, and choose **Abstract selected modules**.
The selection can contain modules anywhere in the patch.

Connected external ports appear on Peekaboo. Internal cables disappear; their engine connections remain intact.
Use **Expose port** to add an extra jack, and **Expose parameter** to add controls targeting original parameters. Standard momentary buttons, toggles, and multi-position switches retain their behavior; knobs remain knobs. Custom widgets without switch metadata fall back to a discrete switch for snapped quantities or a knob otherwise.
There is no fixed exposed-control limit; the minimum width grows when needed to keep every control accessible.
The top grid has **Exposed** and **All** columns for **Wiggle**, **Randomize**, and **Initialize**, each with a mono trigger input.
Exposed affects only exposed parameters. All randomizes/initializes each member using its normal Rack behavior; Wiggle All adjusts its eligible parameters.
**Amount** sets the maximum wiggle as a percentage of each parameter's range. Mod+R randomizes exposed parameters.
Button actions are undoable. CV actions run on the next UI update without adding undo entries; repeated pulses within one update are combined.
**Expand** places the members together in one selected block of the rack and keeps them in the abstraction.
Patch and adjust them there, then press **Collapse** to hide the group again.
**End abstraction** releases the members from either state. Removing Peekaboo also releases them.
Use **Add selected modules** for an outside selection, or **Remove selected members** for a selection in the expanded block.
Adding/removing membership, expanding/collapsing, and ending the abstraction support undo and redo, including positions and selection.

Grouping, original positions, exposed ports, and exposed parameters are saved with the patch.
Reopening restores the saved expanded/collapsed state after Rack has loaded its modules and cables.
Hidden module widgets keep their original dimensions inside zero-size registered holders, so their previous rack space can be reused.
Expansion finds a free continuous block on the same row. Expanded/collapsed state and member positions survive save/reload.

**Show** opens the grouped modules without expanding them. The two buttons at the top switch modes:

- **Adjust subpatch:** turn knobs, patch internal jacks, and drag modules on the Rack grid. Shift-click or Cmd/Ctrl-click toggles module selection; drag a rectangle on empty rack space to select several. Cmd/Ctrl+A selects all.
- **Add module:** opens Rack’s module browser. You can also double-click empty rack space or use its right-click menu. The added module becomes a member automatically.
- Right-click a module and choose **Remove from abstraction** to return it to the main rack. If several modules are selected, the command removes the selection. Their audio and cables are retained. Moving and removing support undo/redo.
- **Expose:** click a knob or jack to toggle its presence on the abstraction. Green marks exposed controls.

Yellow jacks have external connections and are locked in both modes. Closing the view or pressing Escape keeps the abstraction intact.
The view has independent zoom and scrolling. Use **− / +**, the percentage button for 100%, **Fit**, or Cmd/Ctrl + mouse wheel. Panels are arranged without overlap; cables and plugs use Rack's renderer. Tooltips retain the original labels, values, descriptions, and enabled/disabled setting. Subpatch positions are saved independently of main-rack expansion positions. Rails cover the whole viewport and the workspace grows as modules move.
Grouping actions and internal cable edits support Rack's standard undo and redo shortcuts.

Expanders and nested groups are not implemented.
Duplicating only the container creates an unbound copy; it cannot take over the original group.
A missing member causes the available members to expand rather than remain hidden.
Expanded members retain a transparent registered parent until the patch is reopened; arbitrary third-party widget compatibility still needs testing.
Try cable-connected modules in a disposable patch first. Arbitrary third-party widget compatibility is not established.

## Offline checks

`scripts/preview-module.sh peekaboo` runs an isolated Rack-engine fixture and renders the actual container widget.
It checks a non-contiguous selection, four-channel audio, unchanged engine cable identities, hidden internal cables,
a visible external cable, an extra unpatched input, and a parameter mapped to the original DSP.
It places another module into the freed space, saves and destroys the patch, reloads through Rack's native engine/widget JSON paths,
and checks restored grouping, ports, parameters, audio, and zero-size footprints.
It also reloads 25 exposed ports and 24 parameters, checks duplicate prevention and control bounds, and checks expansion around occupied space, background interaction on expanded modules, copied-container isolation,
missing-member recovery, container/patch deletion, and native Select All → Delete.
It checks internal repatching and undo/redo, external-cable and exposure locks, exposure toggling,
undo/redo of grouping and expansion, and both subview modes with actual computerscare module widgets.
It does not operate the running Rack application or vcvmon.

Drag either panel edge to resize. Width is saved with the patch. The panel adapts automatically: dense layouts use tiny jacks, small knobs and icon buttons without labels; wider layouts add short labels, full labels and module names as space permits. Every exposed control and all six action CV inputs remain available. The minimum width depends on how many controls must fit. Tooltips identify source modules, with instance numbers when a model appears more than once.

Responsive thresholds and control budgets live in `src/PeekabooLayout.hpp`; placement is calculated only on resize or mapping changes. Audio processing is unchanged.

The subpatch UI uses the existing Rack instance through public container APIs. During a native callback or draw, a scoped adapter lends it the subpatch’s visual proxies, then restores its original containers, parent, bounds, and touched parameter. It never constructs a private `RackWidget` or creates a second audio engine.

The preview also renders 16 layout cases (sparse, skewed, medium and crowded mappings) into `spectrum-16.png`, four row strips and individual images. Each case checks that mapped controls stay inside the front faces of their perspective blocks.

The title has its own fitted header box. Module controls use a capped-width matrix: Exposed and All are columns, actions are rows. At the narrowest widths the sets stack in one column. Parameters can occupy the space beside the controls when that reduces the required height.

The former `computerscare-patch-abstractor` slug is registered as a hidden compatibility model so saved patches continue to load. New instances use `computerscare-peekaboo`.
