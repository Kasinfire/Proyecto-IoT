// Reemplazo temporal del enlace ESP-NOW para probar el gatito sin radio.
// Imprime lo que se enviaría y recibe comandos por el monitor serial:
//   L0..L6  luz en esa tecla (mensaje Luz)
//   LA      luz en todas las teclas
//   O0 / O1 osito desconectado / conectado
// Cuando exista el enlace real de OrquestaComun, se quita este módulo.

#pragma once

#include <protocolo.h>

#include <cstdint>

namespace enlace_simulado {

void iniciar();
void actualizar();
bool enviarNota(protocolo::Nota nota, uint8_t velocidad);
bool hayOsito();

}  // namespace enlace_simulado
