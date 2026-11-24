#include "luces.h"

#include <Arduino.h>
#include <enlace.h>
#include <protocolo.h>

#include "config.h"
#include "pines.h"
#include "teclado.h"

namespace {

uint32_t luz_hasta_ms[pines::NUM_TECLAS] = {};
uint32_t ultimo_parpadeo_ms = 0;
bool fase_encendida = false;

bool hayLuzPedida(size_t indice, uint32_t ahora_ms) {
  // La resta con signo evita fallar cuando millis() desborda.
  return static_cast<int32_t>(luz_hasta_ms[indice] - ahora_ms) > 0;
}

}  // namespace

namespace luces {

void iniciar() {
  for (const uint8_t pin : pines::LEDS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void pedirLuz(uint8_t led, uint16_t duracion_ms) {
  const uint32_t hasta_ms = millis() + duracion_ms;
  if (led == protocolo::LED_TODOS) {
    for (auto& luz_hasta : luz_hasta_ms) {
      luz_hasta = hasta_ms;
    }
  } else if (led < pines::NUM_TECLAS) {
    luz_hasta_ms[led] = hasta_ms;
  }
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  const bool hay_osito = enlace::estaConectado(protocolo::Nodo::Osito);

  if (!hay_osito && (ahora_ms - ultimo_parpadeo_ms) >= config::PARPADEO_SIN_OSITO_MS) {
    ultimo_parpadeo_ms = ahora_ms;
    fase_encendida = !fase_encendida;
  }

  for (size_t i = 0; i < pines::NUM_TECLAS; i++) {
    const bool parpadeo = !hay_osito && fase_encendida;
    const bool encender = teclado::estaPresionada(i) || hayLuzPedida(i, ahora_ms) || parpadeo;
    digitalWrite(pines::LEDS[i], encender ? HIGH : LOW);
  }
}

}  // namespace luces
