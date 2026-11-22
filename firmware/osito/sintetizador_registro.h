// Sintetizador que no suena: imprime por Serial lo que tocaría. Sirve para probar los modos
// sin haber elegido la ruta de audio. Sus líneas empiezan con [AUDIO-REGISTRO].
#pragma once

#include "sintetizador.h"

class SintetizadorRegistro : public Sintetizador {
 public:
  void iniciar() override;
  void notaOn(uint8_t canal, uint8_t nota_midi, uint8_t velocidad) override;
  void notaOff(uint8_t canal, uint8_t nota_midi) override;
  void expresion(uint8_t canal, uint8_t valor) override;
  void volumen(uint8_t valor) override;
};
