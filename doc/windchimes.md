# Windchimes

Windchimes creates a stereo scene of wind-driven tuned chime sets. Start with the
included metal set, connect **L / mono** and **R** to your mixer, and let the breeze
move its striker. Turn **Wind mix** down for chimes without audible wind.

## Arrange the scene

- **+ Add** creates a set, up to eight. Each set has one to twelve tubes.
- Click a set to select it. Drag it horizontally to change its stereo position;
  drag it down to bring it closer and increase its level. Each set is a stereo
  point source: its horizontal position determines the pan of all its tubes.
- Right-click a set for **Strike**, **Stop**, **Randomize**, **Wiggle**, then a divider and **Duplicate**, **Divide**, **Remove**. Duplicate copies all per-set settings and places the new set nearby; Divide makes the same copy and gently wiggles a random subset of its sound controls. The copy becomes selected. Duplicate/Divide are disabled when all eight slots are occupied. Randomize and Wiggle affect the clicked set's six sound controls. Copying, removing, and sound edits support one-step undo/redo. Strike pushes/resumes the clicked set; Stop pauses its motion and fades its direct sound and room send over 5 ms. The existing shared reverb tail decays normally. A paused set stays visible and retains its settings until Strike resumes it; this performance state is not saved in patches.
- **Remove**, **Delete**, or **Backspace** deletes the active set. The keys act on a focused visualization or while the pointer is over it, and do not delete the module. Holding a key removes only one set per press. Add, remove, edits, and moves support Rack's
  undo/redo. Patch files save every set and its settings.
- **Strike**, or double-clicking a set, pushes its suspended striker toward the
  tubes. Audio starts on physical contact. Tubes hang from a shared support on
  fixed-length strings, swing within bounded arcs, and can strike one another.
  Struck tubes briefly brighten; a matching flash marks striker impacts. The **Gust** button in the right control column pushes all sets.

The highlighted set uses the material and scale menus and the **Tubes**, **Root**,
**Register**, **Fine**, **EDO**, **Decay**, **Brightness**, **Hardness**, **Set level**,
**Spread**, and **Swing** controls. Hover over a knob to see its value; right-click
for exact entry. Material choices are metal, wood/bamboo, and plastic.

**EDO** means equal divisions of the octave (5–24). Scale degrees are mapped from
standard twelve-tone scales to the nearest step of the selected division. **Root**
uses standard semitones, **Register** moves by octaves, and **Fine** is in cents.
**Spread** is a scale-degree stride: 1 plays consecutive notes, 2 skips one note, 3 skips two, and 4 skips three, continuing through octave boundaries. It controls pitch spacing, independently of scene position and stereo pan. The scale menu offers 18 choices: the original five, Chromatic, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic minor, ascending Melodic minor, Minor blues, Major blues, half–whole Diminished, Hirajoshi, and Insen. Existing scale IDs and parameter IDs are preserved; Spread is appended with a default stride of 1. Duplicate/Divide include Spread, while sound Random/Wiggle leave tuning unchanged. Strike is in the selected sound section. This version does not import Scala files or use arbitrary ratio tunings.

## Wind and connections

**Wind**, **Gustiness**, and **Turbulence** control a shared changing weather field.
Each set responds to the shared weather through its own moving tubes and sail; collision strength determines strike
volume. **Swing** controls damping and motion persistence. **Wind mix** controls
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

The engine is original modal synthesis with a fixed twelve-slot modal bank per tube and
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

The tubes hang around a circular support. The central striker and each tube swing in two horizontal axes with fixed cord lengths. Impacts dissipate momentum, and gravity plus damping return the assembly to rest when wind stops. Click the 2D/3D button in the right control column to switch between the 2D top view and the pseudo-3D view. Both show the same simulated geometry; the 3D view draws distant bodies first.

