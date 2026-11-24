// Controla el LED de estado sin bloquear: destello al agitar y luz de Simón Dice.
#pragma once
#include <cstdint>

namespace led_estado {
void iniciar();
void actualizar();
void destellar();
// Si ya estaba prendido por más tiempo, no lo acorta.
void encenderPor(uint32_t duracion_ms);
}  // namespace led_estado
