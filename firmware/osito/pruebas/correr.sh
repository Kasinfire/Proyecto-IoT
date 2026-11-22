#!/bin/sh
# Compila y corre en la compu (sin placa) las pruebas de la lógica pura del osito.
# Todo lo que imprime es SIMULACIÓN: no prueba radio, cifrado ni hardware.
set -e
AQUI="$(cd "$(dirname "$0")" && pwd)"
OSITO="$AQUI/.."
COMUN="$OSITO/../libraries/OrquestaComun/src"
SALIDA="${TMPDIR:-/tmp}/prueba_osito"
c++ -std=c++17 -Wall -Wextra -I"$AQUI/stubs" -I"$COMUN" -I"$OSITO" \
  "$AQUI/prueba_logica.cpp" \
  "$OSITO/estado_animales.cpp" "$OSITO/interprete.cpp" \
  -o "$SALIDA"
"$SALIDA"