Shape and Body are continuous per-set controls on every material. For wood/plastic, Shape morphs solid bar (0%), compact block (33%), open shell (67%), and hollow tube (100%); these are sound-design profiles rather than exact geometry measurements. Body controls muting of the object resonances for all three materials. Hardness controls the finite contact pulse duration; Brightness controls relative modal color. Wood starts darker and loses upper modes faster than metal. Decay maps to natural wood sustain with an extended upper range, and logarithmically to 0.12–40 seconds for the metal principal mode. For plastic, the main Decay range covers 0.045–2 seconds at 220 Hz and full Body, with the top 15% extending to 8 seconds; frequency and muting shorten it. Actual wood decay also depends on frequency, Shape, and Body. Voices sleep when total residual energy falls below the quietness threshold, with no fixed tail cutoff. Existing parameter IDs are preserved; Shape and Body are appended in separate per-set slots.

Rack’s standard Randomize action randomizes only Decay, Brightness, Hardness, Shape, Body, and Inharmonicity for every enabled chime set. It preserves global controls, all tuning controls (including tube count and pitch Spread), material, level, swing, scene placement, and enabled/disabled state. Disabled sets retain their saved sound settings. Rack’s standard undo restores the action.

The selected-set Random button randomizes Decay, Brightness, Hardness, Shape, Body, and Inharmonicity together. One undo restores all six settings. The adjacent Wiggle button chooses one to six of those controls without repetition, nudging each by a random 1–5% of its range in either direction. Nudges reflect at the control limits. One undo restores the entire wiggle; other sets and tuning/motion/global controls are unaffected.

Sound presets are filtered by the selected material. Wood provides Hollow bamboo, Split bamboo clack, Deep bamboo, Dry bamboo rattle, Resonant wood block, Hard lumber knock, Soft timber, and Singing hardwood. Metal offers Bright tube, Soft tube, Warm bar, Thin strip, Muted clink, Shell ping, Singing bar, and Shimmering tube. Plastic provides Hollow plastic, Solid plastic knock, Soft plastic block, Rigid pipe, Thin cup, Bottle pop, Dry toy clack, and Resonant bar. Applying a preset changes only Decay, Brightness, Hardness, Shape, Body, and Inharmonicity in the selected set, with one undo. The dropdown shows Custom when those values no longer match a preset, including after automation or randomization. Sound controls and their material/preset selectors share a panel group; scale and tuning share another. Global wind and reverb controls are grouped below, with jacks along the bottom.

Wood DSP revision: six structural modes and two broad cavity modes, with no independent click bank or directly mixed noise burst. Body adds frequency-dependent damping to the same structural modes instead of replacing them with unrelated transients. A normalized half-sine contact-force pulse supplies the attack. Its duration depends on hardness and collision speed; tube contact uses material/Shape hardness and a broader contact footprint, independently of the striker Hardness control. Signed mode-shape weights account for collision position and contact width. Bar profiles use free-free beam eigenfunctions, morphing to cosine approximations for compact/hollow profiles. These are analytic, research-informed sound-design approximations, not profiles fitted to measured recordings. Structural loss increases with absolute frequency, keeping upper modes short even at extended Decay. Coefficients update only when settings change; mode shapes and contact duration are evaluated on collisions, with no audio-rate allocation.

Inharmonicity is continuous on every material, preserving the fundamental. Wood/plastic move from harmonic structural spacing at 0% through natural geometry at 50% to exaggerated spacing at 100%. Metal instead varies a bounded beam dispersion correction and bending-pair splitting throughout the range, retaining a chime-like spectrum even at the endpoints. The control is included in presets and sound randomization. Its parameter slots are appended after existing controls. Wood presets have been retuned for shorter natural sustain; the upper 15% of Decay retains extended sustain up to a 12-second main-mode T60 at full Body.

The animation fills the left side from top to bottom; every knob, selector, button and jack sits in the right-side control column. There is no module header. Far is the top of the scene and Near is the bottom: nearer sets draw larger and in front of farther sets. Hit testing follows this visual order. Dragging horizontally sets stereo pan; vertical placement sets nearness, affecting level and a gentle distance low-pass filter. The right-side 2D/3D button changes only the view.

