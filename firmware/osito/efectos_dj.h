// Efectos del teclado DJ. Se pone entre el director y el sintetizador real: todas las
// notas (en vivo, del loop y de la canción) pasan por aquí. No usa Arduino.
//   Eco:    cada nota se repite ECO_REPETICIONES veces, cada vez más bajito.
//   Redoble, subida y caída: los toca el DJ en sus propios canales (CANAL_DJ_*), que el
//   fade de Canción no toca.
#pragma once

#include <cstddef>
#include <cstdint>

#include "sintetizador.h"

class EfectosDj : public Sintetizador {
 public:
  void conectar(Sintetizador& salida);
  void reiniciar();

  void iniciar() override {}
  void notaOn(uint8_t canal, uint8_t nota_midi, uint8_t velocidad) override;
  void notaOff(uint8_t canal, uint8_t nota_midi) override;
  void expresion(uint8_t canal, uint8_t valor) override;
  void volumen(uint8_t valor) override;

  // En cada pulso del director: suelta las notas cortas y toca los ecos que tocan.
  void alPulso();
  // Lleva el redoble y la subida, que van más rápido que el pulso.
  void avanzar(uint32_t ahora_ms);

  bool alternarEco();     // true si quedó encendido
  void redoble(uint32_t ahora_ms, uint32_t pulso_ms);
  void subida(uint32_t ahora_ms, uint32_t pulso_ms);
  // En Simón: ni eco ni efectos, para que solo se oiga el juego. Al volver sigue el eco.
  void pausar(bool pausado);

 private:
  static constexpr size_t CANALES = 17;  // índice = canal MIDI 1..16
  static constexpr size_t NOTAS = 128;
  static constexpr size_t ECO_MAX = 48;
  static constexpr size_t CORTAS_MAX = 64;

  struct Eco {
    uint8_t canal;
    uint8_t nota;
    uint8_t velocidad;
    uint8_t pulsos_para_sonar;
    uint8_t repeticiones_quedan;
  };
  struct Corta {
    uint8_t canal;
    uint8_t nota;
  };

  void sonarCorta(uint8_t canal, uint8_t nota, uint8_t velocidad);
  void actualizarRedoble(uint32_t ahora_ms);
  void actualizarSubida(uint32_t ahora_ms);

  Sintetizador* salida_ = nullptr;
  bool eco_ = false;
  bool pausado_ = false;
  Eco ecos_[ECO_MAX] = {};
  size_t num_ecos_ = 0;
  Corta cortas_[CORTAS_MAX] = {};
  size_t num_cortas_ = 0;

  bool hay_redoble_ = false;
  uint32_t redoble_inicio_ms_ = 0;
  uint32_t redoble_dura_ms_ = 0;
  uint32_t redoble_siguiente_ms_ = 0;

  bool hay_subida_ = false;
  uint32_t subida_inicio_ms_ = 0;
  uint32_t subida_dura_ms_ = 0;
  size_t subida_paso_ = 0;
  uint8_t subida_sonando_ = 0;
};
