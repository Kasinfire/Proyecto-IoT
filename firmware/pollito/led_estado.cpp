// Apaga automáticamente el LED después del tiempo configurado.
#include "led_estado.h"

#include <Arduino.h>

#include "config.h"
#include "pines.h"

namespace led_estado {
namespace {
bool encendido = false;
uint32_t inicio_ms = 0;
uint32_t duracion_actual_ms = 0;
}  // namespace

void iniciar() {
  pinMode(pines::LED_ESTADO, OUTPUT);
  digitalWrite(pines::LED_ESTADO, LOW);
}

void actualizar() {
  if (encendido && millis() - inicio_ms >= duracion_actual_ms) {
    digitalWrite(pines::LED_ESTADO, LOW);
    encendido = false;
  }
}

void destellar() {
  encenderPor(config::DURACION_DESTELLO_MS);
}

void encenderPor(uint32_t duracion_ms) {
  const uint32_t ahora_ms = millis();
  // Un destello al agitar no debe cortar la luz que pidió el osito.
  if (encendido && ahora_ms - inicio_ms + duracion_ms < duracion_actual_ms) {
    return;
  }
  digitalWrite(pines::LED_ESTADO, HIGH);
  encendido = true;
  inicio_ms = ahora_ms;
  duracion_actual_ms = duracion_ms;
}
}  // namespace led_estado
