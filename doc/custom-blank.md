# Custom Blank

<img src="https://github.com/freddyz/computerscare-vcv-modules/blob/master/doc/custom-blank-with-expander.png" width="500" alt="Computerscare Custom Blank with Expander" />

- Resizable (click & drag the sides of the module.
- Add your own PNG, JPEG, BMP, or Animated GIF
- Move the image with keyboard keys A,S,D,W. Zoom in/out with Z,X
- Resizing modes available via right-click menu:
  - Fit Both (stretch both directions): image will always fill the height and width of module
  - Fit Width: image will always take up the full width of the module
  - Fit Height: image will always take up the full height
  - Free: image will not be automatically scaled.  You can still move/resize with the ASDW/ZX keys

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