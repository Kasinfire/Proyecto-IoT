// Convierte la sombra de la mano sobre la LDR en un nivel 0..protocolo::NIVEL_SOMBRA_MAX.
#pragma once
#include <cstdint>

namespace sensor_sombra {
void iniciar();
void actualizar();
bool calibrando();
uint8_t nivel();    // 0 = destapada, protocolo::NIVEL_SOMBRA_MAX = totalmente tapada
bool huboCambio();  // true una sola vez cada que hay que avisarle al osito
}  // namespace sensor_sombra
