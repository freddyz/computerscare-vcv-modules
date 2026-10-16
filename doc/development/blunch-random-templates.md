# Blunch random-template customization

Advanced reference for changing the recipe catalogue. For playing and editing sequences, see the [Blunch user reference](../blunch.md).

Edit [res/programs/blunch-random.json](../../res/programs/blunch-random.json) to change the recipes. Blunch reads this file for each new module and each Randomize, so no C++ edits or recompilation are needed. When running an installed plugin, edit the copy in that plugin’s `res/programs` directory. Keep changes in your source copy too, since installing a new plugin version replaces its resources.

The file contains:

- `startup`: an expression that produces 1–4 formula lines. Its `wpick` weights favor fewer lines.
- `randomize`: an expression that produces one formula line.
- `definitions`: named strings or arrays of `{ "weight": ..., "template": ... }` choices. Larger weights make a choice more frequent; zero disables it. Weights need not sum to 100.

For example, change `tempo` from `(nrange 50 180)` to `(nrange 90 140)` to narrow the BPM range. Add a weighted entry to `line` to introduce a new pattern, or change its weight to adjust how often it appears. `clock` controls the simple clock vocabulary; `line` adds sequences and other structures.

The small template language is independent of Blunch’s clock parser:

| Expression                     | Result                                               |
| ------------------------------ | ---------------------------------------------------- |
| `(nrange 50 180)`              | Random integer, including both endpoints             |
| `(frange 0.84 3 2)`            | Random decimal, formatted to two decimal places      |
| `(pick 2 3 4 6 8)`             | One discrete choice                                  |
| `(wpick 6 1 4 2 2 3 1 4)`      | One value from weight/value pairs                    |
| `(ref tempo)`                  | Expand a named definition or weighted recipe         |
| `(repeat "(ref tempo)" 2 " ")` | Two independently drawn tempos, separated by a space |

Literal text surrounds expressions: `[(nrange 333 1000) (nrange 333 1000)]ms` can become `[397 602]ms`. Quote arguments containing spaces; `\n` inside a quoted argument inserts a newline (written as `\\n` inside a JSON string). Templates can nest. Escape literal parentheses with a backslash, doubled for JSON.

Only the chosen branch is expanded, and each repetition draws fresh values. Expansion is deterministic for a supplied entropy seed; the module draws fresh entropy for each generation. The panel Seed control continues to govern randomness inside running Blunch formulas.

Expansions are bounded to 24 nesting levels, 16 repetitions per call, 4096 evaluation steps, and 4096 output bytes. Cyclic references, invalid expressions, and invalid generated formulas report an error in the module context menu and Rack log. Failed Randomize leaves the existing formula intact; startup falls back to `120bpm` if its recipes cannot load or generate valid formulas.
