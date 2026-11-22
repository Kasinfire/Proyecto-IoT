#pragma once
#include <cstdint>

namespace config {
constexpr uint32_t BAUDIOS = 115200;
constexpr uint32_t VENTANA_AGITACION_MS = 120;
constexpr uint32_t ANTIRREBOTE_AGITACION_MS = 4;
constexpr uint8_t CAMBIOS_PARA_INTENSIDAD_MAX = 10;
constexpr uint8_t UMBRAL_GESTO = 20;
constexpr uint8_t VENTANAS_PARA_CONFIRMAR = 1;
constexpr uint32_t INTERVALO_ENTRE_GESTOS_MS = 350;
constexpr uint32_t DURACION_DESTELLO_MS = 100;
}  // namespace config
