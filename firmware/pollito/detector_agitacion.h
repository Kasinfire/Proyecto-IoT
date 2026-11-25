// Decide cuándo los movimientos de la bolita son una sacudida. No usa Arduino (se prueba en la compu).
// Mira los últimos VENTANA_AGITACION_MS en cada cambio: así no se pierde una sacudida que caiga
// entre dos ventanas fijas, y responde sin esperar a que la ventana termine.
#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"

class DetectorAgitacion {
 public:
  // Un cierre o apertura del sensor. true si con este ya es una sacudida.
  bool alCambio(uint32_t ahora_ms) {
    tiempos_[siguiente_] = ahora_ms;
    siguiente_ = (siguiente_ + 1) % GUARDADOS;
    if (cuantos_ < GUARDADOS) {
      ++cuantos_;
    }
    if (recientes(ahora_ms) < config::CAMBIOS_PARA_GESTO ||
        (hubo_gesto_ && ahora_ms - ultimo_gesto_ms_ < config::INTERVALO_ENTRE_GESTOS_MS)) {
      return false;
    }
    hubo_gesto_ = true;
    ultimo_gesto_ms_ = ahora_ms;
    return true;
  }

  size_t recientes(uint32_t ahora_ms) const {
    size_t n = 0;
    for (size_t i = 0; i < cuantos_; ++i) {
      n += ahora_ms - tiempos_[i] < config::VENTANA_AGITACION_MS ? 1 : 0;
    }
    return n;
  }

  // Qué tan fuerte: crece con los cambios recientes, de 0 a `maxima`.
  uint8_t intensidad(uint32_t ahora_ms, uint8_t maxima) const {
    const size_t n = recientes(ahora_ms);
    return n >= config::CAMBIOS_PARA_INTENSIDAD_MAX
               ? maxima
               : static_cast<uint8_t>(n * maxima / config::CAMBIOS_PARA_INTENSIDAD_MAX);
  }

 private:
  static constexpr size_t GUARDADOS = 16;
  static_assert(config::CAMBIOS_PARA_INTENSIDAD_MAX <= GUARDADOS, "No caben los cambios");
  uint32_t tiempos_[GUARDADOS] = {};
  size_t siguiente_ = 0;
  size_t cuantos_ = 0;
  uint32_t ultimo_gesto_ms_ = 0;
  bool hubo_gesto_ = false;
};
