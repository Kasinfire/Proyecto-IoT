// Mensajes ESP-NOW cifrados con AES-128-GCM entre los animales y el osito.
// Las claves, la MAC del osito y el canal se leen de NVS: primero hay que subir
// firmware/aprovisionar a cada placa.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "protocolo.h"

namespace enlace {

// Un mensaje que ya pasó el tag de AES-GCM y la revisión contra replay.
struct Mensaje {
  protocolo::Cabecera cabecera;
  uint8_t payload[protocolo::PAYLOAD_MAX];
  uint8_t tamano;

  protocolo::Nodo emisor() const {
    return static_cast<protocolo::Nodo>(cabecera.nodo_emisor);
  }
  protocolo::TipoMensaje tipo() const {
    return static_cast<protocolo::TipoMensaje>(cabecera.tipo);
  }

  // false si el mensaje es de otro tipo, así no se lee un PayloadModo como PayloadLuz.
  template <typename T>
  bool leer(T& destino) const {
    if (cabecera.tipo != protocolo::aEntero(protocolo::TipoDe<T>::valor) || tamano != sizeof(T)) {
      return false;
    }
    memcpy(&destino, payload, sizeof(T));
    return true;
  }
};

// false si falta algo en NVS (dice qué por Serial).
// Animales: llamarlo antes de cualquier otra cosa de Wi-Fi.
// Osito: llamarlo después de conectarse al router, para usar su mismo canal.
bool iniciar(protocolo::Nodo yo);
void actualizar();  // animales: manda el latido cada protocolo::LATIDO_CADA_MS

bool enviarCrudo(protocolo::TipoMensaje tipo, const void* datos, size_t tamano);

// El tipo del mensaje sale del struct, así no se puede mandar un PayloadNota marcado como Sombra.
template <typename T>
bool enviar(const T& payload) {
  return enviarCrudo(protocolo::TipoDe<T>::valor, &payload, sizeof(T));
}

bool enviarLatido();

// Saca el siguiente mensaje válido; los alterados, repetidos o ajenos se descartan aquí.
bool recibir(Mensaje& mensaje);

// Si ese nodo mandó algo válido en los últimos protocolo::DESCONECTADO_TRAS_MS.
bool estaConectado(protocolo::Nodo nodo);
}  // namespace enlace
