#include "teclado.h"

#include <Arduino.h>
#include <antirrebote.h>
#include <enlace.h>
#include <protocolo.h>

#include <iterator>

#include "config.h"
#include "pines.h"

namespace {

constexpr bool ACTIVO_EN_BAJO = true;

// Una por tecla, en el mismo orden que protocolo::Nota.
Antirrebote teclas[] = {
    {pines::BOTONES[0], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
    {pines::BOTONES[1], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
    {pines::BOTONES[2], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
    {pines::BOTONES[3], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
    {pines::BOTONES[4], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
    {pines::BOTONES[5], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
    {pines::BOTONES[6], config::ANTIRREBOTE_MS, ACTIVO_EN_BAJO},
};

static_assert(std::size(teclas) == pines::NUM_TECLAS, "Falta o sobra una tecla");
static_assert(pines::NUM_TECLAS == protocolo::NUM_NOTAS, "Una tecla por cada nota del protocolo");

void enviarNota(protocolo::Nota nota, uint8_t velocidad) {
  const bool enviada = enlace::enviar(protocolo::PayloadNota{protocolo::aEntero(nota), velocidad});
  Serial.printf("[gatito] nota %s vel=%u%s\n", protocolo::NOMBRE_NOTA[protocolo::aEntero(nota)],
                velocidad, enviada ? "" : " (NO enviada)");
}

}  // namespace

namespace teclado {

void iniciar() {
  for (auto& tecla : teclas) {
    tecla.iniciar();
  }
}

void actualizar() {
  for (size_t i = 0; i < std::size(teclas); i++) {
    teclas[i].actualizar();

    const auto nota = static_cast<protocolo::Nota>(i);
    if (teclas[i].sePresiono()) {
      enviarNota(nota, config::VELOCIDAD_PRESIONAR);
    }
    if (teclas[i].seSolto()) {
      enviarNota(nota, protocolo::VELOCIDAD_SOLTO);
    }
  }
}

bool estaPresionada(uint8_t indice) {
  return indice < std::size(teclas) && teclas[indice].presionado();
}

}  // namespace teclado
