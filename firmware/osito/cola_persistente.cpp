#include "cola_persistente.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "cola_eventos.h"
#include "config.h"

namespace cola_persistente {
namespace {
constexpr char RUTA_COLA[] = "/cola.bin";
constexpr char RUTA_TEMPORAL[] = "/cola.tmp";
constexpr char RUTA_DESBORDE[] = "/desborde.bin";
constexpr size_t REGISTRO_BYTES = sizeof(cola_eventos::Registro);

// Registros de tamaño fijo, uno tras otro. Si el osito se apaga a media escritura, el registro
// incompleto del final se ignora (el tamaño del archivo se divide entre REGISTRO_BYTES).
class DesbordeArchivo : public cola_eventos::Desborde {
 public:
  void abrir(uint32_t ya_leidos) {
    File archivo = LittleFS.open(RUTA_DESBORDE, "r");
    total_ = archivo ? archivo.size() / REGISTRO_BYTES : 0;
    archivo.close();
    leidos_ = ya_leidos < total_ ? ya_leidos : total_;
    if (total_ > 0 && leidos_ == total_) {
      borrar();
    }
  }

  bool meter(const cola_eventos::Registro& registro) override {
    if (total_ >= config::DESBORDE_MAX) {
      return false;
    }
    File archivo = LittleFS.open(RUTA_DESBORDE, "a");
    if (!archivo) {
      return false;
    }
    const size_t escritos =
        archivo.write(reinterpret_cast<const uint8_t*>(&registro), REGISTRO_BYTES);
    archivo.close();
    if (escritos != REGISTRO_BYTES) {
      return false;
    }
    ++total_;
    return true;
  }

  bool sacar(cola_eventos::Registro& registro) override {
    if (leidos_ >= total_) {
      return false;
    }
    File archivo = LittleFS.open(RUTA_DESBORDE, "r");
    const bool ok = archivo && archivo.seek(leidos_ * REGISTRO_BYTES) &&
                    archivo.read(reinterpret_cast<uint8_t*>(&registro), REGISTRO_BYTES) ==
                        REGISTRO_BYTES;
    archivo.close();
    if (!ok) {
      Serial.println("[cola] No pude leer el desborde: se descarta lo que quedaba");
      borrar();
      return false;
    }
    ++leidos_;
    if (leidos_ == total_) {
      borrar();  // ya volvió todo a la RAM
    }
    return true;
  }

  size_t cantidad() const override {
    return total_ - leidos_;
  }

  uint32_t leidos() const {
    return static_cast<uint32_t>(leidos_);
  }

 private:
  void borrar() {
    LittleFS.remove(RUTA_DESBORDE);
    total_ = 0;
    leidos_ = 0;
  }

  size_t total_ = 0;
  size_t leidos_ = 0;
};

DesbordeArchivo desborde;
bool listo = false;
// /cola.bin: cuántos registros del desborde ya estaban en la RAM (4 B) y luego cola_eventos::exportar().
uint8_t copia[4 + cola_eventos::EXPORTAR_MAX];
uint32_t version_guardada = 0;
uint32_t leidos_guardados = 0;
bool hay_copia = false;
uint32_t ultima_copia_ms = 0;
uint32_t frente_arranque = 0;
uint32_t frente_seq = 0;
uint32_t frente_desde_ms = 0;

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

// Devuelve cuántos registros del desborde ya se habían pasado a la RAM.
uint32_t recuperar() {
  File archivo = LittleFS.open(RUTA_COLA, "r");
  if (!archivo) {
    return 0;
  }
  const size_t tamano = archivo.size();
  const bool leido = tamano >= 4 && tamano <= sizeof(copia) &&
                     archivo.read(copia, tamano) == tamano;
  archivo.close();
  if (!leido || !cola_eventos::importar(copia + 4, tamano - 4)) {
    Serial.println("[cola] La copia guardada no es válida: se ignora");
    LittleFS.remove(RUTA_COLA);
    return 0;
  }
  hay_copia = true;
  return leer32(copia);
}

void guardar(uint32_t ahora_ms) {
  const size_t bytes = cola_eventos::exportar(copia + 4, sizeof(copia) - 4);
  if (bytes == 0) {
    return;
  }
  escribir32(copia, desborde.leidos());
  File archivo = LittleFS.open(RUTA_TEMPORAL, "w");
  const bool escrito = archivo && archivo.write(copia, 4 + bytes) == 4 + bytes;
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
  const uint32_t leidos = recuperar();
  desborde.abrir(leidos);
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
