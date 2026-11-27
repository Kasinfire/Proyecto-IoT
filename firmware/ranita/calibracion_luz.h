// Lleva la "luz normal" del cuarto mientras la ranita está encendida, para que no dependa de cómo
// estaba la luz al arrancar. No usa Arduino (se prueba en la compu: ranita/pruebas/correr.sh).
//
// - Un poco más de luz (hasta +4 %) se sigue despacio. Mucha más luz solo cuenta si se queda
//   estable CALIBRA_SUBIR_ESTABLE_MS: prender las luces o haberse encendido tapada, sí; una lámpara
//   en la mano o un cable flojo, no.
// - Si tras subir la luz vuelve a la normal de antes y se queda ahí, se regresa a ella enseguida.
// - Destapada, la normal baja despacio (~10 s): sigue cambios lentos, como la luz de la tarde.
// - Si parece tapada pero la luz no se mueve durante CALIBRA_TAPADA_FIJA_MS, no es una mano (una
//   mano siempre tiembla un poco): apagaron luces. Esa luz pasa a ser la normal.
#pragma once

#include <cstdint>

#include "config.h"

class CalibracionLuz {
 public:
  void empezar(uint16_t luz, uint32_t ahora_ms) {
    normal_x16_ = static_cast<uint32_t>(luz) * 16;
    anterior_ = 0;
    reiniciarQuieta(luz, ahora_ms);
    reiniciarSubida(luz, ahora_ms);
  }

  uint16_t normal() const {
    return static_cast<uint16_t>(normal_x16_ / 16);
  }

  // Una lectura (ya promediada) cada LECTURA_CADA_MS. destapada: el nivel calculado es 0.
  // true si acaba de recalibrar de golpe porque la luz del cuarto bajó.
  bool actualizar(uint16_t luz, bool destapada, uint32_t ahora_ms) {
    const uint32_t luz_x16 = static_cast<uint32_t>(luz) * 16;
    const uint16_t actual = normal();
    if (luz > actual + margen(actual)) {
      // Mucha más luz: ¿se queda?
      if (!parecida(luz, subida_)) {
        reiniciarSubida(luz, ahora_ms);
      } else if (ahora_ms - subida_desde_ms_ >= config::CALIBRA_SUBIR_ESTABLE_MS) {
        anterior_ = actual;
        normal_x16_ = luz_x16;
        reiniciarSubida(luz, ahora_ms);
      }
      reiniciarQuieta(luz, ahora_ms);
      return false;
    }
    reiniciarSubida(luz, ahora_ms);
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
    if (!parecida(luz, referencia_)) {
      reiniciarQuieta(luz, ahora_ms);
      return false;
    }
    const uint32_t quieta_ms = ahora_ms - quieta_desde_ms_;
    const bool volvio = anterior_ != 0 && parecida(luz, anterior_) &&
                        quieta_ms >= config::CALIBRA_REGRESAR_MS;
    if (!volvio && quieta_ms < config::CALIBRA_TAPADA_FIJA_MS) {
      return false;
    }
    empezar(luz, ahora_ms);
    return true;
  }

 private:
  static uint32_t margen(uint16_t referencia) {
    return static_cast<uint32_t>(referencia) * config::CALIBRA_QUIETA_PCT / 100 +
           config::CALIBRA_QUIETA_MIN;
  }

  static bool parecida(uint16_t luz, uint16_t referencia) {
    const uint32_t diferencia = luz > referencia ? luz - referencia : referencia - luz;
    return diferencia <= margen(referencia);
  }

  void reiniciarQuieta(uint16_t luz, uint32_t ahora_ms) {
    referencia_ = luz;
    quieta_desde_ms_ = ahora_ms;
  }

  void reiniciarSubida(uint16_t luz, uint32_t ahora_ms) {
    subida_ = luz;
    subida_desde_ms_ = ahora_ms;
  }

  uint32_t normal_x16_ = 0;  // x16 para que los pasos chicos no se pierdan al redondear
  uint16_t referencia_ = 0;
  uint32_t quieta_desde_ms_ = 0;
  uint16_t subida_ = 0;
  uint32_t subida_desde_ms_ = 0;
  uint16_t anterior_ = 0;  // la normal antes de la última subida, por si la luz regresa
};
