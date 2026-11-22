// Guarda en NVS las claves y datos del enlace ESP-NOW, para que no estén en el código.
// Se sube una vez a cada placa, se escriben los comandos en el Monitor Serie (115200,
// "New Line") y después se sube el firmware del animal. La NVS no se borra al subir otro sketch.
//
// Las claves se generan una vez con `python3 -c "import secrets; print(secrets.token_hex(16))"`
// y se guardan en .secrets/ (no se sube al repo), nunca en el código.
//
//   clave_grupo=<32 hex>          las 4 placas, la misma
//   clave_ranita=<32 hex>         la ranita y el osito (igual clave_gatito, clave_pollito)
//   mac_osito=AA:BB:CC:DD:EE:FF   solo animales: la MAC que imprime este sketch en el osito
//   canal=<1-13>                  solo animales: el canal Wi-Fi del router del osito
//   ver                           qué hay guardado (sin mostrar las claves)
//   borrar                        borra todo
#include <Preferences.h>
#include <WiFi.h>

namespace {
constexpr uint32_t BAUDIOS = 115200;
constexpr char ESPACIO_NVS[] = "enlace";  // el mismo que usa enlace.cpp
constexpr size_t CLAVE_BYTES = 16;
constexpr size_t MAC_BYTES = 6;
constexpr uint8_t CANAL_MIN = 1;
constexpr uint8_t CANAL_MAX = 13;
constexpr size_t LINEA_MAX = 64;

const char* const CLAVES[] = {"clave_grupo", "clave_ranita", "clave_gatito", "clave_pollito"};

char linea[LINEA_MAX];
size_t largo = 0;
Preferences nvs;

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
}

void procesar(char* texto) {
  if (strcmp(texto, "ver") == 0) {
    ver();
    return;
  }
  if (strcmp(texto, "borrar") == 0) {
    nvs.clear();
    Serial.println("Borrado.");
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
  } else {
    Serial.printf("No conozco '%s'.\n", nombre);
    return;
  }
  Serial.printf("%s guardado.\n", nombre);
}
}  // namespace

void setup() {
  Serial.begin(BAUDIOS);
  WiFi.mode(WIFI_STA);  // para que la MAC sea la que usa ESP-NOW
  nvs.begin(ESPACIO_NVS, false);
  Serial.printf("\nAprovisionamiento del enlace. MAC de esta placa: %s\n",
                WiFi.macAddress().c_str());
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
