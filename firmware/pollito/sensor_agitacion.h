// Detecta cambios del SW-520D y calcula la intensidad de cada sacudida.
#pragma once

#include <cstdint>

namespace sensor_agitacion {
void iniciar();
void actualizar();
bool seDetectoGesto(uint8_t& intensidad);
}  // namespace sensor_agitacion
