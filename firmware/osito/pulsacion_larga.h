// Separa una pulsación corta (cuenta al soltar) de una larga (cuenta al cumplir el tiempo).
// No usa Arduino (se prueba en la compu).
#pragma once

#include <cstdint>

class PulsacionLarga {
 public:
  enum class Tipo : uint8_t { Nada, Corta, Larga };

  explicit PulsacionLarga(uint32_t larga_ms) : larga_ms_(larga_ms) {}

  // Si ya está abajo al arrancar, no cuenta hasta que la suelten.
  void iniciar(bool abajo) {
    abajo_ = abajo;
    ya_conto_ = abajo;
  }

  Tipo actualizar(bool abajo, uint32_t ahora_ms) {
    if (abajo && !abajo_) {
      abajo_ = true;
      ya_conto_ = false;
      desde_ms_ = ahora_ms;
    } else if (abajo && !ya_conto_ && ahora_ms - desde_ms_ >= larga_ms_) {
      ya_conto_ = true;
      return Tipo::Larga;
    } else if (!abajo && abajo_) {
      abajo_ = false;
      return ya_conto_ ? Tipo::Nada : Tipo::Corta;
    }
    return Tipo::Nada;
  }

 private:
  uint32_t larga_ms_;
  uint32_t desde_ms_ = 0;
  bool abajo_ = false;
  bool ya_conto_ = false;
};
