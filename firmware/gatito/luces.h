// Controla los 7 LEDs: se encienden al presionar la tecla, cuando el osito
// pide luz (Simón Dice) y parpadean lento si no hay osito.

#pragma once

#include <cstdint>

namespace luces {

void iniciar();
void actualizar();

// led: índice de la tecla (protocolo::Nota) o protocolo::LED_TODOS.
void pedirLuz(uint8_t led, uint16_t duracion_ms);

}  // namespace luces
