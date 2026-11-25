// Pines del osito: los del cableado que ya se probó funcionando. No cambiarlos sin probar el circuito.
#pragma once
#include <cstdint>

namespace pines {
constexpr uint8_t POTENCIOMETRO = 34;  // ADC1: ADC2 no funciona con Wi-Fi/ESP-NOW activos
constexpr uint8_t BOTON_MODO = 27;     // a GND, con INPUT_PULLUP
constexpr uint8_t LED_MODO_A = 18;
constexpr uint8_t LED_MODO_B = 19;
constexpr uint8_t LED_ALERTA = 21;
// Teclado DJ 4x4 de membrana: sus cables 1 a 8 van a 13, 14, 16, 17, 22, 23, 32, 33. Filas y
// columnas según las teclas que llegaron el 2026-11-24; si se voltea el conector, se intercambian.
// Ninguno es pin de arranque y todos tienen pull-up interno.
constexpr uint8_t TECLADO_FILAS[] = {13, 14, 16, 17};
constexpr uint8_t TECLADO_COLUMNAS[] = {22, 23, 32, 33};
}  // namespace pines
