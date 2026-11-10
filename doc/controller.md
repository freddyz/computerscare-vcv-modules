# Controller

DualShock 4, DualSense and Xbox Series controllers to CV. Pair the controller in your computer's Bluetooth settings, then choose it in the module's device menu. Auto uses the first supported controller. USB connections also work.

Each control has its own output. Four polyphonic banks group the main buttons, other buttons, continuous controls/status, and touch/motion signals. Channel order is listed below. Button banks follow each jack’s Gate/Trigger/Toggle setting. Xbox controllers have no touchpad or motion sensors; those outputs remain at 0 V.

## Buttons

Right-click a button output to choose:

- **Gate:** 10 V while pressed.
- **Trigger:** a pulse on each press. Set its duration in the module menu.
- **Toggle:** each press switches between 0 V and 10 V.

L2/R2 (LT/RT) have both continuous outputs and threshold button outputs. Threshold sets their switching point. Face buttons are digital, not pressure-sensitive. Touch 1 and Touch 2 indicate finger contact, not pressure.

## Continuous outputs

| Output         | Range                                      |
| -------------- | ------------------------------------------ |
| Stick X/Y      | −5 to +5 V; up and right are positive      |
| L2/R2 or LT/RT | 0–10 V                                     |
| Touch X/Y      | 0–10 V; right and up are positive          |
| Gyroscope      | 5 V per π radians/second, limited to ±10 V |
| Accelerometer  | 5 V per 2 g, limited to ±10 V              |
| Battery        | 0–10 V for 0–100%; 0 V when unavailable    |
| Connected      | 10 V while connected                       |

Dead zone removes small stick movements around the center. Slew smooths continuous outputs. Touch coordinates hold their last position when a finger lifts; use the contact output to gate them. Center subtracts the current motion readings, including gravity at the controller's current orientation.

Disconnecting clears every output and toggle. Button modes and the selected device are saved in the patch; toggle states are not.

## Offset and scale

The panel groups buttons, continuous controls, touch and motion signals. Each control has its own labeled jack.

Right-click any named jack for its **Scale** (0–2×) and **Offset** (−10 to +10 V). The output is `control voltage × scale + offset`. Defaults are 1× and 0 V. Button mode is applied before scaling; bank outputs follow the same settings. Unsupported controls and disconnected controllers remain at 0 V regardless of offset.

## Rumble inputs

**Low rumble** and **High rumble** independently control the two vibration intensities. Both inputs use 0–10 V: 0 V stops that vibration, 10 V is full strength. Negative voltages are treated as 0 V; voltages above 10 V are limited to full strength. These are mono inputs.

Unpatching both inputs stops rumble. Rumble also expires when Rack stops processing. Controller, driver and connection support may vary. If several modules target the same controller, the most recent patched module controls rumble.

## Panel

The 15 HP panel uses tiny labeled jacks. All individual outputs, banks and rumble inputs remain available. Button labels light green when pressed or latched on. Hover a jack for its full name; right-click for offset, scale and button mode. Dead zone, threshold and slew have knobs at the top. Center resets the motion baseline. These controls are also in the module menu.

**Set all** at the top changes every button output to Gate, Trigger or Toggle. Individual jack menus can override it afterward. Changes can be undone.

Right-click a jack and choose **Reset offset and scale** to restore that control to 0 V offset and 1× scale. The module menu has **Reset all offsets and scales**. These resets leave button modes unchanged and can be undone.

## Polyphonic banks

Button banks follow the per-button mode, offset and scale settings.

| Channel | Buttons 1       | Buttons 2      |
| ------- | --------------- | -------------- |
| 1       | Cross (×)       | Touchpad click |
| 2       | Circle (○)      | Mute           |
| 3       | Square (□)      | Create         |
| 4       | Triangle (△)    | Options        |
| 5       | D-pad Up        | PS             |
| 6       | D-pad Down      | —              |
| 7       | D-pad Left      | —              |
| 8       | D-pad Right     | —              |
| 9       | L1              | —              |
| 10      | L2 threshold    | —              |
| 11      | L3              | —              |
| 12      | R1              | —              |
| 13      | R2 threshold    | —              |
| 14      | R3              | —              |
| 15      | Touch 1 contact | —              |
| 16      | Touch 2 contact | —              |

The Controls bank carries LX, LY, RX, RY, L2, R2, Battery and Connected on channels 1–8. The Touch/Motion bank carries Touch 1 X/Y, Touch 2 X/Y, Gyro X/Y/Z and Accel X/Y/Z on channels 1–10.

Each bank sits beneath its matching controls. Touch coordinates are grouped with motion because they share that bank. Poly1/Poly2 are the two button banks; the other banks are labeled Poly.
