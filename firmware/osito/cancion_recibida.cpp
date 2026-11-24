#include "cancion_recibida.h"

#include <cstdlib>
#include <cstring>

#include "config.h"
#include "contrato_mqtt.h"

namespace cancion_recibida {
namespace {
using canciones::Cancion;
using canciones::Paso;

struct Espacio {
  bool lista = false;
  uint32_t orden = 0;  // para saber cuál es la más nueva
  char nombre[contrato_mqtt::CANCION_MAX] = {};
  Paso pasos[protocolo::NUM_NODOS][config::CANCION_PASOS_MAX] = {};
  Cancion cancion = {};
};
// Uno puede estar sonando mientras llega otra.
Espacio espacios[2];
Espacio* armando = nullptr;
int32_t carga_actual = 0;
uint32_t siguiente_orden = 1;

bool nombreValido(const char* nombre) {
  const size_t largo = nombre == nullptr ? 0 : strlen(nombre);
  if (largo == 0 || largo >= contrato_mqtt::CANCION_MAX) {
    return false;
  }
  for (size_t i = 0; i < largo; ++i) {
    const char c = nombre[i];
    if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-') {
      return false;
    }
  }
  return true;
}

// Lee un número y deja `p` en el separador que sigue.
bool leerNumero(const char*& p, unsigned long maximo, uint8_t& valor) {
  if (*p < '0' || *p > '9') {
    return false;
  }
  char* fin = nullptr;
  const unsigned long n = strtoul(p, &fin, 10);
  if (n > maximo) {
    return false;
  }
  valor = static_cast<uint8_t>(n);
  p = fin;
  return true;
}

bool leerPaso(const char*& p, Paso& paso) {
  return leerNumero(p, 127, paso.nota_midi) && *p++ == '.' &&
         leerNumero(p, config::CANCION_PULSOS_MAX, paso.pulsos) && paso.pulsos > 0 &&
         *p++ == '.' && leerNumero(p, 127, paso.velocidad);
}

bool descartar() {
  armando = nullptr;
  return false;
}
}  // namespace

void reiniciar() {
  for (Espacio& espacio : espacios) {
    espacio.lista = false;
  }
  armando = nullptr;
}

bool empezar(int32_t carga, const char* nombre, uint32_t pulso_ms, const Cancion* en_uso) {
  if (!nombreValido(nombre) || pulso_ms < config::CANCION_PULSO_MIN_MS ||
      pulso_ms > config::CANCION_PULSO_MAX_MS) {
    return descartar();
  }
  armando = &espacios[0].cancion == en_uso ? &espacios[1] : &espacios[0];
  armando->lista = false;
  strcpy(armando->nombre, nombre);
  armando->cancion.nombre = armando->nombre;
  armando->cancion.pulso_ms = static_cast<uint16_t>(pulso_ms);
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    armando->cancion.partes[i] = {armando->pasos[i], 0};
  }
  carga_actual = carga;
  return true;
}

bool agregar(int32_t carga, uint32_t parte, uint32_t desde, const char* pasos) {
  if (armando == nullptr || carga != carga_actual) {
    return false;  // de otra carga: no toca la que se está armando
  }
  if (parte >= protocolo::NUM_NODOS || pasos == nullptr) {
    return descartar();
  }
  size_t& van = armando->cancion.partes[parte].num_pasos;
  if (desde < van) {
    return true;  // repetido
  }
  if (desde > van) {
    return descartar();  // se perdió uno
  }
  const char* p = pasos;
  while (true) {
    Paso paso;
    if (van == config::CANCION_PASOS_MAX || !leerPaso(p, paso)) {
      return descartar();
    }
    armando->pasos[parte][van++] = paso;
    if (*p == '\0') {
      return true;
    }
    if (*p++ != ',') {
      return descartar();
    }
  }
}

bool terminar(int32_t carga, const uint32_t (&totales)[protocolo::NUM_NODOS]) {
  if (armando == nullptr || carga != carga_actual) {
    return false;
  }
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    if (totales[i] == 0 || armando->cancion.partes[i].num_pasos != totales[i]) {
      return descartar();
    }
  }
  armando->lista = true;
  armando->orden = siguiente_orden++;
  armando = nullptr;
  return true;
}

const Cancion* buscar(const char* nombre) {
  const Espacio* encontrada = nullptr;
  for (const Espacio& espacio : espacios) {
    if (espacio.lista && nombre != nullptr && strcmp(espacio.nombre, nombre) == 0 &&
        (encontrada == nullptr || espacio.orden > encontrada->orden)) {
      encontrada = &espacio;
    }
  }
  return encontrada == nullptr ? nullptr : &encontrada->cancion;
}
}  // namespace cancion_recibida
