// Qué hace cada tecla del teclado DJ del osito. No usa Arduino (se prueba en la compu).
//   A/B/C: modo.  1/2/3: silenciar o devolver a ranita/gatito/pollito.
//   *: grabar loop (o encimar otra capa).  #: play/stop.  D: deshacer la última capa
//   (mantenida 1 s = borrar todo el loop; llega como TECLA_D_LARGA).
//   7/9: más lento/más rápido.  4: eco.  5: redoble.  6: subida y caída.
//   8: ritmo de fondo.  0: acordes de fondo.
// En Simón la fila de abajo y 7/9 controlan el juego: 7/9 nivel, * empezar, # repetir, 0 pista.
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

constexpr char TECLA_D_LARGA = 'd';

enum class Tipo : uint8_t {
  Ninguna,
  CambiarModo,
  AlternarSilencio,
  GrabarLoop,
  AlternarLoop,
  BorrarLoop,
  DeshacerLoop,
  CambiarTempo,
  Eco,
  Redoble,
  Subida,
  Ritmo,
  Acordes,
  SimonNivel,
  SimonEmpezar,
  SimonRepetir,
  SimonPista,
};

struct Accion {
  Tipo tipo;
  protocolo::Modo modo;  // solo CambiarModo
  protocolo::Nodo nodo;  // solo AlternarSilencio
  int cuanto;            // CambiarTempo (%) y SimonNivel: cuánto sube o baja
};

Accion deTecla(char tecla, protocolo::Modo modo_actual);
}  // namespace acciones_dj
