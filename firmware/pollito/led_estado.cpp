// Apaga automáticamente el LED después del tiempo configurado.
#include <Arduino.h>

#include "config.h"
#include "led_estado.h"
#include "pines.h"

namespace led_estado {
namespace {
bool encendido = false;
uint32_t inicio_destello_ms = 0;
}  // namespace

void iniciar() {
  pinMode(pines::LED_ESTADO, OUTPUT);
  digitalWrite(pines::LED_ESTADO, LOW);
}

void actualizar() {
  if (encendido && millis() - inicio_destello_ms >= config::DURACION_DESTELLO_MS) {
    digitalWrite(pines::LED_ESTADO, LOW);
    encendido = false;
  }
}

void destellar() {
  digitalWrite(pines::LED_ESTADO, HIGH);
  encendido = true;
  inicio_destello_ms = millis();
}
}  // namespace led_estado
