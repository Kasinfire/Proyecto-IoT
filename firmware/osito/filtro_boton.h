// Antirrebote sin delay(): un cambio solo cuenta cuando la lectura se queda igual estable_ms.
// No lee el pin (recibe la lectura y la hora), así se prueba en la compu.
// Posible duplicación futura: es el mismo algoritmo que Antirrebote del PR #5
// (OrquestaComun/antirrebote.h). Cuando ese PR entre a main, usar ese y borrar este.
#pragma once

#include <cstdint>

class FiltroBoton {
 public:
  explicit FiltroBoton(uint32_t estable_ms) : estable_ms_(estable_ms) {}

  // Parte del estado real para que un botón ya presionado al arrancar no cuente como pulsación.
  void iniciar(bool presionado, uint32_t ahora_ms);
  void actualizar(bool presionado, uint32_t ahora_ms);

  bool presionado() const {
    return estado_;
  }
  bool sePresiono();  // true una sola vez por pulsación

 private:
  uint32_t estable_ms_;
  bool lectura_previa_ = false;
  bool estado_ = false;
  uint32_t cambio_ms_ = 0;
  bool hubo_presion_ = false;
};
