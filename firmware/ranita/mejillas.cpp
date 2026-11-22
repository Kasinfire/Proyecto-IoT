// Mientras el sensor calibra, las mejillas parpadean para avisar que no tapen la ranita.
#include <Arduino.h>
#include <protocolo.h>

#include "config.h"
#include "mejillas.h"
#include "pines.h"
#include "sensor_sombra.h"

namespace mejillas {
namespace {
uint32_t ultimo_parpadeo_ms = 0;
bool encendidas = false;
int ultimo_brillo = -1;  // -1 = todavía no se ha escrito nada

void escribir(uint8_t brillo) {
  if (brillo == ultimo_brillo) {
    return;
  }
  analogWrite(pines::MEJILLA_IZQ, brillo);
  analogWrite(pines::MEJILLA_DER, brillo);
  ultimo_brillo = brillo;
}
}  // namespace

void iniciar() {
  pinMode(pines::MEJILLA_IZQ, OUTPUT);
  pinMode(pines::MEJILLA_DER, OUTPUT);
  escribir(0);
}

void actualizar() {
  if (sensor_sombra::calibrando()) {
    const uint32_t ahora_ms = millis();
    if (ahora_ms - ultimo_parpadeo_ms >= config::PARPADEO_CALIBRACION_MS) {
      ultimo_parpadeo_ms = ahora_ms;
      encendidas = !encendidas;
      escribir(encendidas ? config::BRILLO_MAX : 0);
    }
    return;
  }
  escribir(map(sensor_sombra::nivel(), 0, protocolo::NIVEL_SOMBRA_MAX, 0, config::BRILLO_MAX));
}
}  // namespace mejillas
