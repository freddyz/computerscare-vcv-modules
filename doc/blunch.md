## Blunch

Text-based polyphonic trigger sequencer.

### Run

The **Run** button is the master transport enable. When Run is off, Blunch is stopped and the Run input is ignored.

When Run is on, the **Run input** can control playback:

- With no cable connected, Blunch runs normally.
- In **Gate** mode, high voltage runs playback and low voltage stops playback. Gate is the default mode.
- In **Trigger toggle** mode, each rising edge toggles playback on or off.

Right click the module and use **Run input** to choose **Gate** or **Trigger toggle**.

The Run input supports polyphony. A monophonic cable controls all active channels. A polyphonic cable controls matching channels individually, so channel 1 controls sequencer channel 1, channel 2 controls sequencer channel 2, and so on. If the Run input has fewer polyphonic channels than Blunch is outputting, channels without a matching Run input channel stop while the cable is connected.
