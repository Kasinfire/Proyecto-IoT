#include "simon.h"

#include "config.h"
#include "estado_animales.h"

namespace simon {
namespace {
using protocolo::Nodo;

enum class Estado : uint8_t { Pausa, Esperando };

Estado estado = Estado::Pausa;
uint32_t desde_ms = 0;
uint32_t aleatorio = 1;
protocolo::PayloadLuz pedido_actual = {};
uint16_t num_aciertos = 0;
uint16_t num_errores = 0;

// Generador congruencial: basta para variar el juego y se puede repetir en las pruebas.
uint32_t siguienteAleatorio() {
  aleatorio = aleatorio * 1664525u + 1013904223u;
  return aleatorio >> 8;
}

bool elegir(uint32_t ahora_ms, protocolo::PayloadLuz& pedido) {
  Nodo candidatos[protocolo::NUM_NODOS];
  size_t cuantos = 0;
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    const Nodo nodo = static_cast<Nodo>(i);
    // Solo animales conectados: pedirle algo a uno apagado sería un error seguro para el niño.
    if (nodo != Nodo::Osito && estado_animales::estaConectado(nodo, ahora_ms)) {
      candidatos[cuantos++] = nodo;
    }
  }
  if (cuantos == 0) {
    return false;
  }
  const Nodo elegido = candidatos[siguienteAleatorio() % cuantos];
  uint8_t led = protocolo::LED_TODOS;
  if (elegido == Nodo::Gatito) {
    led = siguienteAleatorio() % protocolo::NUM_NOTAS;
  } else if (elegido == Nodo::Pollito) {
    led = 0;  // el pollito tiene un solo LED
  }
  pedido = {protocolo::aEntero(elegido), led, config::SIMON_LUZ_MS};
  return true;
}

Resultado terminar(Resultado resultado, uint32_t ahora_ms) {
  if (resultado == Resultado::Acierto) {
    ++num_aciertos;
  } else {
    ++num_errores;
  }
  estado = Estado::Pausa;
  desde_ms = ahora_ms;
  return resultado;
}
}  // namespace

void iniciar(uint32_t semilla, uint32_t ahora_ms) {
  aleatorio = semilla == 0 ? 1 : semilla;
  estado = Estado::Pausa;
  desde_ms = ahora_ms;
  num_aciertos = 0;
  num_errores = 0;
}

bool actualizar(uint32_t ahora_ms, protocolo::PayloadLuz& pedido, Resultado& resultado) {
  resultado = Resultado::Nada;
  switch (estado) {
    case Estado::Pausa:
      if (ahora_ms - desde_ms >= config::SIMON_PAUSA_MS && elegir(ahora_ms, pedido)) {
        pedido_actual = pedido;
        estado = Estado::Esperando;
        desde_ms = ahora_ms;
        return true;
      }
      break;
    case Estado::Esperando:
      if (ahora_ms - desde_ms >= config::SIMON_ESPERA_MS) {
        resultado = terminar(Resultado::TiempoAgotado, ahora_ms);
      }
      break;
  }
  return false;
}

Resultado alEvento(const interprete::Evento& evento, uint32_t ahora_ms) {
  const bool es_inicio =
      evento.es_gesto && !(evento.clase == interprete::Clase::Nota &&
                           evento.velocidad == protocolo::VELOCIDAD_SOLTO);
  if (estado != Estado::Esperando || !es_inicio) {
    return Resultado::Nada;
  }
  const bool animal_correcto = protocolo::aEntero(evento.nodo) == pedido_actual.nodo_destino;
  const bool tecla_correcta = evento.clase != interprete::Clase::Nota ||
                              pedido_actual.led == protocolo::LED_TODOS ||
                              evento.valor == pedido_actual.led;
  return terminar(animal_correcto && tecla_correcta ? Resultado::Acierto : Resultado::Error,
                  ahora_ms);
}

uint16_t aciertos() {
  return num_aciertos;
}

uint16_t errores() {
  return num_errores;
}
}  // namespace simon
