// Banda de fondo del teclado DJ: un ritmo de batería y unos acordes que se repiten.
// Los cambios entran en el siguiente compás. Suena en canales del DJ, que el fade de Canción
// no toca. No usa Arduino (se prueba en la compu).
#pragma once

#include <cstdint>

#include "sintetizador.h"

namespace acompanamiento {
enum class Ritmo : uint8_t { Apagado, Tranquilo, Pop, Cumbia };
inline constexpr const char* NOMBRE_RITMO[] = {"apagado", "tranquilo", "pop", "cumbia"};

void reiniciar();
Ritmo siguienteRitmo();   // el que sonará desde el siguiente compás
bool alternarAcordes();   // true si quedaron encendidos
bool encendido();
Ritmo ritmoPedido();
bool acordesPedidos();
void alPulso(Sintetizador& salida);
void callar(Sintetizador& salida);  // suelta el acorde; lo elegido se conserva
}  // namespace acompanamiento
