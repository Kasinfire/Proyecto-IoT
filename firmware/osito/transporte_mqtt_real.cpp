// Wi-Fi, NTP y MQTTS con CA propia, en una tarea propia: la música y ESP-NOW no dependen del
// estado del broker.
#include "transporte_mqtt_real.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

#include <cifrado.h>
#include <protocolo.h>
#include <mbedtls/base64.h>

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "ca_cert.h"
#include "config.h"
#include "contrato_mqtt.h"

namespace {
constexpr char ESPACIO_NVS[] = "mqtt";
constexpr char TOPICO_ESTADO_OSITO[] = "orquesta/osito/estado";
// La tarea de red: el saludo TLS (RSA) necesita pila holgada.
constexpr uint32_t PILA_TAREA = 8192;
constexpr UBaseType_t PRIORIDAD_TAREA = 1;
constexpr BaseType_t NUCLEO_RED = 0;  // el loop de Arduino corre en el 1
constexpr size_t SALIENTES_EN_COLA = 16;
constexpr size_t ENTRANTES_EN_COLA = 16;  // una canción llega en varios trozos
constexpr uint32_t VUELTA_TAREA_MS = 10;
// Sin esto WiFiClientSecure espera hasta 30 s al conectar y 120 s en el saludo TLS.
constexpr uint32_t CONEXION_TCP_MS = 5000;
constexpr unsigned long SALUDO_TLS_S = 10;
// Con picos de 3 s en el Wi-Fi, una espera corta da por caída una conexión que solo va lenta.
constexpr uint16_t ESPERA_MQTT_S = 10;
constexpr size_t MENSAJES_POR_VUELTA = 8;
constexpr size_t TAG_CIFRADO_BYTES = 16;
constexpr size_t SOBRE_MAX = cifrado::NONCE_BYTES + config::EVENTO_JSON_MAX + TAG_CIFRADO_BYTES;
constexpr size_t BASE64_MAX = ((SOBRE_MAX + 2) / 3) * 4 + 1;

TransporteMqttReal* instancia = nullptr;

// Razón que dio el router la última vez que no dejó entrar (la escribe el evento de Wi-Fi).
std::atomic<uint16_t> ultima_razon_wifi{0};

// Para no adivinar por qué no entra. El ESP32 solo ve redes de 2.4 GHz.
const char* pistaWifi(uint16_t razon) {
  switch (razon) {
    case WIFI_REASON_NO_AP_FOUND:
      return "no encuentra la red: ¿es solo de 5 GHz, está lejos o el nombre está mal?";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
      return "la red usa otra seguridad: ¿es abierta y se grabó contraseña, o es WPA3/empresarial?";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_FAIL:
      return "contraseña equivocada";
    case 0:
      return "todavía buscando la red";
    default:
      return "el router lo rechazó";
  }
}

bool extraerSecuencia(const char* json, uint32_t& seq) {
  const char* campo = strstr(json, "\"seq\":");
  if (campo == nullptr) {
    return false;
  }
  campo += sizeof("\"seq\":") - 1;
  char* fin = nullptr;
  const unsigned long valor = strtoul(campo, &fin, 10);
  if (fin == campo || (*fin != ',' && *fin != '}') || valor > UINT32_MAX) {
    return false;
  }
  seq = static_cast<uint32_t>(valor);
  return true;
}

bool nodoDeTopico(const char* topico, char* nodo, size_t tamano) {
  constexpr char PREFIJO[] = "orquesta/";
  constexpr char SUFIJO[] = "/evento";
  if (strncmp(topico, PREFIJO, sizeof(PREFIJO) - 1) != 0) {
    return false;
  }
  const char* inicio = topico + sizeof(PREFIJO) - 1;
  const char* barra = strchr(inicio, '/');
  if (barra == nullptr || strcmp(barra, SUFIJO) != 0) {
    return false;
  }
  const size_t largo = static_cast<size_t>(barra - inicio);
  if (largo == 0 || largo >= tamano) {
    return false;
  }
  memcpy(nodo, inicio, largo);
  nodo[largo] = '\0';
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    if (strcmp(nodo, protocolo::NOMBRE_NODO[i]) == 0) {
      return true;
    }
  }
  return false;
}
}  // namespace

