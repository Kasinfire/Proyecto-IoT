// Comandos por el monitor serie (115200, "New Line") para probar el osito sin los otros
// animales. Todo lo que entra por aquí sale marcado como SIMULACIÓN, nunca como RADIO, y
// se salta el cifrado y el contra-replay (eso solo se prueba con dos placas).
// Escribe "ayuda" para ver los comandos.
#pragma once

#include "transporte_simulado.h"

namespace simulacion {
void iniciar(TransporteSimulado& broker);
void actualizar();
}  // namespace simulacion
