# vcvmon

Reload the watcher after updating it:

```zsh
source ~/dev/computerscare-vcv-modules/scripts/vcvmon.zsh
```

Run from the plugin repository, or set `VCVMON_REPO`.

- `vcvmon` or `vcvmon dev`: builds plugins and Rack in the source checkout, then runs Rack.
- `vcvmon dist`: builds only this plugin against the Rack SDK, packages it, quits the installed Rack app, copies the package into its user plugin folder, then launches the installed app. This replaces computerscare without a backup and retains other installed plugins.

Space pauses/resumes automatic rebuilds, R rebuilds without changing the pause state, and Q quits the watcher and its Rack session. A failed build leaves the current Rack session running. If Rack cannot quit because a dialog is open, dismiss it and press R to retry. Edits made during a build are picked up on the next scan.

Rack sessions launched by the watcher show a small **vcvmon** toolbar in the upper right once a computerscare panel is loaded (opening the module browser also loads panels). It shows Watching, Paused, Changes queued, Building, or Build failed. **Pause/Resume** controls automatic rebuilding; **Rebuild** builds and restarts Rack. The controls do not affect audio transport. Normal Rack launches do not show the toolbar.

The toolbar uses `VCVMON_CONTROL`, set by the watcher, and small `.status` and `.command` files alongside `VCVMON_PAUSE` (default `/tmp/vcvmon.pause`). Removing the pause file from another terminal also rebuilds queued changes. The terminal keys and toolbar stay in sync. Restart the watcher after updating this script to enable the toolbar in the newly launched Rack session.

Defaults can be overridden before running:

```zsh
VCVMON_RACK="$HOME/dev/VCV-Rack/Rack"
VCVMON_SDK="$HOME/dev/VCV-Rack/Rack-SDK"
VCVMON_APP="/Applications/VCV Rack 2 Pro.app"
VCVMON_USER_DIR="$HOME/Library/Application Support/Rack2"
```

For an older Rack installation or a custom user folder, set `VCVMON_USER_DIR` to that folder. Dist mode currently targets macOS and chooses the native CPU architecture.

Run the isolated watcher tests with `zsh tests/vcvmon_test.zsh`. These use a temporary fake Rack app and plugin folder and do not touch the installed app.

The toolbar has **Save version**, **Save branch**, and **Options**. Both save actions switch to the new patch and add it to Recent Files.

- **Save version** increments only the trailing number: `horse.vcv` → `horse1.vcv`; `horse2-pig.vcv` → `horse2-pig1.vcv` → `horse2-pig2.vcv`. It uses the highest existing sibling number plus one.
- **Save branch** appends a short animal, plant, tree, geography, or household word: `horse2.vcv` → `horse2-elm.vcv`. From a root branch it creates a sibling: `horse2-pig.vcv` → `horse2-elm.vcv`. Numbered branches keep their full name: `horse2-pig1.vcv` → `horse2-pig1-elm.vcv`. Existing filenames are indexed in one folder scan. If all 150 single words are taken, names concatenate words, such as `horse2-horseflax.vcv`, then longer combinations if needed.
- **Save in patch folder** defaults on. Opening `patchxyz.vcv` and using either action creates `patchxyz/` beside it. All subsequent saves stay in that single folder. Opening `test1.vcv` starts a `test1/` folder; its next numbered save is `test1/test2.vcv`. The original file stays untouched.
- A small `.computerscare-patch-folder.json` marker keeps a folder recognizable after moving or reopening it. Saves within it do not create nested folders. Parent-name associations are no longer needed.
- Turn folder mode off in **Options** to save beside the current file. This preference survives Rack restarts. An unnamed patch first prompts for its initial save; canceling cancels the operation.
