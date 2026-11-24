// Pines del gatito (variante barata). Validar contra el circuito real antes de soldar.

#pragma once

#include <cstddef>
#include <cstdint>

namespace pines {

constexpr size_t NUM_TECLAS = 7;

// Mismo orden que protocolo::Nota: do, re, mi, fa, sol, la, si. En el montaje, do es la tecla
// del GPIO32 (probado el 2026-11-24); los LEDs van en el mismo orden que sus teclas.
// Botones a GND con pull-up interno. Ninguno es pin de arranque ni solo-entrada.
inline constexpr uint8_t BOTONES[NUM_TECLAS] = {32, 33, 25, 26, 27, 14, 13};
inline constexpr uint8_t LEDS[NUM_TECLAS] = {23, 22, 21, 19, 18, 17, 16};

}  // namespace pines
