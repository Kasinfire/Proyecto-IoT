// Lo único que el director musical sabe del audio. Cada ruta (DAC + PAM8403, serial a la
// laptop o SAM2695) será otra implementación de esta clase, sin tocar la lógica de los modos.
// Mientras no se confirme una ruta, el osito usa SintetizadorRegistro (solo imprime).
#pragma once

#include <cstdint>

class Sintetizador {
 public:
  virtual ~Sintetizador() = default;
  virtual void iniciar() = 0;
  virtual void actualizar() {}

  // canal 1..16 como en MIDI; nota_midi 0..127; velocidad 1..127.
  virtual void notaOn(uint8_t canal, uint8_t nota_midi, uint8_t velocidad) = 0;
  virtual void notaOff(uint8_t canal, uint8_t nota_midi) = 0;
  // Volumen de una sola parte (0..127): con esto entran y salen con fade.
  virtual void expresion(uint8_t canal, uint8_t valor) = 0;
  virtual void volumen(uint8_t valor) = 0;  // general, del potenciómetro, 0..127
};
