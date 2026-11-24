// Broker de mentira para probar los contratos MQTT con una sola placa: no usa Wi-Fi, solo
// imprime lo que se publicaría. Arranca desconectado; se maneja con "sim mqtt ..." (simulacion.h).
#pragma once

#include "transporte_mqtt.h"

class TransporteSimulado : public TransporteMqtt {
 public:
  void iniciar(AlMensaje al_mensaje) override;
  void actualizar() override {}
  bool conectado() override {
    return conectado_;
  }
  bool publicar(const char* topico, const char* payload, bool retenido) override;
  const char* nombre() const override {
    return "MQTT-SIMULACIÓN";
  }
  bool esSimulado() const override {
    return true;
  }

  void conectar(bool si);
  void recibir(const char* topico, const char* payload);  // como si llegara de Node-RED

 private:
  AlMensaje al_mensaje_ = nullptr;
  bool conectado_ = false;
};
