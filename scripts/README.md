# scripts/

Helpers for development. The split is between things that work in a sandboxed
environment without the VCV Rack SDK (so an AI agent or a fresh CI runner
can use them) and things that need the SDK installed.

## In a sandbox / agent loop

```sh
scripts/sandbox-check.sh
```

Fast (~5s), no network, no SDK. Catches the kinds of regressions that break
an agent loop:

- `plugin.json` is invalid JSON
- A module slug in `plugin.json` has no matching `createModel<>("slug")`
- A `p->addModel(modelFoo)` refers to an undefined `Model*`
- An `asset::plugin(pluginInstance, "res/foo.svg")` reference points to a
  file that no longer exists
- An SVG or `.vcvm` preset doesn't parse
- A C++ file fails the preprocessor (mismatched `#if`, stray characters,
  bad relative `#include`)

What it does **not** catch: type errors, undefined symbols, link errors.
Those require a real build against the SDK.

Set `CHECK_FMT=1` to also fail on `clang-format` violations (off by default,
since most of the legacy code isn't formatted yet).

The CI workflow `.github/workflows/sandbox-check.yml` runs this on every push.

## Getting the SDK locally

```sh
scripts/fetch-rack-sdk.sh
export RACK_DIR=$HOME/Rack-SDK
```

The script tries `vcvrack.com` first (canonical SDK with prebuilt deps);
if that's blocked (common in sandboxes), it falls back to cloning the
`VCVRack/Rack@v2` source tree from GitHub. The GitHub fallback is
**headers-only** — `dep/include` is empty because the dep submodules
(nanovg, glfw, glew, …) aren't bundled in the source tarball, so an
actual `make` will fail. Use it for IDE indexing or syntax checks; for
real builds, get the official SDK.

## With the SDK

```sh
scripts/dev.sh sandbox     # same as scripts/sandbox-check.sh
scripts/dev.sh fmt         # clang-format -i
scripts/dev.sh fmt-check   # clang-format --dry-run --Werror
scripts/dev.sh lint        # clang-tidy
scripts/dev.sh build       # make -j
scripts/dev.sh test        # build then run /tmp/cs_test
scripts/dev.sh check       # fmt-check + lint + build
```

`lint`, `build`, `test`, and `check` require `RACK_DIR` to be set. `fmt`,
`fmt-check`, and `sandbox` do not.
