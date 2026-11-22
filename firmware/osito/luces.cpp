// Modo en binario con dos LEDs: Canción = A, Libre = B, Simón = A y B.
#include "luces.h"

#include <Arduino.h>

#include "config.h"
#include "pines.h"

namespace luces {
namespace {
// Como van de 3.3 V al GPIO (con resistencia), LOW los prende y HIGH los apaga.
constexpr uint8_t LED_ENCENDIDO = LOW;
constexpr uint8_t LED_APAGADO = HIGH;

protocolo::Modo modo_actual = protocolo::Modo::Cancion;
bool en_pulso = false;
uint32_t inicio_pulso_ms = 0;
bool hay_alerta = false;
bool alerta_prendida = false;
uint32_t ultimo_parpadeo_ms = 0;

void escribirLed(uint8_t pin, bool encendido) {
  digitalWrite(pin, encendido ? LED_ENCENDIDO : LED_APAGADO);
}

void escribirModo(bool apagados) {
  const bool a = modo_actual != protocolo::Modo::Libre;
  const bool b = modo_actual != protocolo::Modo::Cancion;
  escribirLed(pines::LED_MODO_A, a && !apagados);
  escribirLed(pines::LED_MODO_B, b && !apagados);
}
}  // namespace

void iniciar() {
  pinMode(pines::LED_MODO_A, OUTPUT);
  pinMode(pines::LED_MODO_B, OUTPUT);
  pinMode(pines::LED_ALERTA, OUTPUT);
  escribirLed(pines::LED_ALERTA, false);
  escribirModo(false);
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  if (en_pulso && ahora_ms - inicio_pulso_ms >= config::PULSO_LED_MS) {
    en_pulso = false;
    escribirModo(false);
  }
  if (hay_alerta && ahora_ms - ultimo_parpadeo_ms >= config::PARPADEO_ALERTA_MS) {
    ultimo_parpadeo_ms = ahora_ms;
    alerta_prendida = !alerta_prendida;
    escribirLed(pines::LED_ALERTA, alerta_prendida);
  }
}

void mostrarModo(protocolo::Modo modo) {
  modo_actual = modo;
  escribirModo(en_pulso);
}

void pulso() {
  en_pulso = true;
  inicio_pulso_ms = millis();
  escribirModo(true);
}

void alerta(bool encendida) {
  if (encendida == hay_alerta) {
    return;
  }
  hay_alerta = encendida;
  if (!encendida) {
    alerta_prendida = false;
    escribirLed(pines::LED_ALERTA, false);
  }
}
}  // namespace luces
