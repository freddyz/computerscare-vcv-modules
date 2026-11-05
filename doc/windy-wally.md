# Windy Wally

Stereo wind and a wind-strength CV output. Uses the same wind model as [32nd & Columbus](32nd-and-columbus.md).

## Controls

- **Strength:** Base wind strength.
- **Gustiness:** Variation in the breeze.
- **Turbulence:** Irregular movement.
- **Tone:** Balance of low wind and brighter passing air.
- **Texture:** Swells, buffeting, and resonances.
- **Presence:** Fills in quiet airflow and adds close buffeting.
- **Width:** Mono at zero; full stereo at 100%.
- **Overdrive:** Smooth saturation, with level compensation.
- **Gust:** Adds a gust. Its strength follows Strength.

Howls and whistles emerge from the wind settings; there is no separate control.

## Inputs

All inputs use the first channel of a cable.

| Input                                                                      | Range and behavior                                                                          |
| -------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| Strength, Gustiness, Turbulence, Tone, Texture, Presence, Width, Overdrive | 10 V adds the full knob range. Negative voltage subtracts. The result is limited to 0–100%. |
| Gust                                                                       | A rising trigger at 1 V adds a gust.                                                        |

## Outputs

- **L / mono, R:** Stereo audio. Either output alone carries mono wind. Audio is limited to ±5 V.
- **Wind CV:** Current wind strength, 0–10 V. Works with the audio outputs disconnected. Overdrive and Width do not affect it.
