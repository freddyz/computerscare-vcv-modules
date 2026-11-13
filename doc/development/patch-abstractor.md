# Patch Abstractor

On the `patch-abstractor` branch, add **Patch Abstractor** from the module browser.
Select the modules to hide, right-click Abstractor, and choose **Abstract selected modules**.
The selection can contain modules anywhere in the patch.

Connected external ports appear on Abstractor. Internal cables disappear; their engine connections remain intact.
Use **Expose port** to add an extra jack, and **Expose parameter** to add controls targeting original parameters. Standard momentary buttons, toggles, and multi-position switches retain their behavior; knobs remain knobs. Custom widgets without switch metadata fall back to a discrete switch for snapped quantities or a knob otherwise.
There is no fixed exposed-control limit; the panel widens as needed.
The top grid has **Visible** and **All** rows for **Wiggle**, **Randomize**, and **Initialize**, each with a mono trigger input.
Visible affects only exposed parameters. All randomizes/initializes each member using its normal Rack behavior; Wiggle All adjusts its eligible parameters.
**Amount** sets the maximum wiggle as a percentage of each parameter's range. Mod+R randomizes Visible.
Button actions are undoable. CV actions run on the next UI update without adding undo entries; repeated pulses within one update are combined.
**Expand** places the members together in one selected block of the rack and keeps them in the abstraction.
Patch and adjust them there, then press **Collapse** to hide the group again.
**End abstraction** releases the members from either state. Removing Abstractor also releases them.
Use **Add selected modules** for an outside selection, or **Remove selected members** for a selection in the expanded block.
Adding/removing membership, expanding/collapsing, and ending the abstraction support undo and redo, including positions and selection.

Grouping, original positions, exposed ports, and exposed parameters are saved with the patch.
Reopening restores the saved expanded/collapsed state after Rack has loaded its modules and cables.
Hidden module widgets keep their original dimensions inside zero-size registered holders, so their previous rack space can be reused.
Expansion finds a free continuous block on the same row. Expanded/collapsed state and member positions survive save/reload.

**Show** opens the grouped modules without expanding them. The two buttons at the top switch modes:

- **Adjust subpatch:** turn knobs and use Rack’s normal cable gestures between internal jacks. Right-click a jack for its cable menu.
- **Expose:** click a knob or jack to toggle its presence on the abstraction. Green marks exposed controls.

Yellow jacks have external connections and are locked in both modes. Closing the view or pressing Escape keeps the abstraction intact.
The view has independent zoom and scrolling. Use **− / +**, the percentage button for 100%, **Fit**, or Cmd/Ctrl + mouse wheel. Panels are arranged without overlap; cables and plugs use Rack's renderer. Tooltips retain the original labels, values, descriptions, and enabled/disabled setting. Its member positions are fixed. Change membership using the rack selection commands.
Grouping actions and internal cable edits support Rack's standard undo and redo shortcuts.

Expanders and nested groups are not implemented.
Duplicating only the container creates an unbound copy; it cannot take over the original group.
A missing member causes the available members to expand rather than remain hidden.
Expanded members retain a transparent registered parent until the patch is reopened; arbitrary third-party widget compatibility still needs testing.
Try cable-connected modules in a disposable patch first. Arbitrary third-party widget compatibility is not established.

## Offline checks

`scripts/preview-module.sh patch-abstractor` runs an isolated Rack-engine fixture and renders the actual container widget.
It checks a non-contiguous selection, four-channel audio, unchanged engine cable identities, hidden internal cables,
a visible external cable, an extra unpatched input, and a parameter mapped to the original DSP.
It places another module into the freed space, saves and destroys the patch, reloads through Rack's native engine/widget JSON paths,
and checks restored grouping, ports, parameters, audio, and zero-size footprints.
It also reloads 25 exposed ports and 24 parameters, checks duplicate prevention and control bounds, and checks expansion around occupied space, background interaction on expanded modules, copied-container isolation,
missing-member recovery, container/patch deletion, and native Select All → Delete.
It checks internal repatching and undo/redo, external-cable and exposure locks, exposure toggling,
undo/redo of grouping and expansion, and both subview modes with actual computerscare module widgets.
It does not operate the running Rack application or vcvmon.

Full, Med, and Small change exposed-control spacing. Full keeps complete labels; Med uses small labels; Small hides labels. Hover identifies the source module, with instance numbers when a model appears more than once.

The subpatch UI uses the existing Rack instance through public container APIs. During a native callback or draw, a scoped adapter lends it the subpatch’s visual proxies, then restores its original containers, parent, bounds, and touched parameter. It never constructs a private `RackWidget` or creates a second audio engine.
