// Eventos que esperan el acuse de Node-RED. Un evento solo se borra cuando llega
// orquesta/sync/ack con su arranque y hasta_seq >= su seq; publicarlo no basta (PubSubClient es QoS 0).
//
// Los primeros config::CAPACIDAD_COLA_MQTT viven en RAM. Si hay un Desborde (LittleFS en el osito),
// lo que no cabe se escribe ahí en orden y vuelve a la RAM conforme Node-RED confirma. Sin desborde,
// o si el desborde también se llena, el evento se descarta y se cuenta.
//
// La parte en RAM se puede exportar e importar (cola_persistente la guarda en LittleFS) para que un
// reinicio del osito no la pierda. Los eventos recuperados conservan su arranque, por eso cada entrada
// lo lleva y el acuse dice de qué arranque es.
// No usa Arduino.
#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"

namespace cola_eventos {
struct Registro {
  uint32_t arranque;
  uint32_t seq;
  uint8_t nodo;  // aEntero(Nodo)
  char json[config::EVENTO_JSON_MAX];
};

// Donde va lo que no cabe en RAM. Primero en entrar, primero en salir.
class Desborde {
 public:
  virtual ~Desborde() = default;
  virtual bool meter(const Registro& registro) = 0;  // false si está lleno o falló la escritura
  virtual bool sacar(Registro& registro) = 0;        // el más viejo
  virtual size_t cantidad() const = 0;
};

void reiniciar();
void usarDesborde(Desborde* desborde);  // nullptr = sin desborde

uint32_t reservarSeq();  // el seq que lleva el siguiente evento
void agregar(uint32_t arranque, uint32_t seq, uint8_t nodo, const char* json);

// El más viejo que todavía no se ha publicado en esta vuelta.
bool siguientePorPublicar(uint32_t& arranque, uint32_t& seq, uint8_t& nodo, const char*& json);
void marcarPublicado(uint32_t arranque, uint32_t seq);
// Al reconectar, o si el acuse no llega, se vuelve a publicar todo lo pendiente
// (Node-RED descarta duplicados por arranque y seq).
void republicarTodo();

// Borra los publicados de ese arranque con seq <= hasta_seq. Devuelve cuántos borró.
size_t confirmarHasta(uint32_t arranque, uint32_t hasta_seq);
size_t pendientes();  // RAM + desborde
size_t enRam();
uint32_t descartados();
bool contieneArranque(uint32_t arranque);

// El más viejo en RAM; false si está vacía. Sirve para saber si los acuses dejaron de llegar.
bool frente(uint32_t& arranque, uint32_t& seq);
// Crece cada vez que cambia lo que hay en RAM.
uint32_t version();

// Copia de la RAM para guardarla. Devuelve los bytes escritos, o 0 si no cupo en `tamano`.
size_t exportar(uint8_t* destino, size_t tamano);
// Agrega lo exportado al final de la RAM, sin marcarlo como publicado. false si el formato no es válido.
bool importar(const uint8_t* origen, size_t tamano);
constexpr size_t EXPORTAR_MAX = 8 + config::CAPACIDAD_COLA_MQTT * (10 + config::EVENTO_JSON_MAX);
}  // namespace cola_eventos
