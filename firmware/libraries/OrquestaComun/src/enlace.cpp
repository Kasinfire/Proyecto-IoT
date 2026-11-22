// Trama: Cabecera (en claro, va como AAD) || nonce || payload cifrado || tag.
// Animal -> osito: unicast con la clave de enlace del animal.
// Osito -> animales: difusión con la clave de grupo.
#include "enlace.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "cifrado.h"
#include "cola_circular.h"
#include "contra_replay.h"

namespace enlace {
namespace {
using protocolo::Nodo;

static_assert(cifrado::NONCE_BYTES == protocolo::NONCE_BYTES, "El nonce no coincide");
static_assert(cifrado::TAG_BYTES == protocolo::TAG_BYTES, "El tag no coincide");

constexpr char ESPACIO_NVS[] = "enlace";
constexpr size_t MAC_BYTES = 6;
constexpr uint8_t MAC_DIFUSION[MAC_BYTES] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint8_t CANAL_POR_DEFECTO = 1;
constexpr size_t CAPACIDAD_COLA = 8;
constexpr size_t SOBRECARGA =
    sizeof(protocolo::Cabecera) + protocolo::NONCE_BYTES + protocolo::TAG_BYTES;

struct Trama {
  uint8_t datos[protocolo::MENSAJE_MAX];
  uint8_t tamano;
};

Nodo yo = Nodo::Osito;
bool listo = false;
uint16_t arranque = 0;
uint32_t seq = 0;
uint8_t mac_destino[MAC_BYTES] = {};

uint8_t clave_grupo[cifrado::CLAVE_BYTES] = {};
// Índice = Nodo. Un animal solo tiene la suya; el osito tiene la de los tres.
uint8_t claves_enlace[protocolo::NUM_NODOS][cifrado::CLAVE_BYTES] = {};
bool tiene_clave_enlace[protocolo::NUM_NODOS] = {};

ColaCircular<Trama, CAPACIDAD_COLA> recibidas;
ContraReplay contra_replay;
uint32_t ultimo_mensaje_ms[protocolo::NUM_NODOS] = {};
bool hubo_mensaje[protocolo::NUM_NODOS] = {};
uint32_t ultimo_latido_ms = 0;

bool soyOsito() {
  return yo == Nodo::Osito;
}

// Corre en la tarea de Wi-Fi: solo copia y sale. Si la cola está llena se pierde la trama.
void alRecibir(const esp_now_recv_info_t*, const uint8_t* datos, int tamano) {
  if (tamano <= 0 || static_cast<size_t>(tamano) > protocolo::MENSAJE_MAX) {
    return;
  }
  Trama trama;
  memcpy(trama.datos, datos, tamano);
  trama.tamano = tamano;
  recibidas.meter(trama);
}

bool leerClave(Preferences& nvs, const char* nombre, uint8_t* destino) {
  if (nvs.getBytes(nombre, destino, cifrado::CLAVE_BYTES) == cifrado::CLAVE_BYTES) {
    return true;
  }
  Serial.printf("[enlace] Falta '%s' en NVS (sube firmware/aprovisionar)\n", nombre);
  return false;
}

bool leerNvs(uint8_t& canal) {
  Preferences nvs;
  if (!nvs.begin(ESPACIO_NVS, false)) {
    Serial.println("[enlace] No se pudo abrir la NVS");
    return false;
  }

  bool completo = leerClave(nvs, "clave_grupo", clave_grupo);
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    const bool la_necesito =
        static_cast<Nodo>(i) != Nodo::Osito && (soyOsito() || i == protocolo::aEntero(yo));
    if (la_necesito) {
      char nombre[16];
      snprintf(nombre, sizeof(nombre), "clave_%s", protocolo::NOMBRE_NODO[i]);
      tiene_clave_enlace[i] = leerClave(nvs, nombre, claves_enlace[i]);
      completo = completo && tiene_clave_enlace[i];
    }
  }

  if (soyOsito()) {
    memcpy(mac_destino, MAC_DIFUSION, MAC_BYTES);
  } else if (nvs.getBytes("mac_osito", mac_destino, MAC_BYTES) != MAC_BYTES) {
    Serial.println("[enlace] Falta 'mac_osito' en NVS (sube firmware/aprovisionar)");
    completo = false;
  }
  canal = nvs.getUChar("canal", CANAL_POR_DEFECTO);

  // Sube en cada arranque para que seq pueda empezar en 0 sin que el receptor lo tome por replay.
  arranque = nvs.getUShort("arranque", 0) + 1;
  nvs.putUShort("arranque", arranque);
  return completo;
}

bool iniciarRadio(uint8_t canal) {
  if (!soyOsito()) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
  } else if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);  // osito sin router (pruebas): se queda en el canal por defecto
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[enlace] esp_now_init falló");
    return false;
  }
  esp_now_register_recv_cb(alRecibir);

  esp_now_peer_info_t par = {};
  memcpy(par.peer_addr, mac_destino, MAC_BYTES);
  par.channel = 0;  // el canal actual de la radio
  par.ifidx = WIFI_IF_STA;
  // El cifrado propio de ESP-NOW no funciona con difusión; ciframos nosotros con AES-GCM.
  par.encrypt = false;
  if (esp_now_add_peer(&par) != ESP_OK) {
    Serial.println("[enlace] esp_now_add_peer falló");
    return false;
  }
  return true;
}

