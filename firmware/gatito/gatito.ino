// Gatito: teclado de 7 teclas (melodía). Solo orquesta los módulos.

#include "enlace_simulado.h"
#include "luces.h"
#include "teclado.h"

void setup() {
  Serial.begin(115200);
  teclado::iniciar();
  luces::iniciar();
  enlace_simulado::iniciar();
  Serial.println("# Gatito listo");
}

void loop() {
  teclado::actualizar();
  luces::actualizar();
  enlace_simulado::actualizar();
}
