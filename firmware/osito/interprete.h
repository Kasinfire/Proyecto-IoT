// Convierte un mensaje del enlace en un evento del osito y revisa que tenga sentido:
// tipo correcto para ese animal y valores dentro de rango. No usa Arduino (se prueba en la compu).
#pragma once

#include <enlace.h>
#include <protocolo.h>

#include <cstddef>
#include <cstdint>

namespace interprete {
enum class Clase : uint8_t { Latido, Sombra, Nota, Agitar, Invalido };

struct Evento {
  protocolo::Nodo nodo;
  Clase clase;
  uint8_t valor;      // Sombra: nivel 0..100. Nota: aEntero(Nota). Agitar: intensidad 0..100.
  uint8_t velocidad;  // solo Nota: 1..127 al presionar, VELOCIDAD_SOLTO al soltar
  bool es_gesto;      // cuenta como "el niño está tocando"
};

Evento interpretar(const enlace::Mensaje& mensaje);

// Texto corto para el monitor serie, p. ej. "gatito nota mi vel=90".
void describir(const Evento& evento, char* destino, size_t tamano);
}  // namespace interprete
