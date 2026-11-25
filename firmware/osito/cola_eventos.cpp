#include "cola_eventos.h"

#include <cstring>

namespace cola_eventos {
namespace {
struct Entrada {
  Registro registro;
  bool publicado;
};

static_assert(config::EVENTO_JSON_MAX <= 256, "el largo del JSON se exporta en un byte");

// Formato de exportar(): "COL1", cantidad (4 B) y por cada entrada:
// arranque (4 B), seq (4 B), nodo (1 B), largo del JSON (1 B) y el JSON sin el '\0'.
constexpr uint8_t MAGIA[4] = {'C', 'O', 'L', '1'};

Entrada entradas[config::CAPACIDAD_COLA_MQTT] = {};
size_t inicio = 0;  // el más viejo
size_t cantidad = 0;
uint32_t proximo_seq = 1;
uint32_t num_descartados = 0;
uint32_t num_version = 0;
Desborde* desborde = nullptr;

Entrada& en(size_t k) {
  return entradas[(inicio + k) % config::CAPACIDAD_COLA_MQTT];
}

void meterEnRam(const Registro& registro) {
  Entrada& nueva = en(cantidad);
  nueva.registro = registro;
  nueva.publicado = false;
  ++cantidad;
  ++num_version;
}

// Lo que espera en el desborde vuelve a la RAM en cuanto hay lugar, en el mismo orden.
void rellenar() {
  Registro registro;
  while (desborde != nullptr && cantidad < config::CAPACIDAD_COLA_MQTT &&
         desborde->cantidad() > 0 && desborde->sacar(registro)) {
    meterEnRam(registro);
  }
}

void escribir32(uint8_t* p, uint32_t valor) {
  for (int i = 0; i < 4; ++i) {
    p[i] = static_cast<uint8_t>(valor >> (8 * i));
  }
}

uint32_t leer32(const uint8_t* p) {
  uint32_t valor = 0;
  for (int i = 0; i < 4; ++i) {
    valor |= static_cast<uint32_t>(p[i]) << (8 * i);
  }
  return valor;
}
}  // namespace

void reiniciar() {
  inicio = 0;
  cantidad = 0;
  proximo_seq = 1;
  num_descartados = 0;
  ++num_version;
}

void usarDesborde(Desborde* d) {
  desborde = d;
  rellenar();
}

uint32_t reservarSeq() {
  return proximo_seq++;
}

void agregar(uint32_t arranque, uint32_t seq, uint8_t nodo, const char* json) {
  Registro registro = {};
  registro.arranque = arranque;
  registro.seq = seq;
  registro.nodo = nodo;
  strncpy(registro.json, json, sizeof(registro.json) - 1);

  rellenar();
  const bool hay_en_desborde = desborde != nullptr && desborde->cantidad() > 0;
  if (cantidad < config::CAPACIDAD_COLA_MQTT && !hay_en_desborde) {
    meterEnRam(registro);
    return;
  }
  // Si ya hay algo esperando en el desborde, lo nuevo va detrás para no adelantarse.
  if (desborde != nullptr && desborde->meter(registro)) {
    return;
  }
  if (desborde != nullptr) {
    // El desborde está lleno: se pierde el nuevo (meterlo en RAM lo adelantaría).
    ++num_descartados;
    return;
  }
  // Sin desborde: se descarta el más viejo, como antes.
  inicio = (inicio + 1) % config::CAPACIDAD_COLA_MQTT;
  --cantidad;
  ++num_descartados;
  meterEnRam(registro);
}

bool siguientePorPublicar(uint32_t& arranque, uint32_t& seq, uint8_t& nodo, const char*& json) {
  for (size_t k = 0; k < cantidad; ++k) {
    if (!en(k).publicado) {
      arranque = en(k).registro.arranque;
      seq = en(k).registro.seq;
      nodo = en(k).registro.nodo;
      json = en(k).registro.json;
      return true;
    }
  }
  return false;
}

void marcarPublicado(uint32_t arranque, uint32_t seq) {
  for (size_t k = 0; k < cantidad; ++k) {
    if (en(k).registro.arranque == arranque && en(k).registro.seq == seq) {
      en(k).publicado = true;
    }
  }
}

void republicarTodo() {
  for (size_t k = 0; k < cantidad; ++k) {
    en(k).publicado = false;
  }
}

size_t confirmarHasta(uint32_t arranque, uint32_t hasta_seq) {
  // Uno que nunca se publicó no se borra aunque el acuse lo cubra: Node-RED no pudo haberlo
  // recibido. Puede haber dos arranques mezclados, así que se compacta en lugar de quitar del frente.
  size_t quedan = 0;
  for (size_t k = 0; k < cantidad; ++k) {
    const Entrada& e = en(k);
    const bool confirmado =
        e.publicado && e.registro.arranque == arranque && e.registro.seq <= hasta_seq;
    if (!confirmado) {
      if (quedan != k) {
        en(quedan) = e;
      }
      ++quedan;
    }
  }
  const size_t borrados = cantidad - quedan;
  cantidad = quedan;
  if (borrados > 0) {
    ++num_version;
  }
  rellenar();
  return borrados;
}

size_t pendientes() {
  return cantidad + (desborde != nullptr ? desborde->cantidad() : 0);
}

size_t enRam() {
  return cantidad;
}

uint32_t descartados() {
  return num_descartados;
}

bool contieneArranque(uint32_t arranque) {
  for (size_t k = 0; k < cantidad; ++k) {
    if (en(k).registro.arranque == arranque) {
      return true;
    }
  }
  return false;
}

bool frente(uint32_t& arranque, uint32_t& seq) {
  if (cantidad == 0) {
    return false;
  }
  arranque = en(0).registro.arranque;
  seq = en(0).registro.seq;
  return true;
}

uint32_t version() {
  return num_version;
}

size_t exportar(uint8_t* destino, size_t tamano) {
  if (tamano < 8) {
    return 0;
  }
  memcpy(destino, MAGIA, sizeof(MAGIA));
  escribir32(destino + 4, static_cast<uint32_t>(cantidad));
  size_t usados = 8;
  for (size_t k = 0; k < cantidad; ++k) {
    const Registro& r = en(k).registro;
    const size_t largo = strnlen(r.json, sizeof(r.json) - 1);
    if (usados + 10 + largo > tamano) {
      return 0;
    }
    escribir32(destino + usados, r.arranque);
    escribir32(destino + usados + 4, r.seq);
    destino[usados + 8] = r.nodo;
    destino[usados + 9] = static_cast<uint8_t>(largo);
    memcpy(destino + usados + 10, r.json, largo);
    usados += 10 + largo;
  }
  return usados;
}

bool importar(const uint8_t* origen, size_t tamano) {
  if (tamano < 8 || memcmp(origen, MAGIA, sizeof(MAGIA)) != 0) {
    return false;
  }
  const uint32_t total = leer32(origen + 4);
  // Primero se revisa todo: un archivo a medias no debe meter basura.
  size_t pos = 8;
  for (uint32_t i = 0; i < total; ++i) {
    if (pos + 10 > tamano || origen[pos + 9] >= config::EVENTO_JSON_MAX ||
        pos + 10 + origen[pos + 9] > tamano) {
      return false;
    }
    pos += 10 + origen[pos + 9];
  }
  if (pos != tamano || cantidad + total > config::CAPACIDAD_COLA_MQTT) {
    return false;
  }
  pos = 8;
  for (uint32_t i = 0; i < total; ++i) {
    Registro r = {};
    r.arranque = leer32(origen + pos);
    r.seq = leer32(origen + pos + 4);
    r.nodo = origen[pos + 8];
    memcpy(r.json, origen + pos + 10, origen[pos + 9]);
    meterEnRam(r);
    pos += 10 + origen[pos + 9];
  }
  return true;
}
}  // namespace cola_eventos
