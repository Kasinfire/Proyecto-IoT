#include "cola_persistente.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_random.h>

#include <cstring>

#include "cola_eventos.h"
#include "config.h"

namespace cola_persistente {
namespace {
constexpr char RUTA_COLA[] = "/cola.bin";
constexpr char RUTA_TEMPORAL[] = "/cola.tmp";
constexpr char RUTA_DESBORDE[] = "/desborde.bin";
constexpr size_t REGISTRO_BYTES = sizeof(cola_eventos::Registro);

// Encabezado ("DES1" y un número al azar) y registros de tamaño fijo, uno tras otro. El número dice
// de qué archivo es el "cuántos ya salieron" de /cola.bin: si el archivo es otro, se lee desde el
// principio (Node-RED descarta los repetidos).
constexpr uint8_t MAGIA_DESBORDE[4] = {'D', 'E', 'S', '1'};
constexpr size_t ENCABEZADO_BYTES = 8;

void escribir32(uint8_t* p, uint32_t valor) {
  for (int i = 0; i < 4; ++i) {
    p[i] = static_cast<uint8_t>(valor >> (8 * i));
  }
}

uint32_t leer32(const uint8_t* p) {
  uint32_t valor = 0;
  for (int i = 0; i < 4; ++i) {
    valor |= static_cast<uint32_t>(p[i]) << (8 * i);
  }
  return valor;
}

class DesbordeArchivo : public cola_eventos::Desborde {
 public:
  void abrir(uint32_t numero, uint32_t ya_salieron) {
    total_ = 0;
    leidos_ = 0;
    numero_ = 0;
    if (!LittleFS.exists(RUTA_DESBORDE)) {
      return;
    }
    File archivo = LittleFS.open(RUTA_DESBORDE, "r");
    uint8_t encabezado[ENCABEZADO_BYTES] = {};
    const bool ok = archivo && archivo.read(encabezado, ENCABEZADO_BYTES) == ENCABEZADO_BYTES &&
                    memcmp(encabezado, MAGIA_DESBORDE, sizeof(MAGIA_DESBORDE)) == 0;
    const size_t tamano = archivo ? archivo.size() : 0;
    archivo.close();
    if (!ok) {
      Serial.println("[cola] El desborde guardado no es válido: se ignora");
      LittleFS.remove(RUTA_DESBORDE);
      return;
    }
    numero_ = leer32(encabezado + 4);
    total_ = (tamano - ENCABEZADO_BYTES) / REGISTRO_BYTES;  // uno a medias no cuenta
    leidos_ = numero == numero_ && ya_salieron <= total_ ? ya_salieron : 0;
  }

  bool meter(const cola_eventos::Registro& registro) override {
    if (cantidad() >= config::DESBORDE_MAX || total_ >= config::DESBORDE_ARCHIVO_MAX) {
      return false;
    }
    if (total_ == 0 && !crear()) {
      return false;
    }
    // En su lugar exacto, no "al final": si el anterior quedó a medias, este lo tapa.
    File archivo = LittleFS.open(RUTA_DESBORDE, "r+");
    const bool escrito =
        archivo && archivo.seek(lugar(total_)) &&
        archivo.write(reinterpret_cast<const uint8_t*>(&registro), REGISTRO_BYTES) == REGISTRO_BYTES;
    archivo.close();
    if (!escrito) {
      return false;
    }
    ++total_;
    return true;
  }

  cola_eventos::Lectura sacar(cola_eventos::Registro& registro) override {
    if (leidos_ >= total_) {
      return cola_eventos::Lectura::Vacio;
    }
    File archivo = LittleFS.open(RUTA_DESBORDE, "r");
    const bool leido = archivo && archivo.seek(lugar(leidos_)) &&
                       archivo.read(reinterpret_cast<uint8_t*>(&registro), REGISTRO_BYTES) ==
                           REGISTRO_BYTES;
    archivo.close();
    ++leidos_;
    if (!leido || !cola_eventos::registroValido(registro)) {
      Serial.println("[cola] Un evento del desborde está dañado: se salta");
      return cola_eventos::Lectura::Danado;
    }
    return cola_eventos::Lectura::Ok;
  }

  size_t cantidad() const override {
    return total_ - leidos_;
  }

  void vaciar() override {
    if (total_ > 0) {
      LittleFS.remove(RUTA_DESBORDE);
      total_ = 0;
      leidos_ = 0;
      numero_ = 0;
    }
  }

  uint32_t leidos() const {
    return static_cast<uint32_t>(leidos_);
  }

  uint32_t numero() const {
    return numero_;
  }

 private:
  static size_t lugar(size_t cual) {
    return ENCABEZADO_BYTES + cual * REGISTRO_BYTES;
  }

  bool crear() {
    uint8_t encabezado[ENCABEZADO_BYTES];
    memcpy(encabezado, MAGIA_DESBORDE, sizeof(MAGIA_DESBORDE));
    numero_ = esp_random() | 1u;  // nunca 0
    escribir32(encabezado + 4, numero_);
    File archivo = LittleFS.open(RUTA_DESBORDE, "w");
    const bool ok = archivo && archivo.write(encabezado, ENCABEZADO_BYTES) == ENCABEZADO_BYTES;
    archivo.close();
    return ok;
  }

