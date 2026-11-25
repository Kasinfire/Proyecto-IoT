// Recibe por trozos una canción creada en la página. Se arma en un espacio que no está sonando y
// solo queda disponible si llegó completa y es válida. Vive en RAM: Node-RED la manda otra vez al
// elegirla. No usa Arduino.
#pragma once

#include <cstddef>
#include <cstdint>

#include "cancion.h"

namespace cancion_recibida {
void reiniciar();
// en_uso: la que suena, para no escribir encima. false si el nombre o el pulso no valen.
bool empezar(int32_t carga, const char* nombre, uint32_t pulso_ms,
             const canciones::Cancion* en_uso);
// pasos: "nota.pulsos.velocidad,…". Los trozos de una parte llegan en orden (desde = cuántos van);
// uno repetido se ignora. Si algo no cuadra, toda la carga queda descartada.
bool agregar(int32_t carga, uint32_t parte, uint32_t desde, const char* pasos);
// totales: pasos que debe tener cada parte. true = ya se puede elegir.
bool terminar(int32_t carga, const uint32_t (&totales)[protocolo::NUM_NODOS]);
// La más nueva con ese nombre, o nullptr.
const canciones::Cancion* buscar(const char* nombre);
}  // namespace cancion_recibida