void TransporteMqttReal::iniciar(AlMensaje al_mensaje) {
  al_mensaje_ = al_mensaje;
  instancia = this;
  if (!leerCredenciales()) {
    Serial.println("[MQTTS] Faltan datos en NVS; usa firmware/aprovisionar para cargarlos");
    return;
  }
  cliente_.setCallback(recibir);
  cliente_.setBufferSize(config::BUFFER_MQTT_BYTES);
  cliente_.setKeepAlive(30);
  cliente_.setSocketTimeout(ESPERA_MQTT_S);
  tls_.setCACert(CA_CERT_PEM);
  tls_.setConnectionTimeout(CONEXION_TCP_MS);
  tls_.setHandshakeTimeout(SALUDO_TLS_S);

  salientes_ = xQueueCreate(SALIENTES_EN_COLA, sizeof(Saliente));
  entrantes_ = xQueueCreate(ENTRANTES_EN_COLA, sizeof(Entrante));
  if (salientes_ == nullptr || entrantes_ == nullptr) {
    Serial.println("[MQTTS] Sin memoria para las colas; MQTT queda apagado");
    credenciales_validas_ = false;
    return;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  // Con el ahorro de energía del Wi-Fi la radio "duerme" entre beacons y ESP-NOW pierde tramas.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(
      [](arduino_event_id_t, arduino_event_info_t info) {
        // 36 = el propio ESP32 abandonó el intento (al reintentar): taparía la razón real.
        if (info.wifi_sta_disconnected.reason != WIFI_REASON_STA_LEAVING) {
          ultima_razon_wifi.store(info.wifi_sta_disconnected.reason);
        }
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // Red abierta: sin contraseña (si se pasa una, el ESP32 exige WPA y no entra).
  WiFi.begin(ssid_.c_str(), clave_wifi_.length() > 0 ? clave_wifi_.c_str() : nullptr);
  ultimo_intento_wifi_ms_ = millis();
  Serial.println("[MQTTS] Conectando al Wi-Fi; ESP-NOW se inicia al asociarse al router");
  xTaskCreatePinnedToCore(tarea, "mqtts", PILA_TAREA, this, PRIORIDAD_TAREA, nullptr, NUCLEO_RED);
}

bool TransporteMqttReal::leerCredenciales() {
  Preferences nvs;
  if (!nvs.begin(ESPACIO_NVS, true)) {
    return false;
  }
  ssid_ = nvs.getString("ssid", "");
  clave_wifi_ = nvs.getString("wifi_pass", "");
  host_ = nvs.getString("host", "");
  usuario_ = nvs.getString("user", "");
  clave_mqtt_ = nvs.getString("mqtt_pass", "");
  const bool tiene_aes = nvs.getBytesLength("aes_key") == sizeof(clave_aes_) &&
                         nvs.getBytes("aes_key", clave_aes_, sizeof(clave_aes_)) ==
                             sizeof(clave_aes_);
  nvs.end();

  credenciales_validas_ = ssid_.length() > 0 && host_.length() > 0 && usuario_.length() > 0 &&
                          clave_mqtt_.length() > 0 && tiene_aes;
  if (credenciales_validas_) {
    cliente_.setServer(host_.c_str(), config::PUERTO_MQTTS);
  }
  return credenciales_validas_;
}

bool TransporteMqttReal::wifiConectado() const {
  return credenciales_validas_ && WiFi.status() == WL_CONNECTED;
}

bool TransporteMqttReal::conectado() {
  return credenciales_validas_ && mqtt_conectado_.load();
}

// Loop: entrega en este hilo lo que llegó del broker (nube no se toca desde la tarea).
void TransporteMqttReal::actualizar() {
  if (!credenciales_validas_ || al_mensaje_ == nullptr) {
    return;
  }
  Entrante mensaje;
  while (xQueueReceive(entrantes_, &mensaje, 0) == pdTRUE) {
    al_mensaje_(mensaje.topico, mensaje.payload);
  }
}

void TransporteMqttReal::tarea(void* yo) {
  auto* transporte = static_cast<TransporteMqttReal*>(yo);
  for (;;) {
    transporte->vueltaDeTarea();
    vTaskDelay(pdMS_TO_TICKS(VUELTA_TAREA_MS));
  }
}

void TransporteMqttReal::vueltaDeTarea() {
  const uint32_t ahora_ms = millis();
  if (WiFi.status() != WL_CONNECTED) {
    mqtt_conectado_.store(false);
    if (cliente_.connected()) {
      cliente_.disconnect();
    }
    if (reporto_wifi_) {
      Serial.println("[MQTTS] Wi-Fi desconectado; ESP-NOW no se reinicia");
      reporto_wifi_ = false;
    }
    // Cada búsqueda de red mueve la radio de canal y corta ESP-NOW un momento: si no conecta,
    // se espera cada vez más (hasta REINTENTO_WIFI_MAX_MS) para no molestar a la orquesta.
    if (ahora_ms - ultimo_intento_wifi_ms_ >= espera_wifi_ms_) {
      ultimo_intento_wifi_ms_ = ahora_ms;
      const uint16_t razon = ultima_razon_wifi.load();
      Serial.printf("[MQTTS] Wi-Fi sin conectar (razón %u: %s); reintento en %lu s\n", razon,
                    pistaWifi(razon), static_cast<unsigned long>(espera_wifi_ms_ / 1000));
      if (!busco_red_) {
        busco_red_ = true;
        buscarRed();  // ya vuelve a intentar al terminar
      } else {
        WiFi.reconnect();
      }
      espera_wifi_ms_ = espera_wifi_ms_ * 2 > config::REINTENTO_WIFI_MAX_MS
                            ? config::REINTENTO_WIFI_MAX_MS
                            : espera_wifi_ms_ * 2;
    }
    return;
  }

  if (!reporto_wifi_) {
    reporto_wifi_ = true;
    espera_wifi_ms_ = config::REINTENTO_WIFI_MS;
    Serial.printf("[MQTTS] Wi-Fi conectado en canal %u\n",
                  static_cast<unsigned>(WiFi.channel()));
  }
  if (!ntp_iniciado_) {
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    ntp_iniciado_ = true;
  }

  if (!cliente_.connected()) {
    mqtt_conectado_.store(false);
    if (ahora_ms - ultimo_intento_mqtt_ms_ >= config::REINTENTO_MQTT_MS) {
      ultimo_intento_mqtt_ms_ = ahora_ms;
      mqtt_conectado_.store(conectarBroker());
    }
    return;
  }

  // Al menos una vuelta (mantiene vivo el keepalive); más si ya hay mensajes esperando.
  for (size_t i = 0; i < MENSAJES_POR_VUELTA && cliente_.loop() && tls_.available() > 0; ++i) {
  }
  Saliente mensaje;
  while (xQueueReceive(salientes_, &mensaje, 0) == pdTRUE) {
    if (!cliente_.publish(mensaje.topico, mensaje.payload, mensaje.retenido)) {
      Serial.printf("[MQTTS] No se publicó en %s; nube lo reintentará\n", mensaje.topico);
    }
  }
  mqtt_conectado_.store(cliente_.connected());
}

// Para comparar nombres sin fijarse en mayúsculas, espacios ni apóstrofos (el iPhone usa ’).
String sinDetalles(const String& nombre) {
  String limpio;
  for (size_t i = 0; i < nombre.length(); ++i) {
    const char c = nombre[i];
    const bool apostrofo_curvo = static_cast<uint8_t>(c) == 0xE2 && i + 2 < nombre.length() &&
                                 static_cast<uint8_t>(nombre[i + 1]) == 0x80 &&
                                 static_cast<uint8_t>(nombre[i + 2]) == 0x99;
    if (apostrofo_curvo) {
      i += 2;
    } else if (c != ' ' && c != '\'') {
      limpio += static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
  }
  return limpio;
}

// Una vez, si no conecta: ¿el ESP32 ve la red? Suelta el intento para poder buscar y luego
// vuelve a intentar. No imprime nombres de redes, solo lo que sirve para decidir.
void TransporteMqttReal::buscarRed() {
  WiFi.disconnect(false, false);
  vTaskDelay(pdMS_TO_TICKS(200));
  const int16_t redes = WiFi.scanNetworks(false, true);
  if (redes < 0) {
    Serial.println("[MQTTS] No se pudo buscar redes ahora");
  } else {
    bool exacta = false;
    bool parecida = false;
    for (int16_t i = 0; i < redes; ++i) {
      if (WiFi.SSID(i) == ssid_) {
        exacta = true;
        Serial.printf("[MQTTS] Veo la red: canal %ld, señal %ld dBm, %s\n",
                      static_cast<long>(WiFi.channel(i)), static_cast<long>(WiFi.RSSI(i)),
                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "abierta" : "con contraseña");
      } else if (sinDetalles(WiFi.SSID(i)) == sinDetalles(ssid_)) {
        parecida = true;
      }
    }
    if (!exacta && parecida) {
      Serial.println("[MQTTS] Hay una red CASI igual a la grabada: cambia en mayúsculas, "
                     "espacios o apóstrofo (el iPhone usa ’). Corrige WIFI_SSID.");
    } else if (!exacta) {
      Serial.printf("[MQTTS] No veo la red grabada entre %d redes de 2.4 GHz (¿hotspot sin "
                    "anunciarse, solo 5 GHz, o nombre distinto?)\n", redes);
    }
    WiFi.scanDelete();
  }
  WiFi.begin(ssid_.c_str(), clave_wifi_.length() > 0 ? clave_wifi_.c_str() : nullptr);
}

bool TransporteMqttReal::conectarBroker() {
  // Sin hora válida el certificado no se puede validar: esperar al NTP.
  if (time(nullptr) < static_cast<time_t>(config::HORA_VALIDA_DESDE)) {
    return false;
  }
  char id_cliente[40];
  snprintf(id_cliente, sizeof(id_cliente), "osito-%s", WiFi.macAddress().c_str());
  char fuera_de_linea[32];
  char en_linea[32];
  contrato_mqtt::armarEstado(false, fuera_de_linea, sizeof(fuera_de_linea));
  contrato_mqtt::armarEstado(true, en_linea, sizeof(en_linea));
  const bool conecto = cliente_.connect(id_cliente, usuario_.c_str(), clave_mqtt_.c_str(),
                                        TOPICO_ESTADO_OSITO, 1, true, fuera_de_linea);
  if (!conecto) {
    Serial.printf("[MQTTS] Conexión TLS/MQTT falló (estado %d); reintentaré\n",
                  cliente_.state());
    return false;
  }
  // Sin Nagle: un mensaje corto no espera el acuse TCP del anterior.
  if (tls_.setNoDelay(true) < 0) {
    Serial.println("[MQTTS] No se pudo quitar la espera de Nagle; sigo igual");
  }
  if (!cliente_.subscribe(contrato_mqtt::TOPICO_MODO) ||
      !cliente_.subscribe(contrato_mqtt::TOPICO_SYNC_ACK) ||
      !cliente_.subscribe(contrato_mqtt::TOPICO_AVISO) ||
      !cliente_.subscribe(contrato_mqtt::TOPICO_CANCION) ||
      !cliente_.subscribe(contrato_mqtt::TOPICO_TECLA_REMOTA) ||
      !cliente_.publish(TOPICO_ESTADO_OSITO, en_linea, true)) {
    Serial.println("[MQTTS] No se pudo completar la suscripción/estado; reconectaré");
    cliente_.disconnect();
    return false;
  }
  Serial.println("[MQTTS] Broker conectado con TLS validado; estado en línea publicado");
  return true;
}

// Corre en la tarea de red: solo copia a la cola, sin tocar nube.
void TransporteMqttReal::recibir(char* topico, uint8_t* payload, unsigned int tamano) {
  if (instancia == nullptr || topico == nullptr || tamano >= ENTRANTE_MAX ||
      strlen(topico) >= TOPICO_MAX) {
    return;
  }
  Entrante mensaje;
  strcpy(mensaje.topico, topico);
  memcpy(mensaje.payload, payload, tamano);
  mensaje.payload[tamano] = '\0';
  xQueueSend(instancia->entrantes_, &mensaje, 0);  // llena: se pierde (Node-RED reintenta)
}

bool TransporteMqttReal::cifrarEvento(const char* topico, const char* json, char* destino,
                                      size_t tamano) {
  char nodo[16];
  uint32_t seq = 0;
  const size_t largo = strnlen(json, config::EVENTO_JSON_MAX);
  if (largo == config::EVENTO_JSON_MAX || !nodoDeTopico(topico, nodo, sizeof(nodo)) ||
      !extraerSecuencia(json, seq)) {
    return false;
  }

  uint8_t sobre[SOBRE_MAX] = {};
  uint8_t aad[32] = {};
  const int largo_aad = snprintf(reinterpret_cast<char*>(aad), sizeof(aad), "%s:%lu", nodo,
                                 static_cast<unsigned long>(seq));
  if (largo_aad <= 0 || static_cast<size_t>(largo_aad) >= sizeof(aad)) {
    return false;
  }
  uint8_t* nonce = sobre;
  uint8_t* cifrado_bytes = nonce + cifrado::NONCE_BYTES;
  uint8_t* tag = cifrado_bytes + largo;
  if (!cifrado::cifrar(clave_aes_, aad, largo_aad, reinterpret_cast<const uint8_t*>(json), largo,
                       nonce, cifrado_bytes, tag)) {
    return false;
  }

  uint8_t base64[BASE64_MAX] = {};
  size_t largo_base64 = 0;
  if (mbedtls_base64_encode(base64, sizeof(base64), &largo_base64, sobre,
                            cifrado::NONCE_BYTES + largo + TAG_CIFRADO_BYTES) != 0) {
    return false;
  }
  const int escritos = snprintf(destino, tamano, "{\"seq\":%lu,\"data\":\"%s\"}",
                                static_cast<unsigned long>(seq), base64);
  return escritos > 0 && static_cast<size_t>(escritos) < tamano;
}

// Loop: arma (y cifra) el mensaje aquí y la tarea lo publica. true = quedó en la cola.
bool TransporteMqttReal::publicar(const char* topico, const char* payload, bool retenido) {
  if (!conectado() || topico == nullptr || payload == nullptr ||
      strlen(topico) >= TOPICO_MAX) {
    return false;
  }
  Saliente mensaje;
  strcpy(mensaje.topico, topico);
  mensaje.retenido = retenido;
  if (strstr(topico, "/evento") != nullptr) {
    if (!cifrarEvento(topico, payload, mensaje.payload, sizeof(mensaje.payload))) {
      Serial.println("[MQTTS] No se cifró un evento: JSON, tópico o clave AES no válidos");
      return false;
    }
  } else if (snprintf(mensaje.payload, sizeof(mensaje.payload), "%s", payload) >=
             static_cast<int>(sizeof(mensaje.payload))) {
    return false;
  }
  return xQueueSend(salientes_, &mensaje, 0) == pdTRUE;
}
