// Pines del osito: los del cableado que ya se probó funcionando. No cambiarlos sin probar el circuito.
#pragma once
#include <cstdint>

namespace pines {
constexpr uint8_t POTENCIOMETRO = 34;  // ADC1: ADC2 no funciona con Wi-Fi/ESP-NOW activos
constexpr uint8_t BOTON_MODO = 27;     // a GND, con INPUT_PULLUP
constexpr uint8_t LED_MODO_A = 18;
constexpr uint8_t LED_MODO_B = 19;
constexpr uint8_t LED_ALERTA = 21;
}  // namespace pines
