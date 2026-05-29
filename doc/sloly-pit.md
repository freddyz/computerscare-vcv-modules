# Sloly Pit

Polyphonic utility for splitting one polyphonic input into up to 16 outputs.

Sloly Pit can act as a simple polyphonic breakout, or it can build grouped and custom output routings. Each output can emit one or more polyphonic channels depending on the selected routing mode.

## Ports

**Poly in:** Polyphonic input signal to split or route.

**Randomize Trigger:** Trigger input for custom routing randomization. This only affects Custom routing mode. A monophonic trigger performs the same randomization as Rack's module Randomize command. A polyphonic trigger randomizes the matching custom route channel for each triggered poly channel across the output routes.

**1st through 16th outputs:** Routed outputs. Output channel counts depend on the selected routing mode and custom route contents.

## Routing Modes

Right-click the module and use **Routing Mode** to select the active routing behavior.

**Single:** Each output receives the matching input channel as a monophonic output. The 1st output receives input channel 1, the 2nd output receives input channel 2, and so on.

**Dynamic Below:** Connected outputs receive ranges of input channels from their output number up to the next connected output. If no later output is connected, the range continues to the end of the input's channel count.

**Dynamic Above:** Connected outputs receive ranges of input channels from the previous connected output up through their own output number.

**Custom:** Each output uses its stored custom route. A custom route can contain 0 to 16 input channel references, so an output can be silent, mono, or polyphonic.

## Custom Editing

In Custom mode, click an output's row to edit that output's route. The left column shows the selected output's custom route channels. Type or select channel references from the context controls to change the route.

Channel references use 1 through 9, 0 for channel 10, and a through f for channels 11 through 16.

## Set Custom Routing To

Right-click the module and use **Set Custom Routing to** to copy one of the generated routing modes into the stored custom routing.

- **Single:** Sets all 16 custom output routes to the matching single-channel route. This does not depend on which outputs are currently connected.
- **Dynamic Below:** Sets custom routes to the same routes that Dynamic Below would currently generate. If no outputs are connected, all custom routes are cleared.
- **Dynamic Above:** Sets custom routes to the same routes that Dynamic Above would currently generate. If no outputs are connected, all custom routes are cleared.

This changes the stored Custom routing but does not switch the active routing mode.

## Randomization

Randomization only changes routes in Custom mode.

The **Randomization (Custom mode only)** context-menu section controls how Rack's module Randomize command and the Randomize Trigger input choose custom routes.

**Input Pool:** Choose whether randomization picks only from currently active input channels or from all 16 possible input channels.

**Replacement:** Choose whether a route may reuse the same input channel, must avoid duplicate input channels, or shuffles the route's existing input channels.

**Channel Count:** Choose whether randomization keeps each output's current custom route length, picks a random route length, or sets a fixed route length.

**Output Targets:** Choose whether randomization changes only connected outputs or all outputs.
