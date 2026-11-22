#include "entradas.h"

#include <Arduino.h>
#include <promedio_movil.h>

#include <cstdlib>

#include "config.h"
#include "filtro_boton.h"
#include "pines.h"

namespace entradas {
namespace {
FiltroBoton boton(config::BOTON_ESTABLE_MS);
bool presion_simulada = false;

PromedioMovil<config::LECTURAS_POTENCIOMETRO> promedio;
uint32_t ultima_lectura_ms = 0;
uint8_t volumen_actual = 0;
bool cambio_volumen = false;

bool botonAbajo() {
  return digitalRead(pines::BOTON_MODO) == LOW;  // a GND con pull-up
}

void leerPotenciometro(uint32_t ahora_ms) {
  if (ahora_ms - ultima_lectura_ms < config::LECTURA_POTENCIOMETRO_CADA_MS) {
    return;
  }
  ultima_lectura_ms = ahora_ms;
  const uint16_t crudo = promedio.agregar(analogRead(pines::POTENCIOMETRO));
  const uint8_t nuevo = map(crudo, 0, config::ADC_MAX, 0, config::VOLUMEN_MAX);
  // Histéresis: sin esto el ruido del ADC reporta cambios de volumen todo el tiempo.
  if (abs(static_cast<int>(nuevo) - volumen_actual) >= config::CAMBIO_MINIMO_VOLUMEN) {
    volumen_actual = nuevo;
    cambio_volumen = true;
  }
}
}  // namespace

void iniciar() {
  pinMode(pines::BOTON_MODO, INPUT_PULLUP);
  boton.iniciar(botonAbajo(), millis());
  volumen_actual = map(analogRead(pines::POTENCIOMETRO), 0, config::ADC_MAX, 0,
                       config::VOLUMEN_MAX);
  cambio_volumen = true;
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  boton.actualizar(botonAbajo(), ahora_ms);
  leerPotenciometro(ahora_ms);
}

uint8_t volumen() {
  return volumen_actual;
}

bool huboCambioVolumen() {
  const bool hubo = cambio_volumen;
  cambio_volumen = false;
  return hubo;
}

bool sePresionoModo() {
  const bool simulada = presion_simulada;
  presion_simulada = false;
  return boton.sePresiono() || simulada;
}

void simularPresionModo() {
  presion_simulada = true;
}
}  // namespace entradas
