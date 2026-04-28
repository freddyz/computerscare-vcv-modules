# Custom Blank

<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/custom-blank-with-expander.png" width="500" alt="Computerscare Custom Blank with Expander" />

- Resizable (click & drag the sides of the module)
- Add your own PNG, JPEG, BMP, or Animated GIF
- Move the image with keyboard keys A,S,D,W. Zoom in/out with Z,X.

## Right-Click Options

### Image Scaling

- Fit Both (stretch both directions): image will always fill the height and width of module
- Fit Width: image will always take up the full width of the module
- Fit Height: image will always take up the full height
- Free: image will not be automatically scaled. You can still move/resize with the ASDW/ZX keys

### Dim Visuals with Room

When enabled, the image follows Rack's room brightness. Turn this off to keep the image fully opaque when dimming room lights.

### Keyboard Controls

Shows the keyboard shortcuts for moving, zooming, resetting, and mirroring the image.

### Load image (PNG, JPEG, BMP, GIF)

Loads a still image or animated GIF from disk. Image files can also be dragged and dropped onto the module.

### Animation

**Animation Enabled** turns GIF playback on or off.

**Animation Options** contains:

- **Animation Mode:** Forward, Reverse, Ping Pong, Random Shuffled, or Full Random
- **Constant Frame Delay:** use a single frame delay instead of per-frame GIF timing
- **Animation Speed:** playback speed multiplier

### Slideshow

**Slideshow Active** automatically loads another file from the current image's directory.

**Slideshow Options** contains:

- **Slideshow / Next File Behavior:** choose whether next-file triggers load the next alphabetical file, previous alphabetical file, or a random file from the directory
- **Crossfade:** fade between the current image and the next loaded image
- **Crossfade Time:** crossfade duration
- **Shuffle Seed:** changes the random order used by shuffled playback/file selection
- **Slideshow Time:** time between automatic slideshow file changes

## Expander
Connect to LEFT side of Custom Blank (it must be left side, the right side will not work)

### Inputs
- Clock Input
Control GIF animation via the clock input with 3 different clock modes:
	- Sync: Animation will synchronize to a steady clock signal
	- Scan: Animation will linearly follow a 0-10v CV.  0v → frame 1, 10v → last frame
	- Frame Advance: Clock signal will advance the animation by 1 frame

- Reset: Return to the 1st frame of the animation

### Knobs
	Zero Offset: Control the 1st frame for reset input/button, EOC output, and ping pong bounce point

### Outputs
	- EOC: emits a trigger when the animation restarts
	- Frame: emits a trigger each time the frame changes
