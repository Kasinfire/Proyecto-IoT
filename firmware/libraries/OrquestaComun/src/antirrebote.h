// Antirrebote de botones y sensores digitales: reporta un solo cambio aunque
// el contacto rebote varias veces. Lo usan gatito, pollito y osito.

#pragma once

#include <Arduino.h>

#include <cstdint>

class Antirrebote {
 public:
  // estable_ms viene del config.h de cada animal.
  Antirrebote(uint8_t pin, uint32_t estable_ms, bool activo_en_bajo);

  // Configura el pin (INPUT_PULLUP si activo_en_bajo). Llamar en setup().
  void iniciar();

  // Llamar en cada loop(), sin delay().
  void actualizar();

  bool presionado() const { return estado_; }

  // true una sola vez por cada flanco. Si pasan dos pulsaciones antes de
  // consultar, se cuenta una: hay que llamar en cada loop().
  bool sePresiono();
  bool seSolto();

 private:
  bool leerCrudo() const;

  uint8_t pin_;
  uint32_t estable_ms_;
  bool activo_en_bajo_;

  bool lectura_previa_ = false;  // última lectura cruda, aún con posible rebote
  bool estado_ = false;          // estado ya filtrado
  uint32_t cambio_ms_ = 0;       // cuándo cambió por última vez la lectura cruda
  bool hubo_presion_ = false;
  bool hubo_suelta_ = false;
};
