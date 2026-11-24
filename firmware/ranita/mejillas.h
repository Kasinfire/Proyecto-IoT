// LEDs de las mejillas: brillan más entre más tapada está la ranita, salvo cuando
// el osito pide prenderlas en Simón Dice.
#pragma once
#include <cstdint>

namespace mejillas {
void iniciar();
void actualizar();
// led: 0 izquierda, 1 derecha o protocolo::LED_TODOS. Mientras dura, ignora la sombra.
void pedirLuz(uint8_t led, uint16_t duracion_ms);
}  // namespace mejillas
