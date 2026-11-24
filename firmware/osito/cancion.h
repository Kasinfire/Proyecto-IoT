// Forma de una canción. Cada parte es una lista de pasos que se repite sola; el índice de
// `partes` es el Nodo que la controla (Osito = la base, suena siempre).
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace canciones {
constexpr uint8_t SILENCIO = 0;

struct Paso {
  uint8_t nota_midi;  // SILENCIO = no suena en este paso
  uint8_t pulsos;     // duración, en pulsos
  uint8_t velocidad = 0;  // qué tan fuerte (1..127); 0 = la de siempre (VELOCIDAD_CANCION)
};

struct Parte {
  const Paso* pasos;
  size_t num_pasos;
};

struct Cancion {
  const char* nombre;  // el texto del campo "cancion" de orquesta/control/modo
  uint16_t pulso_ms;
  Parte partes[protocolo::NUM_NODOS];
};
}  // namespace canciones
