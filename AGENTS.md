# Project Instructions

- After code changes, run the strongest validation available and fix any errors before finishing:
  - Lightweight fallback for sandboxed/cloud agents or missing SDK: `scripts/dev.sh sandbox`
  - `scripts/sandbox-check.sh` is the same lightweight fallback and is not a replacement for a Rack SDK build.
- `scripts/dev.sh` requires a subcommand. Run `scripts/dev.sh help` to list commands.
- See `AGENTS.local.md` for local dev-server assumptions and machine-specific paths.
