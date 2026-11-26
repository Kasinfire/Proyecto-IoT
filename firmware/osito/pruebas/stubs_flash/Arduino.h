// Solo para probar cola_persistente en la compu.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>

extern uint32_t reloj_ms;
inline uint32_t millis() { return reloj_ms; }

struct SerialFalso {
  void println(const char*) {}
  template <class... A>
  void printf(const char*, A...) {}
};
inline SerialFalso Serial;
