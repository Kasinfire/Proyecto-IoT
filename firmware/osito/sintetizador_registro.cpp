#include "sintetizador_registro.h"

#include <Arduino.h>

#include "config.h"

void SintetizadorRegistro::iniciar() {
  Serial.println("[AUDIO-REGISTRO] Sin ruta de audio elegida: solo se imprimen las notas");
}

void SintetizadorRegistro::notaOn(uint8_t canal, uint8_t nota_midi, uint8_t velocidad) {
  if (config::IMPRIMIR_AUDIO) {
    Serial.printf("[AUDIO-REGISTRO] ON %u %u %u\n", canal, nota_midi, velocidad);
  }
}

void SintetizadorRegistro::notaOff(uint8_t canal, uint8_t nota_midi) {
  if (config::IMPRIMIR_AUDIO) {
    Serial.printf("[AUDIO-REGISTRO] OFF %u %u\n", canal, nota_midi);
  }
}

void SintetizadorRegistro::expresion(uint8_t canal, uint8_t valor) {
  if (config::IMPRIMIR_AUDIO) {
    Serial.printf("[AUDIO-REGISTRO] EXP %u %u\n", canal, valor);
  }
}

void SintetizadorRegistro::volumen(uint8_t valor) {
  Serial.printf("[AUDIO-REGISTRO] VOL %u\n", valor);
}
