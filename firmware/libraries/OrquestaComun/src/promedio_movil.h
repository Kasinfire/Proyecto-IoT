// Promedio de las últimas N lecturas, para que las lecturas ruidosas del ADC no tiemblen.
// Lleva la suma acumulada: cada lectura nueva resta la que sale y suma la que entra,
// en lugar de recorrer el arreglo completo.
#pragma once

#include <cstddef>
#include <cstdint>

template <size_t N>
class PromedioMovil {
  static_assert(N > 0, "PromedioMovil necesita al menos una lectura");

 public:
  // Agrega una lectura y devuelve el promedio actual.
  uint16_t agregar(uint16_t lectura) {
    if (cantidad_ == N) {
      suma_ -= lecturas_[indice_];
    } else {
      ++cantidad_;
    }
    lecturas_[indice_] = lectura;
    suma_ += lectura;
    indice_ = (indice_ + 1) % N;
    return valor();
  }

  // Mientras no haya N lecturas, promedia solo las que hay.
  uint16_t valor() const {
    return cantidad_ == 0 ? 0 : suma_ / cantidad_;
  }

  void reiniciar() {
    suma_ = 0;
    indice_ = 0;
    cantidad_ = 0;
  }

 private:
  uint16_t lecturas_[N] = {};
  uint32_t suma_ = 0;  // 32 bits: N lecturas de hasta 4095 no caben en 16
  size_t indice_ = 0;  // dónde va la siguiente lectura
  size_t cantidad_ = 0;
};