  size_t total_ = 0;
  size_t leidos_ = 0;
  uint32_t numero_ = 0;
};

DesbordeArchivo desborde;
bool listo = false;
// /cola.bin: cuántos registros del desborde ya habían salido (4 B), de qué archivo (4 B) y luego
// cola_eventos::exportar().
constexpr size_t COPIA_ENCABEZADO = 8;
uint8_t copia[COPIA_ENCABEZADO + cola_eventos::EXPORTAR_MAX];
uint32_t version_guardada = 0;
uint32_t leidos_guardados = 0;
bool hay_copia = false;
uint32_t ultima_copia_ms = 0;
uint32_t frente_arranque = 0;
uint32_t frente_seq = 0;
uint32_t frente_desde_ms = 0;

// Recupera la RAM y dice cuántos registros del desborde, y de cuál, ya habían salido.
void recuperar(uint32_t& ya_salieron, uint32_t& numero) {
  ya_salieron = 0;
  numero = 0;
  if (!LittleFS.exists(RUTA_COLA)) {
    return;
  }
  File archivo = LittleFS.open(RUTA_COLA, "r");
  if (!archivo) {
    return;
  }
  const size_t tamano = archivo.size();
  const bool leido = tamano >= COPIA_ENCABEZADO && tamano <= sizeof(copia) &&
                     archivo.read(copia, tamano) == tamano;
  archivo.close();
  if (!leido || !cola_eventos::importar(copia + COPIA_ENCABEZADO, tamano - COPIA_ENCABEZADO)) {
    Serial.println("[cola] La copia guardada no es válida: se ignora");
    LittleFS.remove(RUTA_COLA);
    return;
  }
  hay_copia = true;
  ya_salieron = leer32(copia);
  numero = leer32(copia + 4);
}

void guardar(uint32_t ahora_ms) {
  const size_t bytes = cola_eventos::exportar(copia + COPIA_ENCABEZADO, sizeof(copia) - COPIA_ENCABEZADO);
  if (bytes == 0) {
    return;
  }
  escribir32(copia, desborde.leidos());
  escribir32(copia + 4, desborde.numero());
  File archivo = LittleFS.open(RUTA_TEMPORAL, "w");
  const bool escrito = archivo && archivo.write(copia, COPIA_ENCABEZADO + bytes) == COPIA_ENCABEZADO + bytes;
  archivo.close();
  if (!escrito || !LittleFS.rename(RUTA_TEMPORAL, RUTA_COLA)) {
    Serial.println("[cola] No pude guardar la copia en LittleFS");
    return;
  }
  hay_copia = true;
  version_guardada = cola_eventos::version();
  leidos_guardados = desborde.leidos();
  ultima_copia_ms = ahora_ms;
}
}  // namespace

void iniciar() {
  // true: si la partición no tiene formato (primera vez), se formatea.
  listo = LittleFS.begin(true);
  if (!listo) {
    Serial.println("[cola] LittleFS no arrancó: la cola solo vivirá en RAM");
    return;
  }
  uint32_t ya_salieron = 0;
  uint32_t numero = 0;
  recuperar(ya_salieron, numero);
  desborde.abrir(numero, ya_salieron);
  cola_eventos::usarDesborde(&desborde);
  version_guardada = cola_eventos::version();
  leidos_guardados = desborde.leidos();
  Serial.printf("[cola] Recuperados de LittleFS: %u en RAM y %u en el desborde\n",
                static_cast<unsigned>(cola_eventos::enRam()),
                static_cast<unsigned>(desborde.cantidad()));
}

void actualizar() {
  if (!listo) {
    return;
  }
  const uint32_t ahora_ms = millis();
  uint32_t arranque = 0;
  uint32_t seq = 0;
  if (!cola_eventos::frente(arranque, seq)) {
    // Todo confirmado: la copia ya no sirve.
    if (hay_copia) {
      LittleFS.remove(RUTA_COLA);
      hay_copia = false;
    }
    frente_desde_ms = ahora_ms;
    return;
  }
  if (arranque != frente_arranque || seq != frente_seq) {
    // Llegó un acuse (o es otro evento): hay red, no hace falta copiar.
    frente_arranque = arranque;
    frente_seq = seq;
    frente_desde_ms = ahora_ms;
  }
  const bool sin_acuse = ahora_ms - frente_desde_ms >= config::ESPERA_PARA_GUARDAR_COLA_MS;
  const bool cambio = cola_eventos::version() != version_guardada ||
                      desborde.leidos() != leidos_guardados || !hay_copia;
  if (sin_acuse && cambio && ahora_ms - ultima_copia_ms >= config::GUARDAR_COLA_CADA_MS) {
    guardar(ahora_ms);
  }
}
}  // namespace cola_persistente
