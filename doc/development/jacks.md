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

## Perspective jacks

Include `PerspectivePentagonJack.hpp` for `JackPerspectiveRaisedPointingUp`,
`JackPerspectiveRaisedPointingDown`, `JackPerspectiveRecessedPointingUp`, and
`JackPerspectiveRecessedPointingDown`. These draw flat shaded faces like the
warped buttons, with a centered cable socket and a 27 × 29 footprint.

Use `configure(profile, pointingUp, angleDegrees, depth, distortion, seed)`.
The angle describes the perspective line clockwise in screen coordinates:
0° right, 90° down, 180° left, 270° up. The default is 135° (down-left),
with depth 3 and distortion 0.6. Each instance gets a stable random variation. Pass a nonzero seed for a reproducible shape.
Raised faces extrude toward that direction; recessed faces expose an inner well.
Changing perspective does not move the cable connection point.

The perspective variants are available for future layouts. Windchimes and the
other modules currently use the regular flat jack styles.

The pentagonal surround varies per instance, while the socket and its rim remain circular. Recessed sockets sit down the perspective line on the well floor; the near walls can slightly obscure the circular rim.
