// Potenciómetro de volumen y botón de modo de la pancita del osito.
#pragma once

#include <cstdint>

namespace entradas {
void iniciar();
void actualizar();

uint8_t volumen();  // 0..config::VOLUMEN_MAX (escala MIDI)
bool huboCambioVolumen();
// simulada: la pulsación vino de "sim boton" (no se publica en el broker real).
bool sePresionoModo(bool& simulada);

// Para la simulación: cuenta como una pulsación, marcada como SIMULACIÓN por quien la llama.
void simularPresionModo();
}  // namespace entradas
