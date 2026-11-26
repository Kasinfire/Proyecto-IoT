// Umbrales y tiempos de la ranita. Si algo se siente raro en la demo, se ajusta aquí.
#pragma once
#include <cstddef>
#include <cstdint>

namespace config {
constexpr uint32_t BAUDIOS = 115200;

// Al arrancar se mide la luz del cuarto durante este tiempo (sin tapar la ranita).
constexpr uint32_t CALIBRACION_MS = 2000;
constexpr uint32_t LECTURA_CADA_MS = 20;

// Con la LDR totalmente tapada, la lectura baja a este porcentaje de la luz ambiente. En un salón
// con luz de techo una mano no deja tan oscuro: con 35 llega al nivel máximo sin tener que pegarla.
constexpr uint8_t LUZ_TAPADA_PCT = 35;
// Debajo de esto el cuarto está tan oscuro que la sombra casi no se nota.
constexpr uint16_t LUZ_AMBIENTE_MIN = 300;
// Niveles menores se toman como 0. Con 12 la luz tiene que bajar ~8 %: ni el ruido del ADC ni la
// sombra de alguien parado junto a la mesa la hacen sonar. Si cuesta que suene, bajarlo.
constexpr uint8_t ZONA_MUERTA_NIVEL = 12;
// Promedio de las últimas lecturas (4 x 20 ms): quita el ruido del ADC y el parpadeo de las
// lámparas sin que se sienta lenta.
constexpr size_t PROMEDIO_LECTURAS = 4;

// Recalibración mientras funciona (calibracion_luz.h). En lecturas de LECTURA_CADA_MS:
constexpr uint32_t CALIBRA_SUBIR_LECTURAS = 50;   // ~1 s para seguir una luz más fuerte
constexpr uint32_t CALIBRA_BAJAR_LECTURAS = 500;  // ~10 s para seguir una luz que baja, destapada
// "Tapada" sin moverse este tiempo = apagaron luces, no es una mano: esa luz pasa a ser la normal.
// Una mano que se queda quieta encima tanto tiempo también cuenta: deja de sonar hasta quitarla.
constexpr uint32_t CALIBRA_TAPADA_FIJA_MS = 30000;
constexpr uint8_t CALIBRA_QUIETA_PCT = 4;  // cuánto puede moverse la luz y seguir "quieta"
constexpr uint16_t CALIBRA_QUIETA_MIN = 25;  // margen fijo por el ruido del ADC con poca luz
// Desde aquí el ADC está saturado (máximo 4095): una lámpara encima o un cable flojo, no la luz del
// cuarto. No se toma como normal; si no, después todo parecería "tapado" y sonaría sola.
constexpr uint16_t LUZ_SATURADA = 4000;

// Cada cuánto imprime luz, luz normal y nivel en el monitor serie, para medir en el salón.
// 0 = no imprime.
constexpr uint32_t IMPRIMIR_LUZ_CADA_MS = 2000;

// Cuándo se le avisa al osito.
constexpr uint8_t CAMBIO_MINIMO_NIVEL = 5;
constexpr uint32_t REPORTE_MIN_CADA_MS = 50;
// El osito apaga la parte de la ranita ~2 s después del último mensaje; si la mano
// se queda quieta tapándola, se repite el nivel para que siga sonando.
constexpr uint32_t REPETIR_TAPADA_CADA_MS = 500;

constexpr uint8_t BRILLO_MAX = 255;
constexpr uint32_t PARPADEO_CALIBRACION_MS = 250;
}  // namespace config
