// Mientras el sensor calibra, las mejillas parpadean para avisar que no tapen la ranita.
#include "mejillas.h"

#include <Arduino.h>
#include <protocolo.h>

#include "config.h"
#include "pines.h"
#include "sensor_sombra.h"

namespace mejillas {
namespace {
constexpr uint8_t LED_IZQ = 0;
constexpr uint8_t LED_DER = 1;

uint32_t ultimo_parpadeo_ms = 0;
bool encendidas = false;
int ultimo_brillo_izq = -1;  // -1 = todavía no se ha escrito nada
int ultimo_brillo_der = -1;

bool hay_luz_pedida = false;
uint8_t led_pedido = protocolo::LED_TODOS;
uint32_t inicio_luz_ms = 0;
uint16_t duracion_luz_ms = 0;

void escribirUna(uint8_t pin, int& ultimo, uint8_t brillo) {
  if (brillo == ultimo) {
    return;
  }
  analogWrite(pin, brillo);
  ultimo = brillo;
}

void escribir(uint8_t brillo_izq, uint8_t brillo_der) {
  escribirUna(pines::MEJILLA_IZQ, ultimo_brillo_izq, brillo_izq);
  escribirUna(pines::MEJILLA_DER, ultimo_brillo_der, brillo_der);
}

void escribir(uint8_t brillo) {
  escribir(brillo, brillo);
}

// true mientras hay una luz de Simón Dice que mostrar.
bool mostrarLuzPedida(uint32_t ahora_ms) {
  if (!hay_luz_pedida) {
    return false;
  }
  if (ahora_ms - inicio_luz_ms >= duracion_luz_ms) {
    hay_luz_pedida = false;
    return false;
  }
  const bool todas = led_pedido == protocolo::LED_TODOS;
  escribir(todas || led_pedido == LED_IZQ ? config::BRILLO_MAX : 0,
           todas || led_pedido == LED_DER ? config::BRILLO_MAX : 0);
  return true;
}
}  // namespace

void iniciar() {
  pinMode(pines::MEJILLA_IZQ, OUTPUT);
  pinMode(pines::MEJILLA_DER, OUTPUT);
  escribir(0);
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  if (sensor_sombra::calibrando()) {
    if (ahora_ms - ultimo_parpadeo_ms >= config::PARPADEO_CALIBRACION_MS) {
      ultimo_parpadeo_ms = ahora_ms;
      encendidas = !encendidas;
      escribir(encendidas ? config::BRILLO_MAX : 0);
    }
    return;
  }
  if (mostrarLuzPedida(ahora_ms)) {
    return;
  }
  escribir(map(sensor_sombra::nivel(), 0, protocolo::NIVEL_SOMBRA_MAX, 0, config::BRILLO_MAX));
}

void pedirLuz(uint8_t led, uint16_t duracion_ms) {
  if (led != LED_IZQ && led != LED_DER && led != protocolo::LED_TODOS) {
    return;
  }
  hay_luz_pedida = true;
  led_pedido = led;
  inicio_luz_ms = millis();
  duracion_luz_ms = duracion_ms;
}
}  // namespace mejillas
