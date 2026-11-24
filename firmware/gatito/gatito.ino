// Gatito: teclado de 7 teclas (melodía). Solo orquesta los módulos.

#include <enlace.h>
#include <protocolo.h>

#include "luces.h"
#include "mensajes_osito.h"
#include "teclado.h"

void setup() {
  Serial.begin(115200);
  enlace::iniciar(protocolo::Nodo::Gatito);
  teclado::iniciar();
  luces::iniciar();
  Serial.println("# Gatito listo");
}

void loop() {
  teclado::actualizar();
  mensajes_osito::actualizar();
  luces::actualizar();
  enlace::actualizar();
}
