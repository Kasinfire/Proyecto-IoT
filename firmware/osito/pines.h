// Pines del osito: los del cableado que ya se probó funcionando. No cambiarlos sin probar el circuito.
#pragma once
#include <cstdint>

namespace pines {
constexpr uint8_t POTENCIOMETRO = 34;  // ADC1: ADC2 no funciona con Wi-Fi/ESP-NOW activos
constexpr uint8_t BOTON_MODO = 27;     // a GND, con INPUT_PULLUP
constexpr uint8_t LED_MODO_A = 18;
constexpr uint8_t LED_MODO_B = 19;
constexpr uint8_t LED_ALERTA = 21;
// Teclado DJ 4x4 de membrana, conectado a 13, 14, 16, 17, 22, 23, 32, 33. En el montaje el
// conector quedó volteado (probado tecla por tecla el 2026-11-23): las filas 1..4 llegan a
// 33, 32, 23, 22 y las columnas 1..4 a 17, 16, 14, 13. Ninguno es pin de arranque y todos
// tienen pull-up interno.
constexpr uint8_t TECLADO_FILAS[] = {33, 32, 23, 22};
constexpr uint8_t TECLADO_COLUMNAS[] = {17, 16, 14, 13};
}  // namespace pines
