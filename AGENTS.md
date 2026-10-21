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
- For quick visual checks, use `scripts/preview-module.sh phlooper --browser`, `scripts/preview-module.sh blunch`, `scripts/preview-module.sh 32nd-and-columbus`, or `scripts/preview-module.sh toly-pensor`. This renders the actual null-module browser widget using Rack's graphics libraries. `scripts/preview-module.sh phlooper` also renders simulated playback states. Repeated runs reuse the compiled runner; `--no-build` skips the plugin build. Images and binaries live in ignored `build/previews/<module>/`. Requires a built local Rack source checkout and macOS graphics access; no audio device or existing patch is opened. See `scripts/preview/README.md` for options.

- Include a render-and-inspect loop when adjusting panel layouts remotely or when the user requests screenshots: build the changes, render with the preview script, open the image yourself, and correct overlaps, clipping, spacing, and alignment before presenting it. Repeat after layout corrections. When the user is actively monitoring Rack locally, use judgment about how often a separate render helps. Preserve the intentional distorted artwork while keeping controls and labels readable and aligned.

- When the user asks for a screenshot, regenerate it with the preview script and inspect the fresh render before showing it. Do not reuse an earlier image or maintain a latest-screenshot shortcut directory. Reuse the compiled runner to keep rendering fast.

- When working remotely on visual module changes, generally finish by sending fresh, inspected screenshots inline so the user can see the result without asking again. After completing preview-script work, show a freshly generated example as well.

- Use appearance-based jack names: `JackSmallPointingUp/Down`, `JackLargePointingUp/Down`, and `JackLargeOutlinePointingUp/Down` (spell out the full class name in code). `Outline` means an extra outer pentagon border. Names do not imply input/output routing. See `doc/development/jacks.md`; preserve a module’s chosen artwork when refactoring.

- For this UI session, show a freshly rendered, inspected screenshot after every UI change, including when working locally. Vary representative module parameters and arrangements between captures so screenshots exercise different states; preserve the actual browser preview when that is specifically requested.
