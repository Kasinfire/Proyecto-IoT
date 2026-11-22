// Osito: gateway y director musical. Recibe a los animales por ESP-NOW, decide qué suena
// y (cuando esté configurado) reporta al broker. Aquí solo se llaman los módulos.
#include <protocolo.h>

#include "animales.h"
#include "config.h"
#include "entradas.h"
#include "estado_animales.h"
#include "luces.h"
#include "simulacion.h"

namespace {
uint8_t modo = 0;  // Etapa 4: lo maneja el director musical
}  // namespace

void setup() {
  Serial.begin(config::BAUDIOS);
  Serial.println("[osito] Arrancando");
  entradas::iniciar();
  luces::iniciar();
  animales::iniciar();
  simulacion::iniciar();
}

void loop() {
  simulacion::actualizar();
  entradas::actualizar();
  animales::actualizar();
  luces::actualizar();

  animales::Recibido recibido;
  while (animales::siguiente(recibido)) {
    // Etapa 4: aquí los consume el director musical.
  }

  if (entradas::sePresionoModo()) {
    modo = (modo + 1) % protocolo::NUM_MODOS;
    luces::mostrarModo(static_cast<protocolo::Modo>(modo));
    Serial.printf("[osito] modo=%s\n", protocolo::NOMBRE_MODO[modo]);
  }
  if (entradas::huboCambioVolumen()) {
    Serial.printf("[osito] volumen=%u\n", entradas::volumen());
  }
  luces::alerta(estado_animales::algunoSeDesconecto(millis()));
}
