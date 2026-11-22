#include "cola_eventos.h"

#include <cstring>

#include "config.h"

namespace cola_eventos {
namespace {
struct Entrada {
  uint32_t seq;
  uint8_t nodo;
  bool publicado;
  char json[config::EVENTO_JSON_MAX];
};

Entrada entradas[config::CAPACIDAD_COLA_MQTT] = {};
size_t inicio = 0;  // el más viejo
size_t cantidad = 0;
uint32_t proximo_seq = 1;
uint32_t num_descartados = 0;

Entrada& en(size_t k) {
  return entradas[(inicio + k) % config::CAPACIDAD_COLA_MQTT];
}
}  // namespace

void reiniciar() {
  inicio = 0;
  cantidad = 0;
  proximo_seq = 1;
  num_descartados = 0;
}

uint32_t reservarSeq() {
  return proximo_seq++;
}

void agregar(uint32_t seq, uint8_t nodo, const char* json) {
  if (cantidad == config::CAPACIDAD_COLA_MQTT) {
    inicio = (inicio + 1) % config::CAPACIDAD_COLA_MQTT;
    --cantidad;
    ++num_descartados;
  }
  Entrada& nueva = en(cantidad);
  nueva.seq = seq;
  nueva.nodo = nodo;
  nueva.publicado = false;
  strncpy(nueva.json, json, sizeof(nueva.json) - 1);
  nueva.json[sizeof(nueva.json) - 1] = '\0';
  ++cantidad;
}

bool siguientePorPublicar(uint32_t& seq, uint8_t& nodo, const char*& json) {
  for (size_t k = 0; k < cantidad; ++k) {
    if (!en(k).publicado) {
      seq = en(k).seq;
      nodo = en(k).nodo;
      json = en(k).json;
      return true;
    }
  }
  return false;
}

void marcarPublicado(uint32_t seq) {
  for (size_t k = 0; k < cantidad; ++k) {
    if (en(k).seq == seq) {
      en(k).publicado = true;
    }
  }
}

void republicarTodo() {
  for (size_t k = 0; k < cantidad; ++k) {
    en(k).publicado = false;
  }
}

size_t confirmarHasta(uint32_t hasta_seq) {
  // Los seq van en orden, así que basta con quitar del frente. Uno que nunca se publicó no se
  // borra aunque el acuse lo cubra: Node-RED no pudo haberlo recibido.
  size_t borrados = 0;
  while (cantidad > 0 && en(0).seq <= hasta_seq && en(0).publicado) {
    inicio = (inicio + 1) % config::CAPACIDAD_COLA_MQTT;
    --cantidad;
    ++borrados;
  }
  return borrados;
}

size_t pendientes() {
  return cantidad;
}

uint32_t descartados() {
  return num_descartados;
}
}  // namespace cola_eventos
