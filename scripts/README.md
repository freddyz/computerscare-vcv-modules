# vcvmon

Reload the watcher after updating it:

```zsh
source ~/dev/computerscare-vcv-modules/scripts/vcvmon.zsh
```

Run from the plugin repository, or set `VCVMON_REPO`.

- `vcvmon` or `vcvmon dev`: existing development workflow, using `make plugins run` in the Rack source checkout.
- `vcvmon dist`: builds only this plugin against the Rack SDK, packages it, quits the installed Rack app, copies the package into its user plugin folder, then launches the installed app. This replaces computerscare without a backup and retains other installed plugins.

Space pauses/resumes automatic rebuilds, R rebuilds, and Q quits the watcher and its Rack session. A failed dist build leaves Rack running. If Rack cannot quit because a dialog is open, dismiss it and press R to retry. Edits made during a build are picked up on the next scan.

Defaults can be overridden before running:

```zsh
VCVMON_RACK="$HOME/dev/VCV-Rack/Rack"
VCVMON_SDK="$HOME/dev/VCV-Rack/Rack-SDK"
VCVMON_APP="/Applications/VCV Rack 2 Pro.app"
VCVMON_USER_DIR="$HOME/Library/Application Support/Rack2"
```

For an older Rack installation or a custom user folder, set `VCVMON_USER_DIR` to that folder. Dist mode currently targets macOS and chooses the native CPU architecture.

Run the isolated watcher tests with `zsh tests/vcvmon_test.zsh`. These use a temporary fake Rack app and plugin folder and do not touch the installed app.
