#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Signals.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace computerscare {
namespace controller {
// Only SDL3's stable opaque-handle C API is needed. Keeping the ABI boundary
// here lets the plugin report a missing runtime instead of failing to load.
struct Sdl {
  void* library = nullptr;
  bool (*init)(uint32_t) = nullptr;
  void (*quit)(uint32_t) = nullptr;
  bool (*hint)(const char*, const char*) = nullptr;
  const char* (*error)() = nullptr;
  void (*update)() = nullptr;
  void (*gamepadEvents)(bool) = nullptr;
  void (*joystickEvents)(bool) = nullptr;
  bool (*connected)(void*) = nullptr;
  uint32_t* (*ids)(int*) = nullptr;
  void (*freeMemory)(void*) = nullptr;
  void* (*open)(uint32_t) = nullptr;
  void (*close)(void*) = nullptr;
  const char* (*name)(void*) = nullptr;
  const char* (*serial)(void*) = nullptr;
  int (*type)(void*) = nullptr;
  uint16_t (*vendor)(void*) = nullptr;
  uint16_t (*product)(void*) = nullptr;
  bool (*hasButton)(void*, int) = nullptr;
  bool (*button)(void*, int) = nullptr;
  bool (*hasAxis)(void*, int) = nullptr;
  int16_t (*axis)(void*, int) = nullptr;
  int (*touchpads)(void*) = nullptr;
  int (*fingers)(void*, int) = nullptr;
  bool (*touch)(void*, int, int, bool*, float*, float*, float*) = nullptr;
  bool (*hasSensor)(void*, int) = nullptr;
  bool (*enableSensor)(void*, int, bool) = nullptr;
  bool (*sensor)(void*, int, float*, int) = nullptr;
  int (*power)(void*, int*) = nullptr;
  uint32_t (*properties)(void*) = nullptr;
  bool (*booleanProperty)(uint32_t, const char*, bool) = nullptr;
  bool (*rumble)(void*, uint16_t, uint16_t, uint32_t) = nullptr;
  template <class F>
  bool bind(F& f, const char* symbol) {
#ifdef _WIN32
    auto p = GetProcAddress(static_cast<HMODULE>(library), symbol);
#else
    auto p = dlsym(library, symbol);
#endif
    static_assert(sizeof(f) == sizeof(p), "Function pointer ABI");
    std::memcpy(&f, &p, sizeof(f));
    return f != nullptr;
  }
  bool load(const std::string& root) {
#ifdef _WIN32
    const std::vector<std::string> paths = {root + "/lib/SDL3.dll", "SDL3.dll"};
#elif defined(__APPLE__)
    const std::vector<std::string> paths = {
        root + "/lib/libSDL3.dylib", "/opt/homebrew/lib/libSDL3.dylib",
        "/usr/local/lib/libSDL3.dylib", "libSDL3.dylib"};
#else
    const std::vector<std::string> paths = {root + "/lib/libSDL3.so.0",
                                            "libSDL3.so.0", "libSDL3.so"};
#endif
    for (const auto& path : paths) {
#ifdef _WIN32
      library = static_cast<void*>(LoadLibraryA(path.c_str()));
#else
      library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
      if (library) break;
    }
    if (!library) return false;
#define CS_SDL(field, symbol) \
  if (!bind(field, symbol)) return false
    CS_SDL(init, "SDL_InitSubSystem");
    CS_SDL(quit, "SDL_QuitSubSystem");
    CS_SDL(hint, "SDL_SetHint");
    CS_SDL(error, "SDL_GetError");
    CS_SDL(update, "SDL_UpdateGamepads");
    CS_SDL(gamepadEvents, "SDL_SetGamepadEventsEnabled");
    CS_SDL(joystickEvents, "SDL_SetJoystickEventsEnabled");
    CS_SDL(connected, "SDL_GamepadConnected");
    CS_SDL(ids, "SDL_GetGamepads");
    CS_SDL(freeMemory, "SDL_free");
    CS_SDL(open, "SDL_OpenGamepad");
    CS_SDL(close, "SDL_CloseGamepad");
    CS_SDL(name, "SDL_GetGamepadName");
    CS_SDL(serial, "SDL_GetGamepadSerial");
    CS_SDL(type, "SDL_GetGamepadType");
    CS_SDL(vendor, "SDL_GetGamepadVendor");
    CS_SDL(product, "SDL_GetGamepadProduct");
    CS_SDL(hasButton, "SDL_GamepadHasButton");
    CS_SDL(button, "SDL_GetGamepadButton");
    CS_SDL(hasAxis, "SDL_GamepadHasAxis");
    CS_SDL(axis, "SDL_GetGamepadAxis");
    CS_SDL(touchpads, "SDL_GetNumGamepadTouchpads");
    CS_SDL(fingers, "SDL_GetNumGamepadTouchpadFingers");
    CS_SDL(touch, "SDL_GetGamepadTouchpadFinger");
    CS_SDL(hasSensor, "SDL_GamepadHasSensor");
    CS_SDL(enableSensor, "SDL_SetGamepadSensorEnabled");
    CS_SDL(sensor, "SDL_GetGamepadSensorData");
    CS_SDL(power, "SDL_GetGamepadPowerInfo");
    CS_SDL(properties, "SDL_GetGamepadProperties");
    CS_SDL(booleanProperty, "SDL_GetBooleanProperty");
    CS_SDL(rumble, "SDL_RumbleGamepad");
#undef CS_SDL
    return true;
  }
  bool initForHost(uint32_t flags) {
    // SDL is embedded in Rack, which owns process shutdown. Otherwise SDL
    // consumes SIGINT/SIGTERM as quit events our polling backend never reads.
    hint("SDL_NO_SIGNAL_HANDLERS", "1");
    return init(flags);
  }
  ~Sdl() {
    if (library) {
#ifdef _WIN32
      FreeLibrary(static_cast<HMODULE>(library));
#else
      dlclose(library);
#endif
    }
  }
};
inline uint32_t nextRumbleOwner() {
  static std::atomic<uint32_t> next{0};
  return next.fetch_add(1) + 1;
}
// One atomic message prevents torn motor pairs. Most recent patched module
// wins.
struct RumbleMailbox {
  std::atomic<uint64_t> command{0}, revision{0};
  void request(uint32_t owner, float low, float high) {
    uint64_t a = rumbleLevel(low), b = rumbleLevel(high);
    command.store((uint64_t(owner) << 32) | (b << 16) | a);
    revision.fetch_add(1);
  }
  void stop(uint32_t owner) {
    uint64_t old = command.load();
    while (uint32_t(old >> 32) == owner) {
      if (command.compare_exchange_weak(old, 0)) {
        revision.fetch_add(1);
        return;
      }
    }
  }
};
struct Connection {
  uint32_t id = 0;
  Profile profile = Unknown;
  std::string key, name;
  std::atomic<bool> connected{false};
  std::atomic<bool> rumbleSupported{false};
  RumbleMailbox rumble;
  std::atomic<uint32_t> supportedButtons{0}, supportedAnalog{0};
  std::array<std::atomic<bool>, buttons> down;
  std::array<std::atomic<uint32_t>, buttons> presses;
  std::array<std::atomic<float>, analogs> values;
  Connection() {
    for (auto& x : down) x.store(false);
    for (auto& x : presses) x.store(0);
    for (auto& x : values) x.store(0);
  }
  void setButton(int i, bool value) {
    bool old = down[i].exchange(value);
    if (value && !old) presses[i].fetch_add(1);
  }
};
class Service {
  struct Device {
    void* pad = nullptr;
    std::shared_ptr<Connection> state;
    int powerTicks = 0;
    uint64_t rumbleRevision = 0;
    uint32_t appliedRumble = 0;
    std::chrono::steady_clock::time_point rumbleUpdated{}, rumbleSent{};
  };
  Sdl sdl;
  std::vector<Device> devices;
  std::vector<std::shared_ptr<Connection>> listed;
  std::mutex mutex;
  std::atomic<bool> stopping{false};
  std::thread worker;
  bool initialized = false;
  std::string message;
  void enumerate() {
    int count = 0;
    uint32_t* ids = sdl.ids(&count);
    for (auto it = devices.begin(); it != devices.end();) {
      bool found = false;
      for (int i = 0; i < count; ++i) found |= ids[i] == it->state->id;
      if (!found) {
        it->state->connected = false;
        sdl.rumble(it->pad, 0, 0, 0);
        sdl.close(it->pad);
        it = devices.erase(it);
      } else
        ++it;
    }
    for (int i = 0; i < count; ++i) {
      bool found = false;
      for (auto& d : devices) found |= d.state->id == ids[i];
      if (found) continue;
      void* pad = sdl.open(ids[i]);
      if (!pad) continue;
      int type = sdl.type(pad);
      uint16_t vid = sdl.vendor(pad);
      Profile profile = (type == 6)                    ? DualSense
                        : (type == 3 || vid == 0x045e) ? Xbox
                                                       : Unknown;
      if (type == 5) profile = DualShock4;
      if (profile == Unknown) {
        sdl.close(pad);
        continue;
      }
      auto state = std::make_shared<Connection>();
      state->id = ids[i];
      state->profile = profile;
      const char* label = sdl.name(pad);
      state->name = label ? label : "Controller";
      const char* serial = sdl.serial(pad);
      state->key = std::to_string(vid) + ":" +
                   std::to_string(sdl.product(pad)) + ":" +
                   (serial && *serial ? serial : state->name);
      // No serial: identical units are distinguished within the current
      // session.
      bool duplicate = false;
      for (auto& d : devices) duplicate |= d.state->key == state->key;
      if (duplicate) state->key += ":" + std::to_string(ids[i]);
      static const int map[] = {0,  1, 2, 3, 11, 12, 13, 14, 9,
                                10, 7, 8, 4, 6,  5,  20, 15};
      uint32_t bm = 0, am = (1u << 7);
      for (int b = 0; b < 17; ++b)
        if (sdl.hasButton(pad, map[b])) bm |= 1u << b;
      for (int a = 0; a < 6; ++a)
        if (sdl.hasAxis(pad, a)) am |= 1u << a;
      if (am & (1u << 4)) bm |= 1u << 19;
      if (am & (1u << 5)) bm |= 1u << 20;
      if (sdl.touchpads(pad) > 0)
        for (int f = 0; f < std::min(2, sdl.fingers(pad, 0)); ++f) {
          bm |= 1u << (17 + f);
          am |= 3u << (8 + 2 * f);
        }
      for (int sensor = 1; sensor <= 2; ++sensor)
        if (sdl.hasSensor(pad, sensor) && sdl.enableSensor(pad, sensor, true))
          am |= 7u << (sensor == 1 ? 15 : 12);
      state->rumbleSupported = sdl.booleanProperty(
          sdl.properties(pad), "SDL.joystick.cap.rumble", false);
      state->supportedButtons = bm;
      state->supportedAnalog = am;
      Device device;
      device.pad = pad;
      device.state = state;
      devices.push_back(device);
    }
    if (ids) sdl.freeMemory(ids);
    std::lock_guard<std::mutex> lock(mutex);
    listed.clear();
    for (auto& d : devices) listed.push_back(d.state);
  }
  void read(Device& d) {
    auto& c = *d.state;
    if (!sdl.connected(d.pad)) {
      c.connected = false;
      return;
    }
    static const int map[] = {0,  1, 2, 3, 11, 12, 13, 14, 9,
                              10, 7, 8, 4, 6,  5,  20, 15};
    for (int i = 0; i < 17; ++i) c.setButton(i, sdl.button(d.pad, map[i]));
    for (int i = 0; i < 6; ++i) {
      int raw = sdl.axis(d.pad, i);
      float value = i < 4 ? raw / (raw < 0 ? 32768.f : 32767.f)
                          : std::max(0, raw) / 32767.f;
      if (i == 1 || i == 3) value = -value;
      c.values[i] = value;
    }
    for (int f = 0; f < 2; ++f)
      if (c.supportedButtons.load() & (1u << (17 + f))) {
        bool down = false;
        float x = 0, y = 0, pressure = 0;
        if (sdl.touch(d.pad, 0, f, &down, &x, &y, &pressure)) {
          c.setButton(17 + f, down);
          if (down) {
            c.values[8 + 2 * f] = x;
            c.values[9 + 2 * f] = 1.f - y;
          }
        }
      }
    for (int sensor = 1; sensor <= 2; ++sensor) {
      int start = sensor == 1 ? 15 : 12;
      if (c.supportedAnalog.load() & (1u << start)) {
        float data[3]{};
        if (sdl.sensor(d.pad, sensor, data, 3))
          for (int i = 0; i < 3; ++i) c.values[start + i] = data[i];
      }
    }
    if (d.powerTicks++ % 500 == 0) {
      int percent = -1;
      sdl.power(d.pad, &percent);
      if (percent >= 0) {
        c.values[6] = percent / 100.f;
        c.supportedAnalog.fetch_or(1u << 6);
      } else
        c.supportedAnalog.fetch_and(~(1u << 6));
    }
    c.values[7] = 1;
    c.connected.store(true, std::memory_order_release);
  }
  void updateRumble(Device& d) {
    auto& c = *d.state;
    if (!c.rumbleSupported.load() || !c.connected.load()) return;
    auto now = std::chrono::steady_clock::now();
    auto revision = c.rumble.revision.load();
    if (revision != d.rumbleRevision) {
      d.rumbleRevision = revision;
      d.rumbleUpdated = now;
    }
    uint64_t command = c.rumble.command.load();
    uint32_t target = uint32_t(command);
    if (!(command >> 32) ||
        now - d.rumbleUpdated > std::chrono::milliseconds(150))
      target = 0;
    auto age = now - d.rumbleSent;
    if ((target != d.appliedRumble && age >= std::chrono::milliseconds(10)) ||
        (target && age >= std::chrono::milliseconds(100))) {
      if (!sdl.rumble(d.pad, uint16_t(target), uint16_t(target >> 16), 200))
        c.rumbleSupported = false;
      d.appliedRumble = target;
      d.rumbleSent = now;
    }
  }
  void loop() {
    int tick = 0;
    while (!stopping.load()) {
      sdl.update();
      if (tick++ % 250 == 0) enumerate();
      for (auto& d : devices) {
        read(d);
        updateRumble(d);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    for (auto& d : devices) {
      d.state->connected = false;
      sdl.rumble(d.pad, 0, 0, 0);
      sdl.close(d.pad);
    }
    devices.clear();
  }

 public:
  // Call on Rack's UI/main thread; device I/O then lives on the worker.
  explicit Service(const std::string& root) {
    if (!sdl.load(root)) {
      message = "SDL3 runtime missing or incompatible";
      return;
    }
    sdl.hint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
    sdl.hint("SDL_JOYSTICK_ENHANCED_REPORTS", "1");
    if (!sdl.initForHost(0x00002000u)) {
      message = sdl.error();
      return;
    }
    sdl.gamepadEvents(false);
    sdl.joystickEvents(false);
    initialized = true;
    message = "Ready";
    worker = std::thread([this]() { loop(); });
  }
  ~Service() {
    stopping = true;
    if (worker.joinable()) worker.join();
    if (initialized) sdl.quit(0x00002000u);
  }
  std::vector<std::shared_ptr<Connection>> connections() {
    std::lock_guard<std::mutex> lock(mutex);
    return listed;
  }
  const std::string& status() const { return message; }
};
inline std::shared_ptr<Service> service(const std::string& root) {
  static std::weak_ptr<Service> shared;
  auto instance = shared.lock();
  if (!instance) {
    instance = std::make_shared<Service>(root);
    shared = instance;
  }
  return instance;
}
}  // namespace controller
}  // namespace computerscare
