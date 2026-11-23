# Beekapoo

Beekapoo groups modules into a smaller panel. Expose the controls and jacks you need; the grouped modules keep playing.

## Quick start

1. Select two or more modules.
2. Right-click the selection and choose **Create Beek-A-Poo**. Undo restores the original modules and selection.
3. External connections appear on Beekapoo, along with a few automatically chosen inputs, outputs, and parameters. Larger selections expose more controls. Internal cables are hidden.
4. Press **Edit**, then **Expose**. Click parameters or jacks to add or remove them from the panel. In Expose mode, translucent green rectangles outline all exposed parameters, inputs and outputs. Ports with external connections remain locked and exposed. Hovering a control turns its overlay bright yellow.
5. Press **Done**. Drag either panel edge to resize; labels appear as space allows.

By default, adding Beekapoo from the module browser creates a random subpatch of 3–11 VCV Free modules, with three inputs, one output, an attenuverter beside each input, and a few exposed parameters. Smaller sets are more common. Mixers use their Mix output first; only connected mixer channels and controls that affect a used output are exposed. Audio and MIDI interfaces are excluded. The panel starts compact; drag its edges to resize.

Select existing Beekapoos with other modules, or select several Beekapoos, then choose **Create Beek-A-Poo** to combine their contents. Connections and exposed controls carry over. Undo restores the separate Beeks.

Right-click the title to rename the subpatch. Press Enter to apply. Generated names change when its modules or internal wiring change. Names you enter stay fixed.

Edit and Eject sit in the header beside the computerscare logo. The strip below contains Generate new subpatch, Grow subpatch (the wavy plus), and the Wiggle/Randomize actions. Grow adds a cable or a module from the selected set, favoring VCA connections when available. Locked modules and their connections are left alone. Hover for each action’s name. Generate replaces the subpatch while keeping the same numbers of exposed parameters, inputs and outputs. Undo restores the previous patch and name. When randomization locks are set, it instead varies unlocked parameters and grows unlocked wiring in place. Existing exterior connections are retained by jack order. **E** opens editing while hovering Beekapoo; **E** or **X** switches Adjust/Expose while editing. Exposed controls grow to fit the available space. Narrow panels keep every parameter visible in a tighter layout and omit labels where necessary. Labels break only between words. Matching parameter names form compact groups under one shared label; numbered variants follow numeric order. Groups fit beside or above one another, with knob sizes adjusted to the available space. The manufacturers, module collection and connections choose varied control shapes and gray shading. Patches using the same manufacturers share an overall shape family. Related controls share a label and style; discrete knobs have dots and tick marks. **Panel arrangement → By module** keeps each module’s controls together instead.

When Rack’s CPU meters are enabled, the bottom-left readout shows the summed processing cost of the contained modules. Rack’s normal reading on the right measures Beekapoo itself. Narrow panels show `Σ` instead of “Subpatch”. This excludes Beekapoo’s own processing and graphics.

## Editing

**Rnd Lock** lets you click a module or parameter to protect it from Beekapoo’s Randomize and Wiggle actions, including CV. Gray padlocks mark protected controls. Module locks protect the whole module; parameter locks protect only that value. Click again to unlock, or choose **Unlock all** from the edit toolbar’s **...** menu. Locks are saved with the subpatch and support Undo. Initialize still works normally.

While editing, **Cmd/Ctrl+A** selects only the subpatch modules.

**Fade outside modules (global)** in the context menu dims modules outside the subpatch while editing. Set it from 0% (none) to 90%; the default is 45%. This preference applies to every Beekapoo and is remembered across Rack sessions.

**Edit** opens the original modules in the real rack. The first opening places them about four rows below Beekapoo, skipping occupied space. Reopening keeps their arrangement. Scrolling, shortcuts, selection, module movement, and patching work normally. Black computerscare logos on yellow markers identify modules in the subpatch; **Members** toggles these marks. **Panel** shows or hides the Beekapoo panel preview. **Near** and **Far** move the whole group close to Beekapoo or four rows away while preserving its layout.

