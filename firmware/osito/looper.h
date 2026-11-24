// Looper del DJ: graba las notas que tocan los animales y las repite en bucle.
// El tiempo se mide en pulsos del director: cada nota se guarda en un paso de la rejilla,
// así el loop no queda chueco. No usa Arduino (se prueba en la compu).
//
//   grabar() con el loop vacío: empieza en el siguiente pulso y graba config::LOOP_PULSOS;
//                               al terminar se repite solo.
//   grabar() con loop:          encima otra capa durante una vuelta, sin dejar de sonar.
//   alternar():                 play / stop.   borrar(): lo deja vacío.
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>

namespace looper {
struct Nota {
  uint8_t paso;
  protocolo::Nodo nodo;
  uint8_t nota;
  uint8_t velocidad;
};

void reiniciar();
void grabar();
bool alternar();  // devuelve true si quedó sonando
void borrar();

// Una nota que se tocó en vivo. al_siguiente: llegó en la segunda mitad del pulso, así
// que se acomoda en el paso que viene.
void anotar(protocolo::Nodo nodo, uint8_t nota, uint8_t velocidad, bool al_siguiente);

// Avanza un paso. Copia en `salida` las notas que tocan ahora (si está sonando) y devuelve
// cuántas. `termino_grabacion` queda en true en el pulso en que se cierra una grabación.
size_t alPulso(Nota* salida, size_t max, bool& termino_grabacion);

bool grabando();
bool sonando();
size_t numNotas();
uint8_t paso();
}  // namespace looper
