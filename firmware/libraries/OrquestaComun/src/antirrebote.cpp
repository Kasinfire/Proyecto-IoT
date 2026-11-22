#include "antirrebote.h"

Antirrebote::Antirrebote(uint8_t pin, uint32_t estable_ms, bool activo_en_bajo)
    : pin_(pin), estable_ms_(estable_ms), activo_en_bajo_(activo_en_bajo) {}

bool Antirrebote::leerCrudo() const {
  bool nivel_alto = digitalRead(pin_) == HIGH;
  return activo_en_bajo_ ? !nivel_alto : nivel_alto;
}

void Antirrebote::iniciar() {
  pinMode(pin_, activo_en_bajo_ ? INPUT_PULLUP : INPUT);
  // Se parte del estado real para que un botón ya presionado al arrancar
  // no genere un flanco falso.
  lectura_previa_ = leerCrudo();
  estado_ = lectura_previa_;
  cambio_ms_ = millis();
}

void Antirrebote::actualizar() {
  const bool crudo = leerCrudo();
  const uint32_t ahora_ms = millis();

  // Cada vez que la lectura cambia se reinicia el reloj: mientras rebota
  // nunca llega a cumplir estable_ms_.
  if (crudo != lectura_previa_) {
    lectura_previa_ = crudo;
    cambio_ms_ = ahora_ms;
  }

  // La resta sin signo es correcta aunque millis() desborde.
  if (crudo != estado_ && (ahora_ms - cambio_ms_) >= estable_ms_) {
    estado_ = crudo;
    if (estado_) {
      hubo_presion_ = true;
    } else {
      hubo_suelta_ = true;
    }
  }
}

bool Antirrebote::sePresiono() {
  const bool resultado = hubo_presion_;
  hubo_presion_ = false;
  return resultado;
}

bool Antirrebote::seSolto() {
  const bool resultado = hubo_suelta_;
  hubo_suelta_ = false;
  return resultado;
}
