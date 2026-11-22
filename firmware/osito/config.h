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
constexpr size_t CAPACIDAD_CAMBIOS = 8;
constexpr uint32_t REPORTE_ESTADO_CADA_MS = 3000;
// Los latidos llegan cada segundo por animal: imprimirlos tapa todo lo demás.
constexpr bool IMPRIMIR_LATIDOS = false;

// Entradas
constexpr uint32_t BOTON_ESTABLE_MS = 30;
constexpr uint32_t LECTURA_POTENCIOMETRO_CADA_MS = 20;
constexpr size_t LECTURAS_POTENCIOMETRO = 8;
constexpr uint16_t ADC_MAX = 4095;
constexpr uint8_t VOLUMEN_MAX = 127;  // escala MIDI
constexpr uint8_t CAMBIO_MINIMO_VOLUMEN = 3;

// Luces
constexpr uint32_t PULSO_LED_MS = 80;
constexpr uint32_t PARPADEO_ALERTA_MS = 300;

// Música
// Canal MIDI por parte (1..16), índice = Nodo: osito bajo, ranita violín, gatito piano,
// pollito percusión (el 10 es el de percusión en General MIDI).
constexpr uint8_t CANAL_POR_NODO[] = {1, 2, 3, 10};
constexpr uint32_t FADE_MS = 150;  // "las partes entran y salen con fade corto"
constexpr uint32_t FADE_CADA_MS = 10;
constexpr uint8_t EXPRESION_PASO = 8;
constexpr uint8_t VELOCIDAD_CANCION = 90;
constexpr uint8_t VELOCIDAD_RANITA = 80;
constexpr uint8_t NOTA_MARACA = 70;  // General MIDI, canal 10
constexpr uint32_t GOLPE_MS = 100;
// Sonidos del osito: acierto, error y aviso de animal desconectado.
constexpr uint8_t VELOCIDAD_AVISO = 100;
constexpr uint8_t NOTA_ACIERTO = 84;
constexpr uint8_t NOTA_ERROR = 40;
constexpr uint8_t NOTA_AVISO_DESCONEXION = 30;
constexpr uint32_t RESPUESTA_MS = 300;
// En el registro de audio: false si las notas tapan lo demás en el monitor serie.
constexpr bool IMPRIMIR_AUDIO = true;

// Simón Dice
constexpr uint16_t SIMON_LUZ_MS = 1500;
constexpr uint32_t SIMON_ESPERA_MS = 5000;
constexpr uint32_t SIMON_PAUSA_MS = 1500;

// MQTT
// RAM: 64 x 128 B = 8 KB. Sin Wi-Fi, a ~2 eventos/s alcanza para ~30 s; lo demás se descarta
// (el más viejo primero) hasta pasar la cola a LittleFS.
constexpr size_t CAPACIDAD_COLA_MQTT = 64;
constexpr size_t EVENTO_JSON_MAX = 128;
constexpr size_t TOPICO_MAX = 40;
constexpr size_t PUBLICAR_POR_VUELTA = 4;
constexpr uint32_t REPUBLICAR_SIN_ACK_MS = 10000;
// La ranita reporta hasta 20 veces por segundo: al broker solo va cada medio segundo
// (y siempre cuando empieza o deja de haber sombra).
constexpr uint32_t SOMBRA_EVENTO_CADA_MS = 500;
constexpr uint32_t HORA_VALIDA_DESDE = 1700000000;  // antes de esto NTP no ha respondido
constexpr size_t PEDIDOS_MODO_MAX = 4;

// Simulación por monitor serie
constexpr size_t LINEA_SIM_MAX = 96;
constexpr size_t PALABRAS_SIM_MAX = 8;
}  // namespace config
