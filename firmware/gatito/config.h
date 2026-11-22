// Tiempos y valores ajustables del gatito.

#pragma once

#include <cstdint>

namespace config {

constexpr uint32_t ANTIRREBOTE_MS = 25;
constexpr uint32_t PARPADEO_SIN_OSITO_MS = 600;

// Fija: las teclas son botones, no distinguen fuerza.
constexpr uint8_t VELOCIDAD_PRESIONAR = 100;

// Solo para el enlace simulado: duración de la luz que piden los comandos L0..L6 y LA.
constexpr uint16_t LUZ_PRUEBA_MS = 2000;

}  // namespace config
