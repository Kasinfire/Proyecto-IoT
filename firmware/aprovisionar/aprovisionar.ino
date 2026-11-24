// Guarda en NVS los datos de ESP-NOW y, en el osito, Wi-Fi/MQTTS; nunca quedan en el código.
// Se sube una vez a cada placa y se escriben los comandos por Monitor Serie (115200, "New Line").
// La NVS no se borra al subir otro sketch. Usa los secretos locales de .secrets/mqtt.env.
//
// Las claves se generan una vez con `python3 -c "import secrets; print(secrets.token_hex(16))"`
// y se guardan en .secrets/ (no se sube al repo), nunca en el código.
//
//   clave_grupo=<32 hex>          las 4 placas, la misma
//   clave_ranita=<32 hex>         la ranita y el osito (igual clave_gatito, clave_pollito)
//   mac_osito=AA:BB:CC:DD:EE:FF   solo animales: la MAC que imprime este sketch en el osito
//   canal=<1-13>                  solo animales: el canal Wi-Fi del router del osito
//   wifi_ssid=...                 solo osito: SSID (espacios permitidos)
//   wifi_password=...             solo osito: clave Wi-Fi
//   broker_host=...               solo osito: IP/DNS incluido en el SAN del certificado
//   mqtt_user=... / mqtt_password=...   solo osito: credencial MQTTS dedicada
//   mqtt_aes_key=<32 hex>         solo osito: clave AES-128-GCM MQTT
//   ver                           qué hay guardado (sin mostrar las claves)
//   borrar                        borra enlace y datos de MQTTS
#include <Preferences.h>
#include <esp_mac.h>

namespace {
constexpr uint32_t BAUDIOS = 115200;
constexpr char ESPACIO_NVS[] = "enlace";  // el mismo que usa enlace.cpp
constexpr size_t CLAVE_BYTES = 16;
constexpr size_t MAC_BYTES = 6;
constexpr uint8_t CANAL_MIN = 1;
constexpr uint8_t CANAL_MAX = 13;
constexpr size_t LINEA_MAX = 128;

const char* const CLAVES[] = {"clave_grupo", "clave_ranita", "clave_gatito", "clave_pollito"};
const char* const DATOS_MQTT[] = {"ssid", "wifi_pass", "host", "user", "mqtt_pass", "aes_key"};

char linea[LINEA_MAX];
size_t largo = 0;
Preferences nvs;
Preferences nvs_mqtt;

int valorHex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

// Lee pares hex, saltándose ':' (para las MAC). false si no son exactamente `tamano` bytes.
bool leerHex(const char* texto, uint8_t* destino, size_t tamano) {
  size_t i = 0;
  while (*texto != '\0') {
    if (*texto == ':') {
      ++texto;
      continue;
    }
    const int alto = valorHex(texto[0]);
    const int bajo = alto < 0 ? -1 : valorHex(texto[1]);
    if (bajo < 0 || i == tamano)
      return false;
    destino[i++] = alto * 16 + bajo;
    texto += 2;
  }
  return i == tamano;
}

bool esClave(const char* nombre) {
  for (const char* clave : CLAVES) {
    if (strcmp(nombre, clave) == 0)
      return true;
  }
  return false;
}

void ver() {
  for (const char* clave : CLAVES) {
    Serial.printf("  %-14s %s\n", clave, nvs.isKey(clave) ? "guardada" : "-");
  }
  uint8_t mac[MAC_BYTES];
  if (nvs.getBytes("mac_osito", mac, MAC_BYTES) == MAC_BYTES) {
    Serial.printf("  mac_osito      %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);
  } else {
    Serial.println("  mac_osito      -");
  }
  Serial.printf("  canal          %u\n", nvs.getUChar("canal", 1));
  Serial.println("  MQTTS (osito):");
  for (const char* clave : DATOS_MQTT) {
    Serial.printf("  %-14s %s\n", clave, nvs_mqtt.isKey(clave) ? "guardada" : "-");
  }
}

void procesar(char* texto) {
  if (strcmp(texto, "ver") == 0) {
    ver();
    return;
  }
  if (strcmp(texto, "borrar") == 0) {
    nvs.clear();
    nvs_mqtt.clear();
    Serial.println("Borrados datos de enlace y MQTTS.");
    return;
  }

  char* igual = strchr(texto, '=');
  if (igual == nullptr) {
    Serial.println("No entendí. Usa nombre=valor, ver o borrar.");
    return;
  }
  *igual = '\0';
  const char* nombre = texto;
  const char* valor = igual + 1;

  if (esClave(nombre)) {
    uint8_t clave[CLAVE_BYTES];
    if (!leerHex(valor, clave, CLAVE_BYTES)) {
      Serial.println("La clave debe ser de 32 caracteres hex (16 bytes).");
      return;
    }
    nvs.putBytes(nombre, clave, CLAVE_BYTES);
  } else if (strcmp(nombre, "mac_osito") == 0) {
    uint8_t mac[MAC_BYTES];
    if (!leerHex(valor, mac, MAC_BYTES)) {
      Serial.println("La MAC debe ser como AA:BB:CC:DD:EE:FF.");
      return;
    }
    nvs.putBytes(nombre, mac, MAC_BYTES);
  } else if (strcmp(nombre, "canal") == 0) {
    const int canal = atoi(valor);
    if (canal < CANAL_MIN || canal > CANAL_MAX) {
      Serial.println("El canal va de 1 a 13.");
      return;
    }
    nvs.putUChar(nombre, canal);
  } else if (strcmp(nombre, "wifi_ssid") == 0) {
    nvs_mqtt.putString("ssid", valor);
  } else if (strcmp(nombre, "wifi_password") == 0) {
    nvs_mqtt.putString("wifi_pass", valor);
  } else if (strcmp(nombre, "broker_host") == 0) {
    nvs_mqtt.putString("host", valor);
  } else if (strcmp(nombre, "mqtt_user") == 0) {
    nvs_mqtt.putString("user", valor);
  } else if (strcmp(nombre, "mqtt_password") == 0) {
    nvs_mqtt.putString("mqtt_pass", valor);
  } else if (strcmp(nombre, "mqtt_aes_key") == 0) {
    uint8_t clave[16];
    if (!leerHex(valor, clave, sizeof(clave))) {
      Serial.println("La clave MQTT debe ser de 32 caracteres hex (16 bytes).");
      return;
    }
    nvs_mqtt.putBytes("aes_key", clave, sizeof(clave));
  } else {
    Serial.printf("No conozco '%s'.\n", nombre);
    return;
  }
  Serial.printf("%s guardado.\n", nombre);
}
}  // namespace

void setup() {
  Serial.begin(BAUDIOS);
  nvs.begin(ESPACIO_NVS, false);
  nvs_mqtt.begin("mqtt", false);
  // Se lee de los eFuse: WiFi.macAddress() da ceros si el Wi-Fi todavía no termina de arrancar.
  // Es la MAC de estación, la misma que usa ESP-NOW.
  uint8_t mac[MAC_BYTES];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf(
      "\nAprovisionamiento del enlace. MAC de esta placa: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0],
      mac[1], mac[2], mac[3], mac[4], mac[5]);
  ver();
}

void loop() {
  while (Serial.available() > 0) {
    const char c = Serial.read();
    if (c == '\r')
      continue;
    if (c != '\n') {
      if (largo < LINEA_MAX - 1)
        linea[largo++] = c;
      continue;
    }
    linea[largo] = '\0';
    largo = 0;
    if (linea[0] != '\0')
      procesar(linea);
  }
}
