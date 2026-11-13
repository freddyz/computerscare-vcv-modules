#include <cassert>
#include <cstdio>

#include "Controller/Backend.hpp"
#ifndef _WIN32
#include <signal.h>
#endif
int main(int argc, char** argv) {
#ifndef _WIN32
  computerscare::controller::Sdl sdl;
  if (!sdl.load(argc > 1 ? argv[1] : ".")) {
    std::puts("SDL3 runtime unavailable; host signal test skipped");
    return 0;
  }
  struct sigaction beforeInt{}, beforeTerm{}, afterInt{}, afterTerm{};
  sigaction(SIGINT, nullptr, &beforeInt);
  sigaction(SIGTERM, nullptr, &beforeTerm);
  sdl.hint("SDL_NO_SIGNAL_HANDLERS", "0");
  assert(sdl.initForHost(0x00004000u));  // Events only: no device I/O.
  sigaction(SIGINT, nullptr, &afterInt);
  sigaction(SIGTERM, nullptr, &afterTerm);
  assert(beforeInt.sa_handler == afterInt.sa_handler);
  assert(beforeTerm.sa_handler == afterTerm.sa_handler);
  sdl.quit(0x00004000u);
  std::puts(
      "Controller SDL initialization preserves host SIGINT/SIGTERM handlers");
#endif
}
