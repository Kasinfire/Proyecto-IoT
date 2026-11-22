// Osito: gateway y director musical. Recibe a los animales por ESP-NOW, decide qué suena
// y (cuando esté configurado) reporta al broker. Aquí solo se llaman los módulos.
#include "animales.h"
#include "config.h"
#include "simulacion.h"

void setup() {
  Serial.begin(config::BAUDIOS);
  Serial.println("[osito] Arrancando");
  animales::iniciar();
  simulacion::iniciar();
}

void loop() {
  simulacion::actualizar();
  animales::actualizar();

  animales::Recibido recibido;
  while (animales::siguiente(recibido)) {
    // Etapa 4: aquí los consume el director musical.
  }
}
