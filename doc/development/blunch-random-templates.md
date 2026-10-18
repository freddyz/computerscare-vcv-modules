# Blunch random-template customization

Advanced reference for changing the recipe catalogue. For playing and editing sequences, see the [Blunch user reference](../blunch.md).

Edit [res/programs/blunch-random.json](../../res/programs/blunch-random.json) to change the recipes. Blunch reads this file for each new module and each Randomize, so no C++ edits or recompilation are needed. When running an installed plugin, edit the copy in that plugin’s `res/programs` directory. Keep changes in your source copy too, since installing a new plugin version replaces its resources.

The file contains:

- `startup`: an expression that produces 1–4 formula lines. Its `wpick` weights favor fewer lines.
- `randomize`: an expression that produces one formula line.
- `definitions`: named strings or arrays of `{ "weight": ..., "template": ... }` choices. Larger weights make a choice more frequent; zero disables it. Weights need not sum to 100.

The numeric definitions (`tempo`, `milliseconds`, `hertz`, and `seconds`) favor familiar values, with less frequent arbitrary values and rare extra decimals. Adjust their weighted choices to change that balance. Add a weighted entry to `line` to introduce a new pattern, or change its weight to adjust how often it appears. `clock` controls the simple clock vocabulary; `modified` supplies short `@` and `#` examples. Most randomized lines use `modified`, and startup always begins with one. `line` also allows occasional plain clocks and pairs.

Counted-repeat examples use two successive clocks or a compound choice, such as `80bpm@3 120bpm@2` or `{100|120}bpm@5`. Timed repeats use a duration, such as `120bpm@3s`. Inner cycles usually contain two values and occasionally three. Some use bracketed runs, such as `(2 [3 1])hz#6`, which play the whole run on its turn. These recipes are available at startup too. Other short recipes demonstrate ranges (`{100-120}bpm@4`), nested cycles with either group position (`(2 (3 1))hz@9` or `((3 1) 2)hz#6`), and mixed units (`440ms 1.1hz@2`).

The small template language is independent of Blunch’s clock parser:

| Expression                     | Result                                               |
| ------------------------------ | ---------------------------------------------------- |
| `(nrange 50 180)`              | Random integer, including both endpoints             |
| `(frange 0.84 3 2)`            | Random decimal, formatted to two decimal places      |
| `(pick 2 3 4 6 8)`             | One discrete choice                                  |
| `(wpick 6 1 4 2 2 3 1 4)`      | One value from weight/value pairs                    |
| `(ref tempo)`                  | Expand a named definition or weighted recipe         |
| `(repeat "(ref tempo)" 2 " ")` | Two independently drawn tempos, separated by a space |

`(distinct "(ref tempo)" 2 "|")` draws different values, for choices such as `{100|120}bpm`. It retries duplicates up to 64 times per value and reports an error if the recipe cannot produce enough distinct results.

Literal text surrounds expressions: `[(nrange 333 1000) (nrange 333 1000)]ms` can become `[397 602]ms`. Quote arguments containing spaces; `\n` inside a quoted argument inserts a newline (written as `\\n` inside a JSON string). Templates can nest. Escape literal parentheses with a backslash, doubled for JSON.

Only the chosen branch is expanded, and each repetition draws fresh values. Expansion is deterministic for a supplied entropy seed; the module draws fresh entropy for each generation. The panel Seed control continues to govern randomness inside running Blunch formulas.

Expansions are bounded to 24 nesting levels, 16 repetitions per call, 4096 evaluation steps, and 4096 output bytes. Cyclic references, invalid expressions, and invalid generated formulas report an error in the module context menu and Rack log. Failed Randomize leaves the existing formula intact; startup falls back to `120bpm` if its recipes cannot load or generate valid formulas.