const uint8_t* claveParaDescifrar(Nodo emisor) {
  if (emisor == Nodo::Osito) {
    return clave_grupo;
  }
  const uint8_t i = protocolo::aEntero(emisor);
  return tiene_clave_enlace[i] ? claves_enlace[i] : nullptr;
}

bool validar(const Trama& trama, Mensaje& mensaje) {
  if (trama.tamano < SOBRECARGA) {
    return false;
  }
  memcpy(&mensaje.cabecera, trama.datos, sizeof(protocolo::Cabecera));
  const size_t tam_payload = trama.tamano - SOBRECARGA;
  const uint8_t num_emisor = mensaje.cabecera.nodo_emisor;
  if (mensaje.cabecera.version != protocolo::VERSION || num_emisor >= protocolo::NUM_NODOS ||
      tam_payload > protocolo::PAYLOAD_MAX) {
    return false;
  }

  // Los animales solo escuchan al osito y el osito solo a los animales.
  const Nodo emisor = mensaje.emisor();
  if ((emisor == Nodo::Osito) == soyOsito()) {
    return false;
  }
  const uint8_t* clave = claveParaDescifrar(emisor);
  if (clave == nullptr) {
    return false;
  }

  const uint8_t* nonce = trama.datos + sizeof(protocolo::Cabecera);
  const uint8_t* secreto = nonce + protocolo::NONCE_BYTES;
  const uint8_t* tag = secreto + tam_payload;
  if (!cifrado::descifrar(clave, trama.datos, sizeof(protocolo::Cabecera), nonce, secreto,
                          tam_payload, tag, mensaje.payload)) {
    Serial.printf("[enlace] Rechazado de %s: el tag no cuadra\n",
                  protocolo::NOMBRE_NODO[num_emisor]);
    return false;
  }
  // Después del tag: así un mensaje falso no puede mover el contador.
  if (!contra_replay.aceptar(num_emisor, mensaje.cabecera.arranque, mensaje.cabecera.seq)) {
    Serial.printf("[enlace] Rechazado de %s: repetido (arranque %u, seq %lu)\n",
                  protocolo::NOMBRE_NODO[num_emisor], mensaje.cabecera.arranque,
                  static_cast<unsigned long>(mensaje.cabecera.seq));
    return false;
  }

  mensaje.tamano = tam_payload;
  ultimo_mensaje_ms[num_emisor] = millis();
  hubo_mensaje[num_emisor] = true;
  return true;
}
}  // namespace

bool iniciar(Nodo nodo) {
  yo = nodo;
  uint8_t canal = CANAL_POR_DEFECTO;
  const bool nvs_completa = leerNvs(canal);
  listo = nvs_completa && iniciarRadio(canal);
  if (listo) {
    Serial.printf("[enlace] Listo como %s, arranque %u\n",
                  protocolo::NOMBRE_NODO[protocolo::aEntero(yo)], arranque);
  }
  return listo;
}

void actualizar() {
  if (!listo || soyOsito()) {
    return;
  }
  const uint32_t ahora_ms = millis();
  if (ahora_ms - ultimo_latido_ms >= protocolo::LATIDO_CADA_MS) {
    ultimo_latido_ms = ahora_ms;
    enviarLatido();
  }
}

bool enviarCrudo(protocolo::TipoMensaje tipo, const void* datos, size_t tamano) {
  if (!listo || tamano > protocolo::PAYLOAD_MAX) {
    return false;
  }
  const protocolo::Cabecera cabecera = {protocolo::VERSION, protocolo::aEntero(tipo),
                                        protocolo::aEntero(yo), arranque, seq++};
  uint8_t trama[protocolo::MENSAJE_MAX];
  memcpy(trama, &cabecera, sizeof(cabecera));
  uint8_t* nonce = trama + sizeof(cabecera);
  uint8_t* secreto = nonce + protocolo::NONCE_BYTES;
  uint8_t* tag = secreto + tamano;

  const uint8_t* clave = soyOsito() ? clave_grupo : claves_enlace[protocolo::aEntero(yo)];
  if (!cifrado::cifrar(clave, trama, sizeof(cabecera), static_cast<const uint8_t*>(datos), tamano,
                       nonce, secreto, tag)) {
    return false;
  }
  return esp_now_send(mac_destino, trama, SOBRECARGA + tamano) == ESP_OK;
}

bool enviarLatido() {
  const uint8_t vacio = 0;  // puntero válido para mbedtls aunque el tamaño sea 0
  return enviarCrudo(protocolo::TipoMensaje::Latido, &vacio, 0);
}

bool recibir(Mensaje& mensaje) {
  Trama trama;
  while (recibidas.sacar(trama)) {
    if (validar(trama, mensaje)) {
      return true;
    }
  }
  return false;
}

bool estaConectado(Nodo nodo) {
  const uint8_t i = protocolo::aEntero(nodo);
  return i < protocolo::NUM_NODOS && hubo_mensaje[i] &&
         millis() - ultimo_mensaje_ms[i] < protocolo::DESCONECTADO_TRAS_MS;
}
}  // namespace enlace
