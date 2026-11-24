// Qué hace cada tecla del teclado DJ del osito. No usa Arduino (se prueba en la compu).
//   A/B/C: modo.  1/2/3: silenciar o devolver a ranita/gatito/pollito.
//   *: grabar loop (o encimar otra capa).  #: play/stop del loop.  D: borrar el loop.
//   7/9: más lento/más rápido.
//   4: eco on/off.  5: redoble.  6: subida y caída.  8: octava on/off.  0: pío-pío.
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>

namespace acciones_dj {
constexpr size_t FILAS = 4;
constexpr size_t COLUMNAS = 4;
// Como está impreso en la membrana, de arriba a abajo y de izquierda a derecha.
inline constexpr char TECLAS[FILAS][COLUMNAS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

enum class Tipo : uint8_t {
  Ninguna,
  CambiarModo,
  AlternarSilencio,
  GrabarLoop,
  AlternarLoop,
  BorrarLoop,
  CambiarTempo,
  Eco,
  Redoble,
  Subida,
  Octava,
  Pio,
};

struct Accion {
  Tipo tipo;
  protocolo::Modo modo;  // solo CambiarModo
  protocolo::Nodo nodo;  // solo AlternarSilencio
  int tempo_pct;         // solo CambiarTempo: cuánto sube o baja
};

Accion deTecla(char tecla);
}  // namespace acciones_dj
