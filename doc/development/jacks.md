# Jack artwork

Jack names describe appearance, not signal direction. Any variant can be used
with `createInput` or `createOutput`. Pointing up/down refers to the pentagon's
point on the panel, not to voltage polarity or input/output routing.

| Component                      | SVG                                    | Size    |
| ------------------------------ | -------------------------------------- | ------- |
| `JackSmallPointingUp`          | `jack-small-pointing-up.svg`           | 16 × 16 |
| `JackSmallPointingDown`        | `jack-small-pointing-down.svg`         | 16 × 16 |
| `JackLargePointingUp`          | `jack-large-pointing-up.svg`           | 27 × 29 |
| `JackLargePointingDown`        | `jack-large-pointing-down.svg`         | 27 × 29 |
| `JackLargeOutlinePointingUp`   | `jack-large-outline-pointing-up.svg`   | 32 × 34 |
| `JackLargeOutlinePointingDown` | `jack-large-outline-pointing-down.svg` | 32 × 34 |

SVGs live in `res/components/`; shared components are in `src/Computerscare.hpp`.
Sizes are Rack pixels before any module-specific scaling. `Outline` means the
extra outer pentagon border. Regular variants have a single pentagon border.

The former names map directly to the same artwork:

| Former name                         | Current name                 |
| ----------------------------------- | ---------------------------- |
| `TinyJack`                          | `JackSmallPointingUp`        |
| `BlunchReverseTinyJack`             | `JackSmallPointingDown`      |
| `PointingUpPentagonPort`            | `JackLargePointingUp`        |
| `InPort`, `PhlooperRightOutputPort` | `JackLargePointingDown`      |
| `OutPort`                           | `JackLargeOutlinePointingUp` |

The large-outline pointing-down variant completes the set; no existing module
was switched to it. Existing modules retain their previous sizes and artwork.
