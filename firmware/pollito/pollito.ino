#include <Arduino.h>
#include <enlace.h>
#include <protocolo.h>

#include "config.h"
#include "led_estado.h"
#include "sensor_agitacion.h"

void setup() {
  Serial.begin(config::BAUDIOS);
  sensor_agitacion::iniciar();
  led_estado::iniciar();
  enlace::iniciar(protocolo::Nodo::Pollito);
}

void loop() {
  sensor_agitacion::actualizar();
  led_estado::actualizar();
  enlace::actualizar();

  uint8_t intensidad = 0;
  if (sensor_agitacion::seDetectoGesto(intensidad)) {
    led_estado::destellar();
    Serial.printf("[pollito] Sacudida confirmada, intensidad: %u\n", intensidad);
    if (!enlace::enviar(protocolo::PayloadAgitar{ intensidad })) {
      Serial.println("[pollito] No se pudo enviar la sacudida");
    }
  }
}
