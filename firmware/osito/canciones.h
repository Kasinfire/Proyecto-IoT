// Canciones del modo Canción: las incluidas (canciones_datos.h, generado desde
// nodered/canciones.json) y la que mande Node-RED (cancion_recibida.h).
#pragma once

#include <cstring>

#include "cancion.h"
#include "cancion_recibida.h"
#include "canciones_datos.h"

namespace canciones {
constexpr char INICIAL[] = "estrellita";

// "prueba" NO es una canción: cada parte repite una sola nota, para oír que entra y sale con su
// animal. Notas MIDI: 36 do grave, 64 mi, 67 sol; pollito (canal 10): maraca 70, wood block 76.
inline constexpr Paso PRUEBA_BASE[] = {{36, 4}};
inline constexpr Paso PRUEBA_RANITA[] = {{64, 4}};
inline constexpr Paso PRUEBA_GATITO[] = {{67, 2}};
inline constexpr Paso PRUEBA_POLLITO[] = {{70, 1, 110}, {70, 1, 50}, {76, 1, 90}, {70, 1, 50}};
inline constexpr Cancion PRUEBA = {"prueba",
                                   250,
                                   {{PRUEBA_BASE, std::size(PRUEBA_BASE)},
                                    {PRUEBA_RANITA, std::size(PRUEBA_RANITA)},
                                    {PRUEBA_GATITO, std::size(PRUEBA_GATITO)},
                                    {PRUEBA_POLLITO, std::size(PRUEBA_POLLITO)}}};

// nullptr si no existe.
inline const Cancion* buscar(const char* nombre) {
  if (nombre == nullptr) {
    return nullptr;
  }
  for (const Cancion& cancion : INCLUIDAS) {
    if (strcmp(cancion.nombre, nombre) == 0) {
      return &cancion;
    }
  }
  return strcmp(nombre, PRUEBA.nombre) == 0 ? &PRUEBA : cancion_recibida::buscar(nombre);
}
}  // namespace canciones
