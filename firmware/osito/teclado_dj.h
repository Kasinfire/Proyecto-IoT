// Lee el teclado DJ 4x4 sin bloquear el loop: en cada vuelta revisa las 4 filas y filtra el
// rebote de cada tecla con FiltroBoton.
#pragma once

namespace teclado_dj {
void iniciar();
void actualizar();
// La siguiente tecla presionada ('1'..'9', '0', 'A'..'D', '*', '#'); false si no hay.
// simulada: vino de "sim tecla" (no se publica en el broker real).
bool siguienteTecla(char& tecla, bool& simulada);

// Para la simulación: cuenta como una tecla presionada, marcada como SIMULACIÓN por quien llama.
void simularTecla(char tecla);
}  // namespace teclado_dj
