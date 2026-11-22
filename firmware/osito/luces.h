// LEDs de la pancita: dos muestran el modo (y parpadean con el pulso) y uno es la alerta
// de animalito desconectado.
#pragma once

#include <protocolo.h>

namespace luces {
void iniciar();
void actualizar();

void mostrarModo(protocolo::Modo modo);
void pulso();  // apaga un instante los LEDs de modo, para marcar el ritmo
void alerta(bool encendida);
}  // namespace luces
