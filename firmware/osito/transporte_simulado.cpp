#include "transporte_simulado.h"

#include <Arduino.h>

void TransporteSimulado::iniciar(AlMensaje al_mensaje) {
  al_mensaje_ = al_mensaje;
  Serial.println("[MQTT-SIMULACIÓN] Sin broker real configurado; nada sale de la placa");
}

bool TransporteSimulado::publicar(const char* topico, const char* payload, bool retenido) {
  if (!conectado_) {
    return false;
  }
  Serial.printf("[MQTT-SIMULACIÓN] -> %s %s%s\n", topico, payload, retenido ? " (retenido)" : "");
  return true;
}

void TransporteSimulado::conectar(bool si) {
  conectado_ = si;
  Serial.printf("[MQTT-SIMULACIÓN] broker %s\n", si ? "conectado" : "desconectado");
}

void TransporteSimulado::recibir(const char* topico, const char* payload) {
  Serial.printf("[MQTT-SIMULACIÓN] <- %s %s\n", topico, payload);
  if (conectado_ && al_mensaje_ != nullptr) {
    al_mensaje_(topico, payload);
  }
}
