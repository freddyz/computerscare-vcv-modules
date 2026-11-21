Computerscare Text / Computerscare Display

Derived from Barlow Medium and Barlow SemiBold by Jeremy Tribby and the
Barlow Project Authors. Licensed under SIL OFL 1.1; see OFL.txt.
Modified 2026 Adam Malone: deterministic distorted outlines, four variants
per character, renamed font families, fixed variant advances and new metrics.
The original Barlow font files are in scripts/fonts/sources/ for regeneration.
Source: https://github.com/google/fonts/tree/main/ofl/barlow
Original font checksums and generated font checksums: manifest.json.

Both files are ordinary installable TrueType fonts. Each covers 523 Unicode
characters, with four outlines per character. Standard Unicode uses the first
outline. OpenType stylistic sets ss02, ss03 and ss04 select the other three.
The private-use mappings are an internal rendering detail for Rack/NanoVG.

Change scripts/fonts/profiles.json to adjust the two distortion profiles.
The generator reuses scripts/svg_text_randomize.py. Install the development
dependency with:

  python3 -m pip install -r scripts/fonts/requirements.txt

Then run:

  python3 scripts/fonts/build.py
  python3 scripts/fonts/build.py --check

The normal plugin build uses the supplied TTF files; it needs no Python font
tools and never generates lettering at runtime. Altered glyphs retain each
character's advance width. Keep the license and this notice with the fonts.
