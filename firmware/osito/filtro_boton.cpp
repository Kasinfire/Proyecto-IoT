#include "filtro_boton.h"

void FiltroBoton::iniciar(bool presionado, uint32_t ahora_ms) {
  lectura_previa_ = presionado;
  estado_ = presionado;
  cambio_ms_ = ahora_ms;
  hubo_presion_ = false;
}

void FiltroBoton::actualizar(bool presionado, uint32_t ahora_ms) {
  // Si el botón sigue rebotando, reinicio el conteo desde el último cambio.
  if (presionado != lectura_previa_) {
    lectura_previa_ = presionado;
    cambio_ms_ = ahora_ms;
  }
  if (presionado != estado_ && ahora_ms - cambio_ms_ >= estable_ms_) {
    estado_ = presionado;
    if (estado_) {
      hubo_presion_ = true;
    }
  }
}

bool FiltroBoton::sePresiono() {
  const bool hubo = hubo_presion_;
  hubo_presion_ = false;
  return hubo;
}
