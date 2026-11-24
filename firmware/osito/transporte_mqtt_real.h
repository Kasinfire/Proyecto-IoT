// Cliente MQTTS del osito. Las credenciales se leen de Preferences; el certificado CA es público.
// Wi-Fi, NTP, TLS y PubSubClient viven en una tarea propia (núcleo 0): un saludo TLS lento o un
// broker caído no congelan la música, ESP-NOW ni el teclado. El loop solo intercambia mensajes
// con esa tarea por colas de FreeRTOS.
#pragma once

// Primero los de OrquestaComun: los headers de TLS del ESP-IDF definen la macro TAG_BYTES y
// romperían cifrado.h y protocolo.h si llegan antes.
#include <cifrado.h>
#include <protocolo.h>

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "config.h"
#include "transporte_mqtt.h"

class TransporteMqttReal : public TransporteMqtt {
 public:
  void iniciar(AlMensaje al_mensaje) override;
  void actualizar() override;  // loop: entrega lo recibido
  bool conectado() override;
  bool publicar(const char* topico, const char* payload, bool retenido) override;
  const char* nombre() const override {
    return "MQTTS";
  }
  bool esSimulado() const override {
    return false;
  }
  bool tieneCredenciales() const {
    return credenciales_validas_;
  }
  bool wifiConectado() const;

 private:
  static constexpr size_t TOPICO_MAX = 48;
  static constexpr size_t SALIENTE_MAX = 320;  // JSON cifrado en Base64 + envoltura
  static constexpr size_t ENTRANTE_MAX = 128;

  struct Saliente {
    char topico[TOPICO_MAX];
    char payload[SALIENTE_MAX];
    bool retenido;
  };
  struct Entrante {
    char topico[TOPICO_MAX];
    char payload[ENTRANTE_MAX];
  };

  static void tarea(void* yo);
  static void recibir(char* topico, uint8_t* payload, unsigned int tamano);
  void vueltaDeTarea();
  bool leerCredenciales();
  bool conectarBroker();
  void buscarRed();
  bool cifrarEvento(const char* topico, const char* json, char* destino, size_t tamano);

  WiFiClientSecure tls_;
  PubSubClient cliente_{tls_};
  AlMensaje al_mensaje_ = nullptr;
  String ssid_;
  String clave_wifi_;
  String host_;
  String usuario_;
  String clave_mqtt_;
  uint8_t clave_aes_[16] = {};
  bool credenciales_validas_ = false;

  QueueHandle_t salientes_ = nullptr;
  QueueHandle_t entrantes_ = nullptr;
  std::atomic<bool> mqtt_conectado_{false};

  // Solo los usa la tarea.
  bool ntp_iniciado_ = false;
  bool reporto_wifi_ = false;
  uint32_t ultimo_intento_wifi_ms_ = 0;
  uint32_t espera_wifi_ms_ = config::REINTENTO_WIFI_MS;
  bool busco_red_ = false;
  uint32_t ultimo_intento_mqtt_ms_ = 0;
};
