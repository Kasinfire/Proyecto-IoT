// Umbrales y tiempos de la ranita. Si algo se siente raro en la demo, se ajusta aquí.
#pragma once
#include <cstdint>

namespace config {
constexpr uint32_t BAUDIOS = 115200;

// Al arrancar se mide la luz del cuarto durante este tiempo (sin tapar la ranita).
constexpr uint32_t CALIBRACION_MS = 2000;
constexpr uint32_t LECTURA_CADA_MS = 20;

// Con la LDR totalmente tapada, la lectura baja a este porcentaje de la luz ambiente.
constexpr uint8_t LUZ_TAPADA_PCT = 30;
// Debajo de esto el cuarto está tan oscuro que la sombra casi no se nota.
constexpr uint16_t LUZ_AMBIENTE_MIN = 300;
// Niveles menores se toman como 0, para que el ruido del ADC no cuente como sombra.
constexpr uint8_t ZONA_MUERTA_NIVEL = 5;

// Cuándo se le avisa al osito.
constexpr uint8_t CAMBIO_MINIMO_NIVEL = 5;
constexpr uint32_t REPORTE_MIN_CADA_MS = 50;
// El osito apaga la parte de la ranita ~2 s después del último mensaje; si la mano
// se queda quieta tapándola, se repite el nivel para que siga sonando.
constexpr uint32_t REPETIR_TAPADA_CADA_MS = 500;

constexpr uint8_t BRILLO_MAX = 255;
constexpr uint32_t PARPADEO_CALIBRACION_MS = 250;
}  // namespace config
