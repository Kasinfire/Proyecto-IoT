#include "teclado_dj.h"

#include <Arduino.h>

#include "acciones_dj.h"
#include "config.h"
#include "filtro_boton.h"
#include "pines.h"

namespace teclado_dj {
namespace {
using acciones_dj::COLUMNAS;
using acciones_dj::FILAS;

constexpr size_t COLA_MAX = 8;
// Tiempo para que la fila baje antes de leer las columnas (cables largos de la membrana).
constexpr uint32_t ASENTAR_US = 5;

FiltroBoton filtros[FILAS][COLUMNAS] = {
    {FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS),
     FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS)},
    {FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS),
     FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS)},
    {FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS),
     FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS)},
    {FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS),
     FiltroBoton(config::BOTON_ESTABLE_MS), FiltroBoton(config::BOTON_ESTABLE_MS)},
};
struct Pulsada {
  char tecla;
  bool simulada;
};
Pulsada cola[COLA_MAX];
size_t inicio = 0;
size_t cuantas = 0;

void meter(char tecla, bool simulada) {
  if (cuantas == COLA_MAX) {  // llena: se pierde la más vieja
    inicio = (inicio + 1) % COLA_MAX;
    --cuantas;
  }
  cola[(inicio + cuantas) % COLA_MAX] = {tecla, simulada};
  ++cuantas;
}

// Con la fila en LOW, una tecla presionada baja su columna (que tiene pull-up).
void leerFila(size_t fila, bool abajo[COLUMNAS]) {
  digitalWrite(pines::TECLADO_FILAS[fila], LOW);
  delayMicroseconds(ASENTAR_US);
  for (size_t c = 0; c < COLUMNAS; ++c) {
    abajo[c] = digitalRead(pines::TECLADO_COLUMNAS[c]) == LOW;
  }
  digitalWrite(pines::TECLADO_FILAS[fila], HIGH);
}
}  // namespace

void iniciar() {
  for (uint8_t pin : pines::TECLADO_FILAS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  for (uint8_t pin : pines::TECLADO_COLUMNAS) {
    pinMode(pin, INPUT_PULLUP);
  }
  const uint32_t ahora_ms = millis();
  bool abajo[COLUMNAS];
  for (size_t f = 0; f < FILAS; ++f) {
    leerFila(f, abajo);
    for (size_t c = 0; c < COLUMNAS; ++c) {
      filtros[f][c].iniciar(abajo[c], ahora_ms);
    }
  }
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  bool abajo[COLUMNAS];
  for (size_t f = 0; f < FILAS; ++f) {
    leerFila(f, abajo);
    for (size_t c = 0; c < COLUMNAS; ++c) {
      filtros[f][c].actualizar(abajo[c], ahora_ms);
      if (filtros[f][c].sePresiono()) {
        meter(acciones_dj::TECLAS[f][c], false);
      }
    }
  }
}

bool siguienteTecla(char& tecla, bool& simulada) {
  if (cuantas == 0) {
    return false;
  }
  tecla = cola[inicio].tecla;
  simulada = cola[inicio].simulada;
  inicio = (inicio + 1) % COLA_MAX;
  --cuantas;
  return true;
}

void simularTecla(char tecla) {
  meter(tecla, true);
}
}  // namespace teclado_dj
