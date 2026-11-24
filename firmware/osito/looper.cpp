#include "looper.h"

#include "config.h"

namespace looper {
namespace {
Nota notas[config::LOOP_NOTAS_MAX] = {};
size_t num_notas = 0;
uint8_t paso_actual = 0;
uint8_t pulsos_grabados = 0;
bool esta_grabando = false;
bool esta_sonando = false;
// Cuántas notas había al empezar cada capa: deshacer vuelve a ese punto.
size_t inicio_capa[config::LOOP_CAPAS_MAX] = {};
size_t num_capas = 0;
bool esperando_inicio = false;  // grabación nueva: arranca en el siguiente pulso

void abrirCapa() {
  if (num_capas == config::LOOP_CAPAS_MAX) {  // lleno: la más vieja ya no se puede deshacer
    for (size_t i = 1; i < num_capas; ++i) {
      inicio_capa[i - 1] = inicio_capa[i];
    }
    --num_capas;
  }
  inicio_capa[num_capas++] = num_notas;
}

// Una capa en la que no se tocó nada no cuenta.
void cerrarCapa() {
  if (num_capas > 0 && inicio_capa[num_capas - 1] == num_notas) {
    --num_capas;
  }
}
}  // namespace

void reiniciar() {
  num_capas = 0;
  num_notas = 0;
  paso_actual = 0;
  pulsos_grabados = 0;
  esta_grabando = false;
  esta_sonando = false;
  esperando_inicio = false;
}

void grabar() {
  if (esta_grabando) {
    return;
  }
  esta_grabando = true;
  pulsos_grabados = 0;
  abrirCapa();
  if (num_notas == 0) {
    esta_sonando = false;
    esperando_inicio = true;
  } else {
    esta_sonando = true;  // encimar: sigue sonando lo que ya había
  }
}

bool alternar() {
  if (num_notas == 0) {  // grabación nueva sin notas todavía: # la cancela
    reiniciar();
    return false;
  }
  esta_sonando = !esta_sonando;
  if (!esta_sonando && esta_grabando) {
    esta_grabando = false;
    esperando_inicio = false;
    cerrarCapa();
  }
  return esta_sonando;
}

void borrar() {
  reiniciar();
}

bool deshacer() {
  if (num_capas == 0) {
    return false;
  }
  num_notas = inicio_capa[--num_capas];
  esta_grabando = false;
  esperando_inicio = false;
  if (num_notas == 0) {
    esta_sonando = false;
  }
  return true;
}

void anotar(protocolo::Nodo nodo, uint8_t nota, uint8_t velocidad, bool al_siguiente) {
  if (!esta_grabando || num_notas == config::LOOP_NOTAS_MAX) {
    return;
  }
  uint8_t donde = paso_actual;
  if (esperando_inicio) {
    donde = 0;  // lo que se toca antes del primer pulso cae en el arranque
  } else if (al_siguiente) {
    donde = (paso_actual + 1) % config::LOOP_PULSOS;
  }
  for (size_t i = 0; i < num_notas; ++i) {  // la misma nota dos veces en un paso no suma
    if (notas[i].paso == donde && notas[i].nodo == nodo && notas[i].nota == nota) {
      return;
    }
  }
  notas[num_notas++] = {donde, nodo, nota, velocidad};
}

size_t alPulso(Nota* salida, size_t max, bool& termino_grabacion) {
  termino_grabacion = false;
  if (esperando_inicio) {
    esperando_inicio = false;
    paso_actual = 0;
  } else {
    paso_actual = (paso_actual + 1) % config::LOOP_PULSOS;
    if (esta_grabando && ++pulsos_grabados == config::LOOP_PULSOS) {
      esta_grabando = false;
      cerrarCapa();
      esta_sonando = num_notas > 0;
      termino_grabacion = true;
    }
  }
  size_t cuantas = 0;
  if (esta_sonando) {
    for (size_t i = 0; i < num_notas && cuantas < max; ++i) {
      if (notas[i].paso == paso_actual) {
        salida[cuantas++] = notas[i];
      }
    }
  }
  return cuantas;
}

bool grabando() {
  return esta_grabando;
}

bool sonando() {
  return esta_sonando;
}

size_t numNotas() {
  return num_notas;
}

uint8_t paso() {
  return paso_actual;
}
}  // namespace looper