**New: Inside** makes newly added modules join Beekapoo after placement. Switch to **New: Outside** to add modules to the main patch instead. Switching back does not capture modules already added outside. Select members and press **Remove** in the editing toolbar to release them into free space near Beekapoo. They leave the editing area, keep their cables, and support undo; the editor stays open. Deleting a member removes that module and its mappings; the rest of the group stays open. Expanders and nested Beekapoo groups are excluded. **Panel** shows or hides a preview of the compact panel. **Done** or Escape hides the group and returns to your previous view. Saving while editing preserves that session, including the return position.

**Eject** releases all contained modules into the rack and deletes Beekapoo. It reuses Beekapoo’s position when the complete group fits, otherwise choosing the closest free space above, below, or beside it. The group keeps its arrangement; other modules stay put. Undo restores the complete group. Ordinary Delete still deletes Beekapoo and its contained modules and cables. The header logo chomps when eligible modules outside the group are selected. Hover to make it chomp faster; click it to add the selection. Beekapoo itself can also be selected. **Add selected modules** and **Remove selected members** are also available in the context menu. Group operations, placement and exposure changes support undo/redo.

## Panel

Right-click the title to rename it. Type a name and press Enter, or Escape to cancel. Custom names keep their exact spelling and capitalization through randomization, subpatch generation, and save/reload. Renaming supports undo.

**Theme → Default / Light / Dark** changes this panel. Default follows the shared setting, or Rack’s panel theme. **Theme → All Beekapoos** switches every Beekapoo in the patch and sets the default for new ones; choose Default there to follow Rack again. Individual overrides are saved with the patch.

Discrete knobs use a dot indicator and tick marks. Buttons and switches keep their momentary, toggle, or multi-position behavior.

Rack’s **Bypass** command bypasses every contained module. Switching it off un-bypasses every contained module, including any that were previously bypassed individually. Each module uses its normal bypass routing.

**Panel arrangement** groups controls by type or by source module. **Show controls** hides or shows parameters, inputs, outputs, or controls from one module. Hiding a control preserves its mapping and connections. Hover for the full control and module name. Port tooltips show the normal live voltage readings. At small widths, exposed parameter knobs fill the available space. Input and output names stay visible, using shorter forms as space tightens: Frequency modulation → FM, Channel 12 CV → CV12. Hover for the full name. Dark mode uses darker attenuverters with light pointers. The module collection chooses uppercase or lowercase labels. Titles usually use lowercase, uppercase, or title case; mixed capitalization is rare. The compact action strip shows four Wiggle/Randomize buttons. Each action’s tiny trigger input sits directly below its button. All Wiggle/Randomize buttons use a contrasting shade to distinguish them from Exposed. Initialize buttons and their trigger inputs are hidden; saved connections still work. Edit, Eject, exposed jacks, and the computerscare logo remain visible. Module actions keep their own box, separate from the exposed jacks.

Cables patched on Beekapoo’s front panel remain visible there, including connections between its own jacks. Internal wiring made while editing is hidden when you finish. Front-panel cable actions leave hidden wiring alone. **Disconnect cables** removes outside connections and visible front-panel cables; internal wiring stays connected.

Exposed jacks preserve the contained modules’ native polyphony.

## Copies and saved selections

**Save Beek…** saves this Beekapoo and its contained modules as a standard `.vcv` patch, including internal wiring, exposed controls, panel settings, and module patch-storage assets such as Phlooper recordings. Outside modules and cables are excluded. Saving a Beek does not switch your current patch file. Open the exported file normally in Rack, or use **Load Beek…** to bring it into an existing Beekapoo.

**Load Beek…** accepts `.vcv` patches and `.vcvs` selections. If the file has several Beekapoos, choose one. A patch without Beekapoo is grouped on import. For VCV Audio 2/8/16 and stoermelder AUDIO-64, choose **Keep** or **Expose**. Expose removes the Audio interfaces. Each used audio input becomes one shared input feeding all its destinations, with one attenuverter. Outputs expose their original source jacks; repeated uses of the same source share one jack. A summed output uses a standard 8vert channel. Audio inputs routed straight to audio outputs also use 8vert. No custom audio-routing module is added. Polyphony and stacked cables are preserved; unused channels are omitted. Missing modules are reported before import. Replacing an existing group asks for confirmation and supports Undo.

