// Provisional: ver enlace.h.
#include <Arduino.h>

#include "enlace.h"

namespace enlace {
namespace {
uint32_t ultimo_latido_ms = 0;
}  // namespace

void iniciar() {
  Serial.println("[enlace] Provisional: los mensajes solo se imprimen");
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  if (ahora_ms - ultimo_latido_ms >= protocolo::LATIDO_CADA_MS) {
    ultimo_latido_ms = ahora_ms;
    Serial.println("[enlace] latido");
  }
}

bool enviar(const protocolo::PayloadSombra& payload) {
  Serial.printf("[enlace] sombra nivel=%u\n", payload.nivel);
  return true;
}
}  // namespace enlace
