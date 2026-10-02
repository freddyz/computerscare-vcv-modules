# Windchimes

Windchimes creates a stereo scene of wind-driven tuned chime sets. Start with the
included metal set, connect **L / mono** and **R** to your mixer, and let the breeze
move its striker. Turn **Wind mix** down for chimes without audible wind.

## Arrange the scene

- **+ Add** creates a set, up to eight. Each set has one to twelve tubes.
- Click a set to select it. Drag it horizontally to change its stereo position;
  drag it down to bring it closer and increase its level. Each set is a stereo
  point source: its horizontal position determines the pan of all its tubes.
- **Remove** deletes the selected set. Add, remove, edits, and moves support Rack's
  undo/redo. Patch files save every set and its settings.
- **Strike**, or double-clicking a set, pushes its suspended striker toward the
  tubes. Audio starts on physical contact. Tubes hang from a shared support on
  fixed-length strings, swing within bounded arcs, and can strike one another.
  Struck tubes briefly brighten; a matching flash marks striker impacts. The **Gust** button at the lower left pushes all sets.

The highlighted set uses the material and scale menus and the **Tubes**, **Root**,
**Register**, **Fine**, **EDO**, **Decay**, **Brightness**, **Hardness**, **Set level**,
and **Swing** controls. Hover over a knob to see its value; right-click
for exact entry. Material choices are metal, wood/bamboo, and plastic.

**EDO** means equal divisions of the octave (5–24). Scale degrees are mapped from
standard twelve-tone scales to the nearest step of the selected division. **Root**
uses standard semitones, **Register** moves by octaves, and **Fine** is in cents.
This version does not import Scala files or use arbitrary ratio tunings.

## Wind and connections

**Wind**, **Gustiness**, and **Turbulence** control a shared changing weather field.
Each set has different pendulum timing; collision strength determines strike
volume. **Swing** controls the pendulum's speed and damping. **Wind mix** controls
only the audible filtered-noise wind, and **Output** controls the final mix.

- **Wind CV**: 0–10 V adds to the Wind knob; negative CV can reduce it.
- **1V/oct**: transposes all tubes.
- **Gust**: a rising trigger pushes every set, including with Wind at zero.
- **Wind out**: 0–10 V wind-strength signal for controlling other modules.
- **L / mono**, **R**: stereo audio. Leaving R unpatched mixes both sides into L.

A bounded output stage prevents dense scenes from producing excessive voltages.
**Dry/wet** blends the complete scene into a stereo reverb. **Size** changes the
room's dimensions and tail length, from a short space to a large ambient wash.
The stereo L/R outputs are grouped together at the lower right.

**Wind tone** moves from deep rumble to bright, airy noise. **Texture** adds
rustling and a shifting breathy howl. These controls shape the audible wind;
the Wind, Gustiness, and Turbulence controls still determine physical movement.

## Implementation

The engine is original modal synthesis with eight damped modes per tube and
material-specific decay and excitation. It shares no Mutable or Audible code.
Wind, tuning, pendulum motion, resonators, reverb, and scene mixing are separate components
in `src/Windchimes/`. Storage is fixed at eight sets and twelve tubes each; DSP
performs no allocation, locks, file access, or UI calls. Motion runs at approximately
400 Hz and parameter updates at approximately 200 Hz. Resonator coefficients are
cached and decayed voices stop processing. Tube-to-tube collision checks run at the motion rate. The simulation and renderer
share the same suspended-capsule geometry, string lengths, and striker size.
Manual strikes apply a physical push; resonators are excited only by contacts.
Visual snapshots are published after the corresponding audio/physics update. The reverb uses fixed delay
buffers and an eight-line feedback network with stereo input diffusion. New effect
controls are appended to the parameter list to preserve older patch settings.
UI motion snapshots and strike requests
use atomics; editable settings use Rack parameters.

The tubes hang around a circular support. The central striker and each tube swing in two horizontal axes with fixed cord lengths. Impacts dissipate momentum, and gravity plus damping return the assembly to rest when wind stops. Click the view label at the top left of the scene to switch between the 2D top view and the pseudo-3D view. Both show the same simulated geometry; the 3D view draws distant bodies first.

Shape and Body are continuous per-set controls on every material. Shape morphs solid bar (0%), compact block (33%), open shell (67%), and hollow tube (100%); these are sound-design profiles rather than exact geometry measurements. Body blends dry contact with sustained resonance. Hardness controls the finite contact pulse duration; Brightness controls modal and impact-noise color. Wood starts darker and loses upper modes faster than metal. Decay maps logarithmically to a main-mode T60 of 0.05–12 seconds for wood, 0.12–24 seconds for metal, and 0.07–16 seconds for plastic. Voices sleep when total residual energy falls below the quietness threshold, with no fixed tail cutoff. Existing parameter IDs are preserved; Shape and Body are appended in separate per-set slots.

The selected-set Random button randomizes Decay, Brightness, Hardness, Shape, and Body together. One undo restores all five settings.

Sound presets are filtered by the selected material. Wood provides Hollow bamboo, Split bamboo clack, Deep bamboo, Dry bamboo rattle, Resonant wood block, Hard lumber knock, Soft timber, and Singing hardwood. Metal and plastic each offer three starting sounds. Applying a preset changes only Decay, Brightness, Hardness, Shape, and Body in the selected set, with one undo. The dropdown shows Custom when those values no longer match a preset, including after automation or randomization. Sound controls and their material/preset selectors share a panel group; scale and tuning share another. Global wind and reverb controls are grouped below, with jacks along the bottom.

Wood DSP revision: six wood-specific structural modes, two independent broad cavity modes, and four short contact-response modes. A normalized half-sine contact-force pulse replaces the declining impulse. Wood has no directly mixed noise burst. Damping follows structural frequency and geometry, with short upper-mode tails even at extended decay. Body balances short object response and sustained resonance; soft contact reduces attack bandwidth and peak rather than extending noise. Tube-to-tube strikes use a longer contact pulse and reduced contact-mode excitation than central-striker impacts. Both paths use collision position to weight modal excitation, while sounds still begin only on actual contacts.

Inharmonicity is continuous on every material: harmonic structural spacing at 0%, natural geometry at 50%, and exaggerated spacing at 100%, preserving the fundamental. The control is included in presets and sound randomization. Its parameter slots are appended after existing controls. Wood presets have been retuned for shorter natural sustain; the upper 15% of Decay retains extended sustain up to a 12-second main-mode T60 at full Body.

The animation fills the left side from top to bottom; every knob, selector, button and jack sits in the right-side control column. There is no module header. Far is the top of the scene and Near is the bottom: nearer sets draw larger and in front of farther sets. Hit testing follows this visual order. Dragging horizontally sets stereo pan; vertical placement sets nearness, affecting level and a gentle distance low-pass filter. The right-side 2D/3D button changes only the view.

The module is 52 HP wide, with all extra width allocated to the animation. The scene has no header or footer overlay; its vertical placement range allows sets nearer the top and bottom edges while retaining clearance for the hanging geometry.

Audible distance uses independent direct and room paths. The far-edge direct gain is one eighth of the near-edge gain (about −18 dB), with two cascaded low-pass stages moving from 800 Hz at Far to 18 kHz at Near. The room send decreases less rapidly than direct sound, increasing the reflected-to-direct balance for distant sets. The global reverb mix still controls the room return; setting it to zero leaves distance attenuation and filtering active.
