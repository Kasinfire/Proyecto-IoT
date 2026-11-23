// Cuenta cierres y aperturas filtrados para medir la actividad del SW-520D.
#include <Arduino.h>
#include <antirrebote.h>
#include <protocolo.h>

#include "config.h"
#include "pines.h"
#include "sensor_agitacion.h"

namespace sensor_agitacion {
namespace {
Antirrebote sensor(pines::SENSOR_AGITACION, config::ANTIRREBOTE_AGITACION_MS, true);

uint32_t inicio_ventana_ms = 0;
uint32_t ultimo_gesto_ms = 0;
uint16_t cambios_ventana = 0;
uint8_t ventanas_fuertes = 0;
uint8_t intensidad_gesto = 0;
bool hay_gesto_previo = false;
bool hay_gesto_pendiente = false;

void contarCambio() {
  if (cambios_ventana < 255) {
    ++cambios_ventana;
  }
}

void cerrarVentana(uint32_t ahora_ms) {
  const uint16_t cambios = cambios_ventana;
  cambios_ventana = 0;

  const uint16_t cambios_limitados =
    cambios > config::CAMBIOS_PARA_INTENSIDAD_MAX ? config::CAMBIOS_PARA_INTENSIDAD_MAX
                                                  : cambios;
  const uint8_t intensidad = static_cast<uint8_t>(
    (cambios_limitados * protocolo::INTENSIDAD_AGITAR_MAX) / config::CAMBIOS_PARA_INTENSIDAD_MAX);

  // Solo si hubo movimiento: cada VENTANA_AGITACION_MS llenaría el monitor de ceros.
  if (cambios > 0) {
    Serial.printf("[pollito] Cambios: %u | Intensidad: %u\n", cambios, intensidad);
  }

  if (intensidad < config::UMBRAL_GESTO) {
    ventanas_fuertes = 0;
    return;
  }

  if (ventanas_fuertes < config::VENTANAS_PARA_CONFIRMAR) {
    ++ventanas_fuertes;
  }

  const bool paso_intervalo =
    !hay_gesto_previo || ahora_ms - ultimo_gesto_ms >= config::INTERVALO_ENTRE_GESTOS_MS;
  if (ventanas_fuertes >= config::VENTANAS_PARA_CONFIRMAR && paso_intervalo) {
    hay_gesto_pendiente = true;
    intensidad_gesto = intensidad;
    ultimo_gesto_ms = ahora_ms;
    hay_gesto_previo = true;
  }
}
}  // namespace

void iniciar() {
  sensor.iniciar();
  inicio_ventana_ms = millis();
}

void actualizar() {
  sensor.actualizar();

  // La intensidad mide ambos flancos: contacto cerrado y contacto abierto.
  if (sensor.sePresiono()) {
    contarCambio();
  }
  if (sensor.seSolto()) {
    contarCambio();
  }

  const uint32_t ahora_ms = millis();
  if (ahora_ms - inicio_ventana_ms >= config::VENTANA_AGITACION_MS) {
    inicio_ventana_ms = ahora_ms;
    cerrarVentana(ahora_ms);
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
