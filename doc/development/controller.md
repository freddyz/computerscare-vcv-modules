# Controller backend

The controller module uses SDL3's gamepad, touchpad and sensor APIs. It does not use Rack's reduced gamepad-to-MIDI mapping. Device polling runs on one shared worker; the audio thread reads atomic values without locks or allocation. SDL initialization happens on the UI thread. The module browser does not initialize SDL.

For local development, install SDL3 or bundle a private runtime:

```
scripts/build-controller-runtime.sh --library /path/to/libSDL3.dylib
```

For a distributable, build SDL 3.4.18 source for each supported platform/architecture:

```
scripts/build-controller-runtime.sh /path/to/SDL3-source
```

The generated `lib/` directory is ignored by Git and included in `make dist`. Keep its SDL license. Match Rack's deployment target and architecture; a Homebrew binary is only suitable for local development. Windows needs SDL3.dll, Linux libSDL3.so.0, macOS libSDL3.dylib. A missing runtime is reported in the device selector instead of preventing the plugin from loading. Runtime bundling is not yet connected to release CI.

Physical verification: pair DualShock 4, DualSense and Xbox Series controllers, check all button modes, both trigger threshold outputs, stick ranges/dead zone, reconnect and unplug clearing. On both PlayStation models also check two-finger touch, touch click, mute, gyro and accelerometer. Bluetooth enhanced reports are enabled for DualSense touch/motion; this can affect its compatibility with other software until the controller is power-cycled.

Render: `scripts/preview-module.sh controller`. Unit checks run through `scripts/dev.sh check`.

Rumble requests use one packed atomic motor pair and an owner token. The audio thread publishes every 64 samples; SDL calls stay on the device worker, limited to 100 updates per second. Requests expire after 150 ms without a heartbeat, and each hardware effect lasts at most 200 ms. Unpatch, module deletion and device changes release the requesting module's ownership. Confirm real Bluetooth rumble on the physical controller; the preview checks command mapping and stop behavior without vibrating hardware.

SDL must be initialized through `Sdl::initForHost`, which sets `SDL_NO_SIGNAL_HANDLERS=1` before initializing any subsystem. SDL's default SIGINT/SIGTERM handlers convert those signals into SDL quit events; this polling backend does not consume quit events, so those handlers can prevent Rack from responding to the dev watcher's shutdown request. The runtime signal regression test initializes events only and does not open controllers, windows or audio devices.
