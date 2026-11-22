// Tiempos, umbrales y rangos del osito. Si algo se siente raro en la demo, se ajusta aquí.
#pragma once
#include <cstddef>
#include <cstdint>

namespace config {
constexpr uint32_t BAUDIOS = 115200;

// Animales
// Especificación: un animal sigue "activo" ~2 s después de su último gesto.
constexpr uint32_t ACTIVO_TRAS_GESTO_MS = 2000;
constexpr size_t CAPACIDAD_EVENTOS = 16;
constexpr uint32_t REPORTE_ESTADO_CADA_MS = 3000;
// Los latidos llegan cada segundo por animal: imprimirlos tapa todo lo demás.
constexpr bool IMPRIMIR_LATIDOS = false;

// Simulación por monitor serie
constexpr size_t LINEA_SIM_MAX = 96;
constexpr size_t PALABRAS_SIM_MAX = 8;
}  // namespace config
