// El botón solo cuenta una pulsación si la lectura se queda estable el tiempo indicado.
// Recibo la lectura y la hora en vez de leer el pin; así lo pruebo en la compu sin placa.
// OrquestaComun ya trae Antirrebote (PR #5); el osito sigue con este filtro local.
#pragma once

#include <cstdint>

class FiltroBoton {
 public:
  explicit FiltroBoton(uint32_t estable_ms) : estable_ms_(estable_ms) {}

  // Si el botón ya está presionado al arrancar, no lo cuento como pulsación.
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
