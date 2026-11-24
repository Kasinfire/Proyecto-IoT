// Canciones del modo Canción. Cada parte es una lista de pasos que se repite sola;
// el índice de `partes` es el Nodo que la controla (Osito = la base, suena siempre).
//
// Falta que el equipo escoja la canción final ("Estrellita" o "Los pollitos dicen", sección 1
// de la especificación) y la transcriba aquí. Por ahora solo está "prueba".
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
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

// "prueba" NO es una canción: cada parte repite una sola nota para oír que entra y sale
// con su animal. Notas MIDI: 36 do grave, 64 mi, 67 sol. El pollito (canal 10) ya lleva un
// ritmo con acentos para probar el mecanismo: "CHK-chk-TOK-chk" (maraca 70, wood block 76).
inline constexpr Paso PRUEBA_BASE[] = {{36, 4}};
inline constexpr Paso PRUEBA_RANITA[] = {{64, 4}};
inline constexpr Paso PRUEBA_GATITO[] = {{67, 2}};
inline constexpr Paso PRUEBA_POLLITO[] = {{70, 1, 110}, {70, 1, 50}, {76, 1, 90}, {70, 1, 50}};

inline constexpr Cancion LISTA[] = {
    {"prueba",
     250,  // pulso corto para que quepa el ritmo del pollito; la base sigue cada 1 s
     {
         {PRUEBA_BASE, std::size(PRUEBA_BASE)},        // Osito
         {PRUEBA_RANITA, std::size(PRUEBA_RANITA)},    // Ranita
         {PRUEBA_GATITO, std::size(PRUEBA_GATITO)},    // Gatito
         {PRUEBA_POLLITO, std::size(PRUEBA_POLLITO)},  // Pollito
     }},
};
constexpr size_t NUM_CANCIONES = std::size(LISTA);

// nullptr si no existe (p. ej. "estrellita" mientras no se transcriba).
inline const Cancion* buscar(const char* nombre) {
  for (const Cancion& cancion : LISTA) {
    if (nombre != nullptr && strcmp(cancion.nombre, nombre) == 0) {
      return &cancion;
    }
  }
  return nullptr;
}
}  // namespace canciones
