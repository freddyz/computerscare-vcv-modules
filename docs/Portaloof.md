# Portaloof

A real-time screen feedback and glitch effect module. Each frame, Portaloof captures the current VCV Rack window as a texture, applies a chain of geometric and color transforms, and draws the result back into its display area — creating a recursive feedback loop.

The module is resizable (drag the left or right edge).

You can also **drop an image file** (PNG/JPG/JPEG/BMP) onto the module to use it as the source instead of the live screen capture. An image can also be loaded or cleared from the right-click menu.

---

## Global Controls (header row)

### CONT — Continuous / Triggered mode

- **Button (toggle):** On = continuous (effect updates every frame). Off = triggered (effect only updates on a trigger event).
- **Gate jack:** High voltage (>0.5 V) forces continuous mode; low forces triggered mode. Overrides the button when connected.

### TRIG — Trigger / Image Gate

- **Triggered mode:** Button press or rising edge on the trigger jack fires one capture frame.
- **Continuous mode (with a loaded image):** A high button or input (>0.5 V) holds the loaded image as the source while held. Releasing returns to live screen capture.

---

## Effect Rows

Each row has: **toggle button** · **gate jack** (overrides toggle) · **CV jack** · **attenuverter knob** · **value knob**

The gate jack overrides the toggle when connected (high = on, low = off).  
The CV jack modulates the knob value: `final = knob + (attenuverter × CV × scale)`, clamped to the row's range.

---

### SCALE — Uniform Scale

Zooms the image in both X and Y equally.  
Range: **0.1 – 4.0** (default 1.0 = no zoom)

### SCL X — Scale X

Independent horizontal zoom with exponential mapping and axis-mirror support.  
Knob range: **-5.0 – +5.0** (default 1.0 = no zoom)

- `|knob| ≤ 1` → scale from 0.1 to 1.0 (quadratic, fine control near center)
- `|knob| > 1` → scale from 1.0 to 5.0 (exponential, coarser at extremes)
- Negative values mirror the X axis.

### SCL Y — Scale Y

Independent vertical zoom. Same mapping as SCL X; negatives mirror the Y axis.  
Knob range: **-5.0 – +5.0** (default 1.0 = no zoom)

> SCALE, SCL X, and SCL Y multiply together. To zoom only one axis, disable the others.

### ROT — Rotation

Rotates the image around the center of the display area.  
Range: **-180° – +180°** (default 0 = no rotation)

### KALI — Kaleidoscope

Selects a mirror symmetry mode. The image is divided into sectors and mirrored to fill the display.  
Range: **0 – 12** (integer steps; 0 = off)

| Mode | Pattern                                 |
| ---- | --------------------------------------- |
| 0    | Off                                     |
| 1    | Left / right mirror                     |
| 2    | Top / bottom mirror                     |
| 3    | Classic quad (4-fold)                   |
| 4–12 | Various multi-sector and strip patterns |

### TRN X — Translate X

Pans the image horizontally. Value is a fraction of the image width.  
Range: **-1.0 – +1.0** (default 0 = centered)

### TRN Y — Translate Y

Pans the image vertically. Value is a fraction of the image height.  
Range: **-1.0 – +1.0** (default 0 = centered)

---

## Color Rows

### HUE — Hue Shift

Rotates all colors around the hue wheel.  
Range: **-180° – +180°** (default 0 = no shift)

### WARP — Solarize / Posterize

Applies a nonlinear tone curve to the image.

- Positive values: posterize + contrast crush
- Negative values: solarize (partial inversion)  
  Range: **-1.0 – +1.0** (default 0 = no effect)

> **FOLD (chromatic divergence)** is no longer on the panel — it lives in the right-click menu as a wide **Fold Frequency** slider. Knob range **0.0 – 1.0** maps internally to fold frequency **1.0 – 4.0** (1.0 = no effect). The FOLD row's toggle, gate, and CV inputs still exist and still gate/modulate the effect.

---

## Context Menu

### Load image… / Clear image

Load a still image (PNG/JPG/JPEG/BMP) to use as the source in place of the live screen capture. `Clear image` returns to live capture. You can also drag-and-drop an image file onto the module.

### Fold Frequency _(slider)_

The FOLD knob, exposed here as a wide slider. Controls chromatic-channel divergence (RGB split). Knob **0.0 – 1.0** → fold frequency **1.0 – 4.0** (1.0 = no effect).

### Render as rack background _(checkbox)_

When enabled, the effect renders as a full-screen backdrop behind all modules in the rack in addition to the module's own display area. The backdrop uses its own independent screen capture and trigger state.

On **Initialize**, this is reset to off.

### Transform post _(checkbox)_

Controls how knob changes behave in **triggered mode** (no effect in continuous mode).

- **Checked (post):** After a snapshot, the transform parameters remain live — moving knobs continuously alters the frozen image in real time.
- **Unchecked (pre):** After a snapshot, transform parameters are frozen at the moment the trigger fired. Moving knobs has no visible effect until the next trigger.

Applies to both windowed and full rack background modes.

---

### Full Rack BG _(submenu)_

Options that apply when **Render as rack background** is enabled.

#### Empty module window _(checkbox)_

When checked, the module's own display area is left transparent while the backdrop is active — the rack background effect shows through. When unchecked, the module window renders the effect as normal alongside the backdrop.

Default: on.

#### Backdrop Alpha _(slider)_

Opacity of the full-rack backdrop layer.  
Range: **0.0 – 1.0** (default 0.85)

At 1.0 the backdrop fully covers the rail background. Lower values blend the effect with the rail/background behind it, which also affects how strongly the feedback loop builds.

---

### Visual _(submenu)_

#### Tile empty space _(checkbox)_

When the effect image doesn't fill the entire display area (due to rotation, scale, or translation), tiling repeats the image to cover the gap instead of leaving it dark.

#### Maintain aspect ratio _(checkbox)_

Sizes the captured image to match the screen's natural aspect ratio within the display area. Without this, the capture is stretched to fill the display width.
