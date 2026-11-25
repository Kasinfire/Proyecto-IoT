// Tiempos y valores ajustables del gatito.

#pragma once

#include <cstdint>

namespace config {

constexpr uint32_t ANTIRREBOTE_MS = 25;
constexpr uint32_t PARPADEO_SIN_OSITO_MS = 600;

// Fija: las teclas son botones, no distinguen fuerza.
constexpr uint8_t VELOCIDAD_PRESIONAR = 100;

}  // namespace config