The module is 52 HP wide, with all extra width allocated to the animation. The scene has no header or footer overlay; its vertical placement range allows sets nearer the top and bottom edges while retaining clearance for the hanging geometry.

Audible distance uses independent direct and room paths. The far-edge direct gain is one eighth of the near-edge gain (about −18 dB), with two cascaded low-pass stages moving from 800 Hz at Far to 18 kHz at Near. The room send decreases less rapidly than direct sound, increasing the reflected-to-direct balance for distant sets. The global reverb mix still controls the room return; setting it to zero leaves distance attenuation and filtering active.

Striker motion revision: the clapper and wind sail are separate masses on two spherical links. Both move in two axes with fixed cord lengths. Coupled constraint forces include centripetal acceleration; collision impulses use the clapper's effective inverse mass while preserving independent sail momentum. A normalized mass ratio of 1.2 clapper : 0.6 sail and material-dependent restitution replace the previous light clapper/fixed restitution model. Swing controls friction, with nonzero damping at maximum. Wind acts primarily on the sail through quadratic relative-flow drag, with a simple projected side-area approximation based on cord orientation. Tubes have weaker mean-wind response and stronger exposure during gusts, preserving energetic tube-pair contact. No periodic strike clock or phase-aware sustaining force is used.

Weather retains prevailing direction through several swings, changing it gradually at 4–14-second intervals. Gusts use correlated fluctuations, including gentle along-wind variation at zero Turbulence; Turbulence adds cross-wind variation and faster eddies. The sail and clapper animation are published from the same physics state that produces audio contact events, in both views. An unforced swing has approximately a 1.92-second period with the default geometry; impacts and wind perturb its cadence. Fully constant airflow can lead to a modest tilted equilibrium; both links return to vertical when wind is removed. All physics work is fixed-size at control rate, with no allocations or locks in audio processing.

Metal DSP revision: six pairs of bending modes replace the shell-tone bank. Shape varies beam dispersion and cross-section asymmetry continuously from a thicker bar toward a slender tube; it no longer inserts low cavity/shell notes. A bounded shear/rotary-inertia approximation compresses ideal Euler–Bernoulli ratios toward observed metal-chime ratios. Inharmonicity controls that compression and subtle pair splitting; its endpoints remain bending spectra rather than harmonic strings or exaggerated bell spectra. The first member of the lowest pair retains the tuning pitch. Pair frequencies remain stable across strikes.

Metal loss separates common sustain/support damping from frequency-dependent internal loss. Upper partials have enough audible ringing to preserve the metallic character at moderate Decay and low Body, while still fading before the principal mode. The previous steep ratio-to-the-power-1.8 loss and long contact pulse were too soft and wooden. Decay sets the principal mode's T60 from 0.12 to 40 seconds at full Body; upper modes stay much shorter. Body adds gentler muting to these same modes with no independent noise burst. Brightness retains a high-frequency radiation floor, with stronger upper-mode weights. The normalized raised-cosine force pulse has smooth endpoints, with duration determined by striker hardness, pitch-dependent size, and impact speed. Metal striker duration spans approximately 0.08–1.28 ms at 220 Hz before impact-speed scaling. Tube contact has a shorter, firmer impulse and different contact width, independent of striker hardness. Signed free-free bending eigenfunctions weight the actual collision position.

Research: Lukkari and Välimäki, _Modal Synthesis of Wind Chime Sounds with Stochastic Event Triggering_ (NORSIG 2004), Tables 2–4; Stepanishen, _Acoustic transients from the impact force excitation of beams and wind chimes_ (ASA 2019); O'Brien, Shen and Gatchalian, _Synthesizing Sounds from Rigid-Body Simulations_ (SCA 2002). The NORSIG tube frequencies were measured, but its decay times were assigned from spectrogram inspection. This model uses original analytic approximations informed by those findings, not copied implementation code, a full shell/FEM solve, or a recording-fitted alloy model. All coefficients are cached and all per-sample storage and work remain fixed.

