// Lo único que el resto del osito sabe de la conexión al broker. Hoy solo existe
// TransporteSimulado; la conexión real queda aislada hasta tener los datos que faltan.
//
// La implementación real (TransporteMqtts) debe, según la sección 7 de la especificación:
//   - Conectar el Wi-Fi con SSID y contraseña leídos de NVS (nunca en el código).
//   - Ajustar la hora por NTP ANTES de conectar al broker (si no, el certificado no valida).
//   - Usar WiFiClientSecure con setCACert(<CA propia>) y el puerto 8883. Nunca setInsecure().
//   - PubSubClient con usuario y contraseña de NVS, LWT "OFFLINE" retenido en orquesta/osito/estado.
//   - Suscribirse a orquesta/control/modo y orquesta/sync/ack.
//   - Cifrar el cuerpo de cada evento con AES-128-GCM y mandarlo en Base64.
#pragma once

#include <cstdint>

class TransporteMqtt {
 public:
  using AlMensaje = void (*)(const char* topico, const char* payload);

  virtual ~TransporteMqtt() = default;
  virtual void iniciar(AlMensaje al_mensaje) = 0;
  virtual void actualizar() = 0;
  virtual bool conectado() const = 0;
  virtual bool publicar(const char* topico, const char* payload, bool retenido) = 0;
  virtual const char* nombre() const = 0;  // para el monitor serie
  // Un evento SIMULADO solo se publica en un transporte simulado, nunca en el broker real.
  virtual bool esSimulado() const = 0;
};
