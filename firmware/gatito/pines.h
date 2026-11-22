// Pines del gatito (variante barata). Validar contra el circuito real antes de soldar.

#pragma once

#include <cstddef>
#include <cstdint>

namespace pines {

constexpr size_t NUM_TECLAS = 7;

// Mismo orden que protocolo::Nota: do, re, mi, fa, sol, la, si.
// Botones a GND con pull-up interno. Ninguno es pin de arranque ni solo-entrada.
inline constexpr uint8_t BOTONES[NUM_TECLAS] = {13, 14, 27, 26, 25, 33, 32};
inline constexpr uint8_t LEDS[NUM_TECLAS] = {16, 17, 18, 19, 21, 22, 23};

}  // namespace pines
