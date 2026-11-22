#include "estado_animales.h"

#include "config.h"

namespace estado_animales {
namespace {
using protocolo::Nodo;

struct Animal {
  bool hubo_mensaje[NUM_FUENTES];
  uint32_t ultimo_mensaje_ms[NUM_FUENTES];
  bool conectado_reportado[NUM_FUENTES];
  bool hubo_gesto;
  uint32_t ultimo_gesto_ms;
  uint8_t teclas_sostenidas;  // un bit por tecla
};

Animal animales[protocolo::NUM_NODOS] = {};

bool esAnimal(Nodo nodo) {
  return nodo != Nodo::Osito && protocolo::aEntero(nodo) < protocolo::NUM_NODOS;
}

Animal& de(Nodo nodo) {
  return animales[protocolo::aEntero(nodo)];
}
}  // namespace

void reiniciar() {
  for (Animal& animal : animales) {
    animal = {};
  }
}

void registrarMensaje(Nodo nodo, Fuente fuente, uint32_t ahora_ms) {
  if (!esAnimal(nodo)) {
    return;
  }
  const uint8_t f = protocolo::aEntero(fuente);
  de(nodo).hubo_mensaje[f] = true;
  de(nodo).ultimo_mensaje_ms[f] = ahora_ms;
}

void registrarGesto(Nodo nodo, uint32_t ahora_ms) {
  if (!esAnimal(nodo)) {
    return;
  }
  de(nodo).hubo_gesto = true;
  de(nodo).ultimo_gesto_ms = ahora_ms;
}

void sostenerTecla(Nodo nodo, uint8_t tecla, bool presionada) {
  if (!esAnimal(nodo) || tecla >= protocolo::NUM_NOTAS) {
    return;
  }
  const uint8_t bit = 1u << tecla;
  uint8_t& sostenidas = de(nodo).teclas_sostenidas;
  sostenidas = presionada ? (sostenidas | bit) : (sostenidas & ~bit);
}

bool estaConectado(Nodo nodo, Fuente fuente, uint32_t ahora_ms) {
  if (!esAnimal(nodo)) {
    return false;
  }
  const uint8_t f = protocolo::aEntero(fuente);
  // Si deja de llegar el latido, marco al animal desconectado.
  return de(nodo).hubo_mensaje[f] &&
         ahora_ms - de(nodo).ultimo_mensaje_ms[f] < protocolo::DESCONECTADO_TRAS_MS;
}

bool estaConectado(Nodo nodo, uint32_t ahora_ms) {
  return estaConectado(nodo, Fuente::Radio, ahora_ms) ||
         estaConectado(nodo, Fuente::Simulacion, ahora_ms);
}

bool estaActivo(Nodo nodo, uint32_t ahora_ms) {
  // Sin conexión no cuenta como activo, así una tecla "sostenida" de un gatito apagado no
  // deja su parte sonando para siempre.
  if (!estaConectado(nodo, ahora_ms)) {
    return false;
  }
  const Animal& animal = de(nodo);
  return animal.teclas_sostenidas != 0 ||
         (animal.hubo_gesto && ahora_ms - animal.ultimo_gesto_ms < config::ACTIVO_TRAS_GESTO_MS);
}

bool seDesconecto(Nodo nodo, uint32_t ahora_ms) {
  if (!esAnimal(nodo)) {
    return false;
  }
  const Animal& animal = de(nodo);
  const bool alguna_vez = animal.hubo_mensaje[0] || animal.hubo_mensaje[1];
  return alguna_vez && !estaConectado(nodo, ahora_ms);
}

bool algunoSeDesconecto(uint32_t ahora_ms) {
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    if (seDesconecto(static_cast<Nodo>(i), ahora_ms)) {
      return true;
    }
  }
  return false;
}

bool sacarCambio(uint32_t ahora_ms, CambioConexion& cambio) {
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    const Nodo nodo = static_cast<Nodo>(i);
    if (!esAnimal(nodo)) {
      continue;
    }
    for (size_t f = 0; f < NUM_FUENTES; ++f) {
      const Fuente fuente = static_cast<Fuente>(f);
      const bool ahora_conectado = estaConectado(nodo, fuente, ahora_ms);
      bool& reportado = animales[i].conectado_reportado[f];
      if (ahora_conectado != reportado) {
        reportado = ahora_conectado;
        if (!ahora_conectado) {
          animales[i].teclas_sostenidas = 0;
        }
        cambio = {nodo, fuente, ahora_conectado};
        return true;
      }
    }
  }
  return false;
}
}  // namespace estado_animales
