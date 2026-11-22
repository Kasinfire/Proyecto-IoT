// Lee las 7 teclas y manda un mensaje Nota al presionar y al soltar.

#pragma once

#include <cstdint>

namespace teclado {

void iniciar();
void actualizar();
bool estaPresionada(uint8_t indice);

}  // namespace teclado