Selections do not contain module patch-storage assets. Files referenced elsewhere on disk are not collected by Save Beek; recipients need those files and all required plugins installed.

Import choices open in a centered dialog. Click an option, then **Load**, or use Up/Down and Enter. **Cancel** or Escape leaves the current Beek unchanged.

**Duplicate** creates an independent copy, including contained modules, their settings, and internal cables. **Duplicate with cables** also copies incoming external connections. Cmd/Ctrl+D and Cmd/Ctrl+Shift+D over Beekapoo use the same options. Contained MIDI Map, stoermelder CV/MIDI mapping, and PatchMaster mappings using the standard mapping format are redirected to the copied modules. Other mapping formats may need to be reassigned.

Rack’s **Save selection as** saves a `.vcvs` selection file. Select Beekapoo to include its contained modules and internal wiring. Load that selection to reuse the group. Selection files do not bundle external sample files or other module assets; keep those files available separately. Loading a Beekapoo preset replaces its contents; Undo restores the previous group and its connections. If a preset cannot load, the current group stays intact.

## Randomization

**Edit → Module sets** chooses the modules used by Generate and Grow. Select a brand, then click module checkboxes to include or exclude them. **New** starts an empty set; **Copy** duplicates the current one. Edit the name directly. Sets are shared across all Beeks; each Beek keeps its own selected set. **Default for new Beeks** chooses what new browser instances use.

The initial set contains the usual Fundamental modules. Other modules use best-effort patching from their ports and tags; they may need adjustment afterward. Device-dependent modules, expanders and Beekapoo itself are unavailable. Missing plugins remain in saved sets and are skipped during generation. If a set cannot make a connected patch, the current subpatch stays unchanged. Set edits affect the next Generate or Grow action.

The action strip has **Shown/Exposed** and **All** sets, each with Wiggle and Randomize buttons. Exposed affects visible mapped parameters. Randomize and Wiggle ignore their normal randomization exclusions, but skip momentary buttons. All uses the grouped modules’ normal randomization behavior. **Wiggle amount** in the context menu sets the wiggle range. Cmd/Ctrl+R randomizes visible mapped parameters.

The action CV inputs include Generate and Grow. Existing connections to the hidden Initialize inputs remain active. These inputs respond to rising signals of at least 1 V. These actions run at UI refresh rate, so rapid repeated triggers may combine. Manual button actions support undo.

Groups, exposed controls, panel arrangement, visibility, and width are saved with the patch. The former Peekaboo and Patch Abstractor names remain compatible with saved patches.

Each exposed input has a regular computerscare attenuverter: center is zero, clockwise is positive, and counterclockwise inverts the signal. It scales every polyphonic channel. Internal signals and any remaining external signals sum at the original input. Adding a connected outside module makes its cables internal without changing their attenuation. An input remains exposed while an outside source uses it or its attenuverter is still needed.

In Edit, incoming cables follow the original module’s jack. Move or disconnect them there as usual; Done brings them back to the Beekapoo panel.

Eject keeps patched, non-unity input attenuation in 8vert modules, filling eight channels each. Shared inputs fan out to all their destinations. Unpatched imported audio inputs use a zeroed 8vert channel to preserve silence. Patched inputs at 100% stay directly connected; ordinary unpatched inputs need no 8vert channel. Consecutive channels with the same sources share 8vert’s normalized input. Combining Beeks carries their exposed input gains into the new Beek instead.

Hover the parameters, inputs, or outputs section for **−**, **dice**, and **+**. Plus exposes another item. Minus removes one eligible item at random. For inputs, unpatched inputs go first; otherwise it moves one input patched only from this Beek’s exposed outputs inside. Stacked cables stay connected, and an internal 8vert preserves any non-unity attenuation. Exposed outputs remain. Inputs with outside sources and locked controls stay in place. Dice chooses a different set of unpatched, unlocked controls without changing their count or values. Input attenuverter settings carry over. These actions support Undo. Joining Beeks or adding modules packs the subpatch into adjacent, balanced rows.
