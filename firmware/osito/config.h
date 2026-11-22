// Tiempos, umbrales y rangos del osito. Si algo se siente raro en la demo, se ajusta aquí.
#pragma once
#include <cstdint>

namespace config {
constexpr uint32_t BAUDIOS = 115200;

// Cada cuánto se imprime quién está conectado.
constexpr uint32_t REPORTE_ESTADO_CADA_MS = 3000;
}  // namespace config
