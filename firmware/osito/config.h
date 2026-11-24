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
// Pollito en Libre/Simón: cuántas sacudidas llegan en VENTANA_SACUDIDAS_MS escoge el
// instrumento (notas General MIDI, canal 10). No se usa la intensidad: el sensor de bolita
// da casi siempre 20 (medido 2026-11-23: 18 de 18 sacudidas). El pollito manda como mucho
// una cada 350 ms, así que en 2 s caben ~6.
constexpr uint8_t NOTA_MARACA = 70;
constexpr uint8_t NOTA_PANDERETA = 54;
constexpr uint8_t NOTA_PLATILLO_ABIERTO = 46;
constexpr uint32_t VENTANA_SACUDIDAS_MS = 2000;
constexpr size_t SACUDIDAS_PANDERETA = 2;  // desde aquí, pandereta
constexpr size_t SACUDIDAS_PLATILLO = 4;   // desde aquí, pandereta + platillo de acento
constexpr size_t SACUDIDAS_PIO = 5;        // desde aquí, además un "pío"
constexpr uint8_t VELOCIDAD_SACUDIDA = 70;
constexpr uint8_t VELOCIDAD_SACUDIDA_PASO = 12;  // más fuerte con cada sacudida seguida
// Fuera de General MIDI (llega hasta 81): el sintetizador de la laptop lo toca como un pío.
constexpr uint8_t NOTA_PIO = 90;
constexpr uint32_t GOLPE_MS = 100;

// Teclado DJ
// Looper: 16 pulsos de 250 ms = 4 s (2 compases de 4 tiempos) con el tempo normal.
constexpr uint8_t LOOP_PULSOS = 16;
constexpr size_t LOOP_NOTAS_MAX = 64;
constexpr size_t LOOP_CAPAS_MAX = 16;  // cuántas se pueden deshacer
constexpr uint32_t TECLA_LARGA_MS = 1000;
constexpr uint8_t PULSOS_POR_TIEMPO = 2;  // el metrónomo suena cada 2 pulsos (cada 0.5 s)
constexpr uint8_t NOTA_METRONOMO = 37;    // General MIDI, canal 10: golpe de aro
constexpr uint8_t VELOCIDAD_METRONOMO = 35;
constexpr uint8_t VELOCIDAD_METRONOMO_FUERTE = 70;  // al empezar la vuelta
constexpr uint8_t TEMPO_MIN_PCT = 50;
constexpr uint8_t TEMPO_MAX_PCT = 200;
constexpr int TEMPO_PASO_PCT = 10;
// Efectos del DJ. Sus sonidos van en canales propios para que el fade de Canción no los
// apague: 14 acordes, 15 melodía (piano en la laptop) y 16 percusión (como el 10).
constexpr uint8_t CANAL_PERCUSION = 10;
constexpr uint8_t CANAL_DJ_ACORDES = 14;
constexpr uint8_t CANAL_DJ_MELODIA = 15;
constexpr uint8_t CANAL_DJ_PERCUSION = 16;
constexpr uint8_t ECO_REPETICIONES = 3;
constexpr uint8_t ECO_CADA_PULSOS = 2;  // cada 0.5 s con el tempo normal
constexpr uint8_t ECO_QUEDA_PCT = 60;   // cada repetición suena al 60 % de la anterior
constexpr uint8_t REDOBLE_PULSOS = 4;   // 1 s
constexpr uint8_t SUBIDA_PULSOS = 8;    // 2 s
constexpr uint8_t NOTA_TAMBOR = 38;     // General MIDI: tambor
constexpr uint8_t NOTA_BOMBO = 36;
constexpr uint8_t NOTA_PLATILLO_CERRADO = 42;
constexpr uint8_t NOTA_CENCERRO = 56;
constexpr uint8_t NOTA_PLATILLO_CHOQUE = 49;
constexpr uint8_t VELOCIDAD_ACORDES = 55;
constexpr uint8_t NOTA_CAIDA = 36;      // do grave del "drop"
// Sonidos del osito: acierto, error y aviso de animal desconectado.
constexpr uint8_t VELOCIDAD_AVISO = 100;
constexpr uint8_t NOTA_ACIERTO = 84;
constexpr uint8_t NOTA_ERROR = 40;
constexpr uint8_t NOTA_AVISO_DESCONEXION = 30;
constexpr uint32_t RESPUESTA_MS = 300;
// En el registro de audio: false si las notas tapan lo demás en el monitor serie.
constexpr bool IMPRIMIR_AUDIO = true;


// Simón Dice
// Del nivel 1 al último: la luz y el hueco se acortan y hay menos tiempo para cada paso.
constexpr uint8_t SIMON_NIVEL_MAX = 6;  // = pasos de la secuencia
constexpr uint8_t SIMON_FALLOS_PARA_BAJAR = 2;
constexpr uint16_t SIMON_LUZ_MS = 900;
constexpr uint16_t SIMON_LUZ_MIN_MS = 450;
constexpr uint32_t SIMON_HUECO_MS = 300;
constexpr uint32_t SIMON_HUECO_MIN_MS = 150;
constexpr uint32_t SIMON_ESPERA_MS = 5000;
constexpr uint32_t SIMON_ESPERA_MIN_MS = 3000;
constexpr uint32_t SIMON_PAUSA_MS = 1500;  // deja oír el resultado antes del siguiente reto
constexpr uint32_t SIMON_PAUSA_CORTA_MS = 600;
// Tapada, la ranita repite su nivel cada 500 ms; el pollito manda rebotes cada 350 ms.
constexpr uint32_t SIMON_SOMBRA_VIGENTE_MS = 1200;
constexpr uint32_t SIMON_REBOTE_POLLITO_MS = 700;
constexpr uint32_t SIMON_TONO_TRAS_MS = 350;  // primero se oye qué se tocó, luego acierto/error
constexpr uint32_t SIMON_ARPEGIO_MS = 110;
constexpr uint8_t SIMON_NOTA_RANITA = 76;
constexpr uint8_t SIMON_VELOCIDAD = 100;

// MQTT
constexpr uint16_t PUERTO_MQTTS = 8883;
constexpr uint32_t REINTENTO_WIFI_MS = 10000;
constexpr uint32_t REINTENTO_WIFI_MAX_MS = 120000;  // sin red, busca a lo más cada 2 min
constexpr uint32_t REINTENTO_MQTT_MS = 5000;
// Si el router no asocia en este tiempo, ESP-NOW arranca igual (la orquesta no depende del Wi-Fi).
constexpr uint32_t ESPERA_WIFI_PARA_RADIO_MS = 15000;
constexpr size_t BUFFER_MQTT_BYTES = 384;
constexpr uint32_t VOLUMEN_CADA_MS = 500;      // el potenciómetro no inunda el broker
constexpr uint32_t COLA_ESTADO_CADA_MS = 1000;
// RAM: 64 x 160 B = 10 KB. Sin Wi-Fi, a ~2 eventos/s alcanza para ~30 s; lo demás se descarta
// (el más viejo primero) hasta pasar la cola a LittleFS.
constexpr size_t CAPACIDAD_COLA_MQTT = 64;
constexpr size_t EVENTO_JSON_MAX = 160;  // modo con una canción larga y el arranque: 157
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
