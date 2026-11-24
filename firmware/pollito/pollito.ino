#include <Arduino.h>
#include <enlace.h>
#include <protocolo.h>

#include "config.h"
#include "led_estado.h"
#include "mensajes_osito.h"
#include "sensor_agitacion.h"

void setup() {
  Serial.begin(config::BAUDIOS);
  enlace::iniciar(protocolo::Nodo::Pollito);  // antes que todo lo demás de Wi-Fi
  sensor_agitacion::iniciar();
  led_estado::iniciar();
}

void loop() {
  sensor_agitacion::actualizar();
  mensajes_osito::actualizar();
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
