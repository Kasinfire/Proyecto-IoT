// Rechaza mensajes repetidos o viejos: por emisor guarda el último (arranque, seq) aceptado
// y solo acepta uno mayor. Vive en RAM, así que se olvida si el receptor se reinicia
// (abierto, va en la lista de OWASP del reporte).
#pragma once

#include <cstdint>

#include "protocolo.h"

class ContraReplay {
 public:
  bool aceptar(uint8_t emisor, uint16_t arranque, uint32_t seq) {
    if (emisor >= protocolo::NUM_NODOS) {
      return false;
    }
    Ultimo& ultimo = ultimos_[emisor];
    const bool es_nuevo = !ultimo.hay || arranque > ultimo.arranque ||
                          (arranque == ultimo.arranque && seq > ultimo.seq);
    if (es_nuevo) {
      ultimo = {true, arranque, seq};
    }
    return es_nuevo;
  }

 private:
  struct Ultimo {
    bool hay;
    uint16_t arranque;
    uint32_t seq;
  };
  Ultimo ultimos_[protocolo::NUM_NODOS] = {};
};
