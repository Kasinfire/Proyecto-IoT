// Formato de los mensajes ESP-NOW entre los animalitos y el osito.
//
// Un mensaje es: Cabecera (en claro) || nonce || payload cifrado || tag.
// La cabecera se pasa como AAD a AES-128-GCM, así que si alguien la altera
// el tag deja de cuadrar y el mensaje se descarta.
//
// Claves:
//   - Animal -> osito: clave de enlace propia de cada animal.
//   - Osito -> animales: clave de grupo compartida, para poder mandar un
//     mismo mensaje (p. ej. cambio de modo) a todos a la vez.
//
// Replay: el receptor guarda por emisor el último (arranque, seq) aceptado y
// rechaza cualquiera que no sea mayor. `arranque` vive en NVS y sube en cada
// boot; por eso `seq` puede reiniciar en 0 sin abrir un hueco.

#pragma once

#include <esp_now.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace protocolo {

constexpr uint8_t VERSION = 2;

// Valor numérico de cualquier enum class, para indexar arreglos o meterlo en un struct.
template <typename E>
constexpr std::underlying_type_t<E> aEntero(E valor) {
  return static_cast<std::underlying_type_t<E>>(valor);
}

enum class Nodo : uint8_t {
  Osito = 0,
  Ranita = 1,
  Gatito = 2,
  Pollito = 3,
};

// Mismo orden que Nodo y mismo texto que el campo "nodo" del JSON en MQTT.
inline constexpr const char* NOMBRE_NODO[] = {"osito", "ranita", "gatito", "pollito"};
constexpr size_t NUM_NODOS = std::size(NOMBRE_NODO);

enum class TipoMensaje : uint8_t {
  // Animal -> osito
  Latido = 0,  // sin payload
  Nota = 1,
  Sombra = 2,
  Agitar = 3,

  // Osito -> animales
  Modo = 10,
  Luz = 11,
};

enum class Nota : uint8_t { Do, Re, Mi, Fa, Sol, La, Si };
inline constexpr const char* NOMBRE_NOTA[] = {"do", "re", "mi", "fa", "sol", "la", "si"};
constexpr size_t NUM_NOTAS = std::size(NOMBRE_NOTA);

enum class Modo : uint8_t { Cancion, Libre, Simon };
// Mismo orden que Modo y mismo texto que el campo "modo" de orquesta/control/modo.
inline constexpr const char* NOMBRE_MODO[] = {"cancion", "libre", "simon"};
constexpr size_t NUM_MODOS = std::size(NOMBRE_MODO);

constexpr uint8_t VELOCIDAD_MIDI_MIN = 1;
constexpr uint8_t VELOCIDAD_MIDI_MAX = 127;
// En PayloadNota::velocidad: la tecla se soltó (como un note on con velocidad 0 en MIDI).
constexpr uint8_t VELOCIDAD_SOLTO = 0;
constexpr uint8_t NIVEL_SOMBRA_MAX = 100;
constexpr uint8_t INTENSIDAD_AGITAR_MAX = 100;
// En PayloadLuz::led: prende todos los LEDs del animal.
constexpr uint8_t LED_TODOS = 0xFF;

#pragma pack(push, 1)

struct Cabecera {
  uint8_t version;
  uint8_t tipo;
  uint8_t nodo_emisor;
  uint16_t arranque;
  uint32_t seq;
};

struct PayloadNota {
  uint8_t nota;
  uint8_t velocidad;  // VELOCIDAD_MIDI_MIN..MAX al presionar, VELOCIDAD_SOLTO al soltar
};

struct PayloadSombra {
  uint8_t nivel;  // 0 = flor descubierta, NIVEL_SOMBRA_MAX = totalmente tapada
};

// El SW-520D solo da 0/1: el pollito calcula la intensidad a partir de
// cuántas veces cambia el sensor en una ventana de tiempo.
struct PayloadAgitar {
  uint8_t intensidad;  // 0..INTENSIDAD_AGITAR_MAX
};

struct PayloadModo {
  uint8_t modo;
};

// Simón Dice. Viaja con la clave de grupo y lo reciben todos: cada animal
// ignora los que no traen su nodo_destino.
struct PayloadLuz {
  uint8_t nodo_destino;  // Nodo
  uint8_t led;           // gatito: aEntero(Nota); ranita: 0 izq, 1 der; pollito: 0; o LED_TODOS
  uint16_t duracion_ms;
};

#pragma pack(pop)

constexpr size_t NONCE_BYTES = 12;
constexpr size_t TAG_BYTES = 16;
constexpr size_t PAYLOAD_MAX = 8;  // margen para crecer sin cambiar MENSAJE_MAX
constexpr size_t MENSAJE_MAX = sizeof(Cabecera) + NONCE_BYTES + PAYLOAD_MAX + TAG_BYTES;

static_assert(sizeof(Cabecera) == 9, "La cabecera cambió de tamaño: sube VERSION");
static_assert(MENSAJE_MAX <= ESP_NOW_MAX_DATA_LEN, "El mensaje excede el límite de ESP-NOW");

// Liga cada payload con su TipoMensaje, para que el código de envío deduzca el
// tipo del struct y no se pueda mandar un PayloadNota marcado como Sombra.
// Un payload sin registrar aquí no compila.
template <typename T>
struct TipoDe;

template <typename T, TipoMensaje TIPO>
struct RegistroPayload {
  static_assert(sizeof(T) <= PAYLOAD_MAX, "El payload no cabe en PAYLOAD_MAX");
  static constexpr TipoMensaje valor = TIPO;
};

template <>
struct TipoDe<PayloadNota> : RegistroPayload<PayloadNota, TipoMensaje::Nota> {};
template <>
struct TipoDe<PayloadSombra> : RegistroPayload<PayloadSombra, TipoMensaje::Sombra> {};
template <>
struct TipoDe<PayloadAgitar> : RegistroPayload<PayloadAgitar, TipoMensaje::Agitar> {};
template <>
struct TipoDe<PayloadModo> : RegistroPayload<PayloadModo, TipoMensaje::Modo> {};
template <>
struct TipoDe<PayloadLuz> : RegistroPayload<PayloadLuz, TipoMensaje::Luz> {};

constexpr uint32_t LATIDO_CADA_MS = 1000;
constexpr uint32_t DESCONECTADO_TRAS_MS = 5000;

}  // namespace protocolo
