// Interfaz del transporte MQTT. TransporteMqttReal aplica TLS y lee los secretos desde NVS;
// TransporteSimulado queda para pruebas manuales y no comparte el flujo de producción.
#pragma once

#include <cstdint>

class TransporteMqtt {
 public:
  using AlMensaje = void (*)(const char* topico, const char* payload);

  virtual ~TransporteMqtt() = default;
  virtual void iniciar(AlMensaje al_mensaje) = 0;
  virtual void actualizar() = 0;
  virtual bool conectado() = 0;
  virtual bool publicar(const char* topico, const char* payload, bool retenido) = 0;
  virtual const char* nombre() const = 0;  // para el monitor serie
  // Un evento SIMULADO solo se publica en un transporte simulado, nunca en el broker real.
  virtual bool esSimulado() const = 0;
};
