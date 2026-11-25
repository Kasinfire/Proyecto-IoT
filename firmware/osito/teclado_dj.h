// Lee el teclado DJ 4x4 sin bloquear el loop: en cada vuelta revisa las 4 filas y filtra el
// rebote de cada tecla con FiltroBoton.
#pragma once

namespace teclado_dj {
void iniciar();
void actualizar();
// La siguiente tecla presionada ('1'..'9', '0', 'A'..'D', '*', '#'), o
// TECLA_D_LARGA si mantuvieron D; false si no hay.
// Remota: del panel de la página. Simulada: de "sim tecla" (no se publica en el broker real).
enum class Origen { Fisica, Remota, Simulada };
bool siguienteTecla(char& tecla, Origen& origen);

// Entra como si la hubieran apretado.
void meterRemota(char tecla);

// Para la simulación: cuenta como una tecla presionada, marcada como SIMULACIÓN por quien llama.
void simularTecla(char tecla);
}  // namespace teclado_dj
