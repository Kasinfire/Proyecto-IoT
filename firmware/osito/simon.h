// Simón Dice: muestra una secuencia de animales (en el gatito, teclas) y el niño la repite.
// Empieza con un paso; al acertar se agrega otro, hasta SIMON_NIVEL_MAX, y todo va más rápido.
// Al fallar se repite el mismo reto; dos fallos seguidos bajan un nivel. No usa Arduino.
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>

#include "interprete.h"

namespace simon {
enum class Resultado : uint8_t { Nada, Acierto, Error, TiempoAgotado };
inline constexpr const char* NOMBRE_RESULTADO[] = {"nada", "acierto", "error", "tiempo agotado"};

struct Paso {
  protocolo::Nodo nodo;
  uint8_t led;  // como en PayloadLuz: tecla del gatito, 0 el pollito, LED_TODOS la ranita
};

struct Salida {
  bool hay_paso;  // hay que prender la luz y tocar la nota de `paso`
  Paso paso;
  uint16_t luz_ms;
  bool empieza_reto;  // primer paso de un turno nuevo (repetir con # no cuenta como turno)
  Resultado resultado;
};

void iniciar(uint32_t semilla, uint32_t ahora_ms);

// Si no hay a quién pedirle algo (nadie conectado o todos silenciados), espera.
Salida actualizar(uint32_t ahora_ms, const bool (&silenciados)[protocolo::NUM_NODOS]);

// Solo cuentan gestos que empiezan: ni soltar, ni seguir tapando la ranita, ni los rebotes del
// pollito. Así un gesto del turno anterior no responde el siguiente.
Resultado alEvento(const interprete::Evento& evento, uint32_t ahora_ms);

// Controles del terapeuta.
void empezar(uint32_t ahora_ms);  // nivel 1 y cuenta en cero
void repetir(uint32_t ahora_ms);  // muestra otra vez el mismo reto
bool pista();                     // muestra solo el paso que toca; false si no es turno del niño
uint8_t cambiarNivel(int cuanto, uint32_t ahora_ms);

uint8_t nivel();
size_t largo();
Paso pasoDe(size_t indice);
uint16_t aciertos();
uint16_t errores();
}  // namespace simon
