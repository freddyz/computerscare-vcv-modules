# Project Instructions

- After code changes, run the strongest validation available and fix any errors before finishing:
  - `scripts/dev.sh check`
  - format code after changes with `scripts/dev.sh fmt`
  - Lightweight fallback for sandboxed/cloud agents or missing SDK: `scripts/dev.sh sandbox`
  - `scripts/sandbox-check.sh` is the same lightweight fallback and is not a replacement for a Rack SDK build.
- `scripts/dev.sh` requires a subcommand. Run `scripts/dev.sh help` to list commands.
- See `AGENTS.local.md` for local dev-server assumptions and machine-specific paths.
- Remember Rack module widgets are constructed with `module == nullptr` in the module browser; guard all widget/render-time module access.
- For each new module, create an SVG panel matching the existing modules. Use the distorted SVG text generator for labels and footer lettering, include a unique distorted computerscare logo and wordmark, and use the box generator for irregular pseudo-3D frames.
- Avoid generic, vibe-coded aesthetics; follow the established computerscare plugin style for layouts, controls, typography, and artwork.
- For quick visual checks, use `scripts/preview-module.sh phlooper --browser` or `scripts/preview-module.sh blunch`. This renders the actual null-module browser widget using Rack's graphics libraries. `scripts/preview-module.sh phlooper` also renders simulated playback states. Repeated runs reuse the compiled runner; `--no-build` skips the plugin build. Images and binaries live in ignored `build/previews/<module>/`. Requires a built local Rack source checkout and macOS graphics access; no audio device or existing patch is opened. See `scripts/preview/README.md` for options.
