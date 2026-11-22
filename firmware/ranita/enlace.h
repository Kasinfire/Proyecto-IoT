// Versión provisional del enlace con el osito: imprime por Serial lo que mandaría.
// Tiene la misma forma que el enlace de OrquestaComun (PENDIENTES.md, punto 4); cuando
// ese exista, se borran enlace.h/.cpp de aquí y ranita.ino queda igual.
#pragma once
#include <protocolo.h>

namespace enlace {
void iniciar();
void actualizar();  // manda el latido cada protocolo::LATIDO_CADA_MS
bool enviar(const protocolo::PayloadSombra& payload);
}  // namespace enlace
