// Osito: gateway y director musical. Recibe a los animales por ESP-NOW, decide qué suena
// y (cuando esté configurado) reporta al broker. Aquí solo se llaman los módulos.
#include "config.h"

void setup() {
  Serial.begin(config::BAUDIOS);
  Serial.println("[osito] Arrancando");
}

void loop() {}
