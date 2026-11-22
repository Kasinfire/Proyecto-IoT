// Cola de tamaño fijo que se puede llenar desde el callback de ESP-NOW (tarea de Wi-Fi)
// y vaciar desde loop(). Si está llena, meter() devuelve false y no sobrescribe.
#pragma once

#include <freertos/FreeRTOS.h>

#include <cstddef>

template <typename T, size_t N>
class ColaCircular {
  static_assert(N > 0, "ColaCircular necesita al menos un lugar");

 public:
  bool meter(const T& elemento) {
    portENTER_CRITICAL(&candado_);
    const bool cabe = cantidad_ < N;
    if (cabe) {
      elementos_[(inicio_ + cantidad_) % N] = elemento;
      ++cantidad_;
    }
    portEXIT_CRITICAL(&candado_);
    return cabe;
  }

  bool sacar(T& elemento) {
    portENTER_CRITICAL(&candado_);
    const bool hay = cantidad_ > 0;
    if (hay) {
      elemento = elementos_[inicio_];
      inicio_ = (inicio_ + 1) % N;
      --cantidad_;
    }
    portEXIT_CRITICAL(&candado_);
    return hay;
  }

  bool vacia() const {
    return tamano() == 0;
  }
  bool llena() const {
    return tamano() == N;
  }
  size_t tamano() const {
    portENTER_CRITICAL(&candado_);
    const size_t cantidad = cantidad_;
    portEXIT_CRITICAL(&candado_);
    return cantidad;
  }

 private:
  T elementos_[N] = {};
  size_t inicio_ = 0;  // el más viejo
  size_t cantidad_ = 0;
  mutable portMUX_TYPE candado_ = portMUX_INITIALIZER_UNLOCKED;
};
