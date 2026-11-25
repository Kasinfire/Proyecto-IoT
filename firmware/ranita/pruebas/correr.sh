#!/bin/sh
# SIMULACIÓN: compila y corre en la compu las pruebas de la ranita que no necesitan placa.
set -eu
cd "$(dirname "$0")"
SALIDA="${TMPDIR:-/tmp}/prueba_calibracion_ranita"
c++ -std=c++17 -Wall -Wextra -Werror -o "$SALIDA" prueba_calibracion.cpp
"$SALIDA"