Plastic DSP: a separate fixed twelve-mode model contains six bending/body modes, four short wall modes and two broad cavity modes. Shape continuously varies solid bar/block versus open shell/tube response, while the wall and cavity families keep separate slots rather than dragging high structural frequencies into low notes. Inharmonicity morphs only the structural spacing and modest wall spacing; the tuned principal mode is preserved. Wall/cavity ratios are analytic perceptual approximations, not a geometry-calibrated shell solve.

Plastic structural damping uses the complex-modulus relationship (amplitude loss rate pi × frequency × loss factor), with mild frequency dispersion and added Body muting. At normal settings upper modes fade quickly but leave a round object response. High Decay allows deliberately extended rigid-plastic tones. The smooth normalized force pulse depends on contact stiffness, speed and object size. Plastic-to-plastic contact has its own firmness and width independent of striker Hardness. Signed spatial response weights the bending and wall modes at the actual collision point. There is no directly mixed noise burst or double application of striker softness. Preset names describe object/attack roles rather than exact polymer chemistry. Storage, mode count, and collision/control-rate coefficient evaluation remain fixed; all materials sleep when residual modal energy is quiet.

Plastic and revised metal references: Aramaki and Kronland-Martinet, _Analysis-Synthesis of Impact Sounds by Real-Time Dynamic Filtering_, IEEE TASLP 14(2), 695–705 (2006), discusses damping/spectral content and excitator filtering for material perception; Woodhouse, _Material damping and complex moduli_, Euphonics 10.3.3, gives the viscoelastic modal-loss relationship. _Non-Hertzian behavior in binary collisions of plastic balls derived from impact acoustics_ (2010), motivates treating contact time as an approximation rather than assuming an exact universal Hertz law. These are research-informed original models, not copied library implementations or recording-fitted polymer profiles.

Performance revision: resonators use three fixed four-lane SIMD banks (SSE on x64, NEON on ARM64), keeping all twelve original modal slots, coefficients, tuning and excitation. Passive ringing avoids repeated saturation work; dense impulses retain the previous clamp behavior through a per-bank headroom guard. Individual modes below 1e-16 squared state amplitude are cleared during the existing 256-sample silence check, and fully quiet banks skip audio work. The total worst-case removed amplitude is below 1.2e-7 before ring gain; the original whole-voice sleep threshold is retained. Strikes wake the configured banks again. Metal bending-pair contact weights are evaluated once per pair, and fixed beam-shape constants are precomputed.

Scene configuration is cached across identical control updates, including sample rate and transpose, and pan/distance gains are computed once per set. Pair-collision geometry is reused during each motion step, with the two affected shapes refreshed immediately after a collision resolution to preserve solver order. No new allocation or thread synchronization is introduced. `tests/windchimes_benchmark.cpp` provides a standalone optional workload for comparing materials, dense scenes, quiet scenes and sustained voices; benchmark timings are informational rather than CI assertions. Before/after renders should be compared when changing this kernel or collision caching.

Chime appearance follows the sound controls: Shape morphs the solid/block profile toward an open tube, Body increases fullness and rim thickness, Brightness strengthens highlights, Decay extends the strike glow and sustain stripe, and Inharmonicity changes surface-band spacing. Metal uses narrow crisp profiles, wood uses thicker slats with grain marks, and plastic uses rounded molded profiles with seams and heavier lips. Hardness changes the striker from a padded disk toward a faceted puck. These are decorative sound cues; collision geometry and motion remain unchanged. The overhead view shows compact cross-sections and hollow rims.

Output calibration boosts wood by 2.5× and plastic by 2× relative to their original resonator levels, and applies 3× overall gain before the existing bounded output limiter. Set level and Output level can trim louder patches; distance attenuation and material dynamics are retained.

Wind is sampled across the stage: position smoothly changes exposure and prevailing direction. Neighboring sets share drifting spatial eddies, with smaller independent local fluctuations even at the same position. Turbulence increases the strength and speed of these differences. Each set also has a modest fixed sail-drag variation. All forces come from weather; local eddies stop driving at zero wind, and the existing damping lets motion settle.
