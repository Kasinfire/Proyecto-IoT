#include "efectos_dj.h"

#include <iterator>

#include "config.h"

namespace {
constexpr uint8_t SIN_NOTA = 0;
// Subida: do pentatónica hacia arriba, dos octavas.
constexpr uint8_t NOTAS_SUBIDA[] = {60, 62, 64, 67, 69, 72, 74, 76,
                                    79, 81, 84, 86, 88, 91, 93, 96};
constexpr size_t PASOS_SUBIDA = std::size(NOTAS_SUBIDA);

// De `desde` a `hasta` según el avance (0..1000).
uint32_t mezclar(uint32_t desde, uint32_t hasta, uint32_t avance_milesimas) {
  return desde + (static_cast<int32_t>(hasta) - static_cast<int32_t>(desde)) *
                     static_cast<int32_t>(avance_milesimas) / 1000;
}
}  // namespace

void EfectosDj::conectar(Sintetizador& salida) {
  salida_ = &salida;
}

void EfectosDj::reiniciar() {
  eco_ = false;
  pausado_ = false;
  num_ecos_ = 0;
  num_cortas_ = 0;
  hay_redoble_ = false;
  hay_subida_ = false;
}

void EfectosDj::notaOn(uint8_t canal, uint8_t nota_midi, uint8_t velocidad) {
  if (canal >= CANALES || nota_midi >= NOTAS) {
    return;
  }
  salida_->notaOn(canal, nota_midi, velocidad);

  if (eco_ && !pausado_ && num_ecos_ < ECO_MAX) {
    const uint8_t eco_vel = velocidad * config::ECO_QUEDA_PCT / 100;
    if (eco_vel > 0) {
      ecos_[num_ecos_++] = {canal, nota_midi, eco_vel, config::ECO_CADA_PULSOS,
                            config::ECO_REPETICIONES};
    }
  }
}

void EfectosDj::notaOff(uint8_t canal, uint8_t nota_midi) {
  salida_->notaOff(canal, nota_midi);
}

void EfectosDj::expresion(uint8_t canal, uint8_t valor) {
  salida_->expresion(canal, valor);
}

void EfectosDj::volumen(uint8_t valor) {
  salida_->volumen(valor);
}

void EfectosDj::sonarCorta(uint8_t canal, uint8_t nota, uint8_t velocidad) {
  salida_->notaOn(canal, nota, velocidad);
  if (num_cortas_ < CORTAS_MAX) {
    cortas_[num_cortas_++] = {canal, nota};
  }
}

void EfectosDj::alPulso() {
  for (size_t i = 0; i < num_cortas_; ++i) {
    salida_->notaOff(cortas_[i].canal, cortas_[i].nota);
  }
  num_cortas_ = 0;

  size_t quedan = 0;
  for (size_t i = 0; i < num_ecos_; ++i) {
    Eco eco = ecos_[i];
    if (--eco.pulsos_para_sonar == 0) {
      sonarCorta(eco.canal, eco.nota, eco.velocidad);
      --eco.repeticiones_quedan;
      eco.velocidad = eco.velocidad * config::ECO_QUEDA_PCT / 100;
      eco.pulsos_para_sonar = config::ECO_CADA_PULSOS;
    }
    if (eco.repeticiones_quedan > 0 && eco.velocidad > 0) {
      ecos_[quedan++] = eco;
    }
  }
  num_ecos_ = quedan;
}

void EfectosDj::avanzar(uint32_t ahora_ms) {
  actualizarRedoble(ahora_ms);
  actualizarSubida(ahora_ms);
}

bool EfectosDj::alternarEco() {
  eco_ = !eco_;
  if (!eco_) {
    num_ecos_ = 0;
  }
  return eco_;
}

void EfectosDj::pausar(bool pausado) {
  pausado_ = pausado;
  if (!pausado) {
    return;
  }
  num_ecos_ = 0;
  hay_redoble_ = false;
  if (hay_subida_ && subida_sonando_ != SIN_NOTA) {
    salida_->notaOff(config::CANAL_DJ_MELODIA, subida_sonando_);
  }
  hay_subida_ = false;
}

void EfectosDj::redoble(uint32_t ahora_ms, uint32_t pulso_ms) {
  hay_redoble_ = true;
  redoble_inicio_ms_ = ahora_ms;
  redoble_dura_ms_ = pulso_ms * config::REDOBLE_PULSOS;
  redoble_siguiente_ms_ = ahora_ms;
}

// Golpes de tambor cada vez más seguidos y más fuertes; al final, platillo.
void EfectosDj::actualizarRedoble(uint32_t ahora_ms) {
  if (!hay_redoble_ || ahora_ms - redoble_siguiente_ms_ > UINT32_MAX / 2) {
    return;  // todavía no toca el siguiente golpe
  }
  const uint32_t pasado = ahora_ms - redoble_inicio_ms_;
  if (pasado >= redoble_dura_ms_) {
    hay_redoble_ = false;
    sonarCorta(config::CANAL_DJ_PERCUSION, config::NOTA_PLATILLO_CHOQUE, 120);
    return;
  }
  const uint32_t avance = pasado * 1000 / redoble_dura_ms_;
  const uint32_t pulso = redoble_dura_ms_ / config::REDOBLE_PULSOS;
  sonarCorta(config::CANAL_DJ_PERCUSION, config::NOTA_TAMBOR,
             static_cast<uint8_t>(mezclar(50, 120, avance)));
  redoble_siguiente_ms_ = ahora_ms + mezclar(pulso / 2, pulso / 6, avance);
}

void EfectosDj::subida(uint32_t ahora_ms, uint32_t pulso_ms) {
  if (hay_subida_ && subida_sonando_ != SIN_NOTA) {
    salida_->notaOff(config::CANAL_DJ_MELODIA, subida_sonando_);
  }
  hay_subida_ = true;
  subida_inicio_ms_ = ahora_ms;
  subida_dura_ms_ = pulso_ms * config::SUBIDA_PULSOS;
  subida_paso_ = 0;
  subida_sonando_ = SIN_NOTA;
}

// Notas que suben y suben; al final, la caída: golpe grave con platillo.
void EfectosDj::actualizarSubida(uint32_t ahora_ms) {
  if (!hay_subida_) {
    return;
  }
  const uint32_t pasado = ahora_ms - subida_inicio_ms_;
  if (pasado >= subida_dura_ms_) {
    hay_subida_ = false;
    if (subida_sonando_ != SIN_NOTA) {
      salida_->notaOff(config::CANAL_DJ_MELODIA, subida_sonando_);
    }
    sonarCorta(config::CANAL_DJ_PERCUSION, config::NOTA_PLATILLO_CHOQUE, 127);
    sonarCorta(config::CANAL_DJ_MELODIA, config::NOTA_CAIDA, 127);
    return;
  }
  const size_t toca = pasado * PASOS_SUBIDA / subida_dura_ms_;
  if (toca < subida_paso_) {
    return;
  }
  if (subida_sonando_ != SIN_NOTA) {
    salida_->notaOff(config::CANAL_DJ_MELODIA, subida_sonando_);
  }
  subida_sonando_ = NOTAS_SUBIDA[toca];
  salida_->notaOn(config::CANAL_DJ_MELODIA, subida_sonando_,
                  static_cast<uint8_t>(mezclar(40, 120, pasado * 1000 / subida_dura_ms_)));
  subida_paso_ = toca + 1;
}
