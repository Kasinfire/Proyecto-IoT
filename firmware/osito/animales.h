// Recibe a la ranita, el gatito y el pollito: lo que llega por ESP-NOW y lo que inyecta la
// simulación pasan por el mismo camino, pero siempre etiquetados con su fuente.
// También manda el latido del osito, que la librería no manda.
#pragma once

#include <enlace.h>

#include "estado_animales.h"
#include "interprete.h"

namespace animales {
struct Recibido {
  interprete::Evento evento;
  estado_animales::Fuente fuente;
};

// Osito: después de conectarse al router (cuando lo haya), para usar su mismo canal.
void iniciar();
void actualizar();

bool radioLista();

// Lo usa el módulo de simulación; nunca se marca como Fuente::Radio.
void inyectarSimulado(const enlace::Mensaje& mensaje);

// Gestos ya validados, en orden de llegada (sin latidos).
bool siguiente(Recibido& recibido);
}  // namespace animales
