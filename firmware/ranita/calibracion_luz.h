// Lleva la "luz normal" del cuarto mientras la ranita está encendida, para que no dependa de cómo
// estaba la luz al arrancar. No usa Arduino (se prueba en la compu: ranita/pruebas/correr.sh).
//
// - Si hay más luz que la normal, nadie la está tapando: la normal sube rápido (~1 s). Así se
//   corrige sola si se encendió tapada o si prenden más luces.
// - Destapada, la normal baja despacio (~10 s): sigue cambios lentos, como la luz de la tarde.
// - Una lectura saturada (lámpara o cable flojo) no mueve la normal.
// - Si parece tapada pero la luz no se mueve durante CALIBRA_TAPADA_FIJA_MS, no es una mano (una
//   mano siempre tiembla un poco): apagaron luces. Esa luz pasa a ser la normal.
#pragma once

#include <cstdint>

#include "config.h"

class CalibracionLuz {
 public:
  void empezar(uint16_t luz, uint32_t ahora_ms) {
    normal_x16_ = static_cast<uint32_t>(luz) * 16;
    reiniciarQuieta(luz, ahora_ms);
  }

  uint16_t normal() const {
    return static_cast<uint16_t>(normal_x16_ / 16);
  }

  // Una lectura (ya promediada) cada LECTURA_CADA_MS. destapada: el nivel calculado es 0.
  // true si acaba de recalibrar de golpe porque la luz del cuarto bajó.
  bool actualizar(uint16_t luz, bool destapada, uint32_t ahora_ms) {
    if (luz >= config::LUZ_SATURADA) {
      reiniciarQuieta(luz, ahora_ms);
      return false;
    }
    const uint32_t luz_x16 = static_cast<uint32_t>(luz) * 16;
    if (luz_x16 > normal_x16_) {
      normal_x16_ += (luz_x16 - normal_x16_) / config::CALIBRA_SUBIR_LECTURAS;
      reiniciarQuieta(luz, ahora_ms);
      return false;
    }
    if (destapada) {
      normal_x16_ -= (normal_x16_ - luz_x16) / config::CALIBRA_BAJAR_LECTURAS;
      reiniciarQuieta(luz, ahora_ms);
      return false;
    }
    // Tapada: ¿la luz se queda quieta?
    const uint32_t margen = static_cast<uint32_t>(referencia_) * config::CALIBRA_QUIETA_PCT / 100 +
                            config::CALIBRA_QUIETA_MIN;
    const uint32_t diferencia = luz > referencia_ ? luz - referencia_ : referencia_ - luz;
    if (diferencia > margen) {
      reiniciarQuieta(luz, ahora_ms);
      return false;
    }
    if (ahora_ms - quieta_desde_ms_ < config::CALIBRA_TAPADA_FIJA_MS) {
      return false;
    }
    empezar(luz, ahora_ms);
    return true;
  }

 private:
  void reiniciarQuieta(uint16_t luz, uint32_t ahora_ms) {
    referencia_ = luz;
    quieta_desde_ms_ = ahora_ms;
  }

  uint32_t normal_x16_ = 0;  // x16 para que los pasos chicos no se pierdan al redondear
  uint16_t referencia_ = 0;
  uint32_t quieta_desde_ms_ = 0;
};
