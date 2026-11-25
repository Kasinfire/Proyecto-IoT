// Lee los cierres y aperturas filtrados del SW-520D; detector_agitacion.h decide si es una sacudida.
#include <Arduino.h>
#include <antirrebote.h>
#include <protocolo.h>

#include "config.h"
#include "detector_agitacion.h"
#include "pines.h"
#include "sensor_agitacion.h"

namespace sensor_agitacion {
namespace {
Antirrebote sensor(pines::SENSOR_AGITACION, config::ANTIRREBOTE_AGITACION_MS, true);
DetectorAgitacion detector;

uint8_t intensidad_gesto = 0;
bool hay_gesto_pendiente = false;
}  // namespace

void iniciar() {
  sensor.iniciar();
}

void actualizar() {
  sensor.actualizar();

  // Cuentan ambos flancos: contacto cerrado y contacto abierto.
  const bool cerro = sensor.sePresiono();
  const bool abrio = sensor.seSolto();
  if (!cerro && !abrio) {
    return;
  }
  const uint32_t ahora_ms = millis();
  if (detector.alCambio(ahora_ms)) {
    hay_gesto_pendiente = true;
    intensidad_gesto = detector.intensidad(ahora_ms, protocolo::INTENSIDAD_AGITAR_MAX);
  }
}

bool seDetectoGesto(uint8_t& intensidad) {
  if (!hay_gesto_pendiente) {
    return false;
  }
  hay_gesto_pendiente = false;
  intensidad = intensidad_gesto;
  return true;
}
}  // namespace sensor_agitacion
