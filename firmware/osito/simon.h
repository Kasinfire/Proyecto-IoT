// Simón Dice: el osito elige un animal conectado (y en el gatito una tecla), pide que se
// ilumine y espera el gesto correcto. Cuenta aciertos y errores. No usa Arduino.
#pragma once

#include <protocolo.h>

#include <cstdint>

#include "interprete.h"

namespace simon {
enum class Resultado : uint8_t { Nada, Acierto, Error, TiempoAgotado };
inline constexpr const char* NOMBRE_RESULTADO[] = {"nada", "acierto", "error", "tiempo agotado"};

void iniciar(uint32_t semilla, uint32_t ahora_ms);

// true cuando hay un pedido nuevo para mandar a los animales (PayloadLuz).
// Si no hay nadie conectado, espera sin pedir nada.
bool actualizar(uint32_t ahora_ms, protocolo::PayloadLuz& pedido, Resultado& resultado);

// Solo cuentan gestos "de inicio": soltar una tecla o sombra 0 se ignoran.
Resultado alEvento(const interprete::Evento& evento, uint32_t ahora_ms);

uint16_t aciertos();
uint16_t errores();
}  // namespace simon
