// Al arrancar toma la luz del cuarto como "destapada" en lugar de usar un umbral
// fijo, así funciona igual en el salón que en casa.
#include <Arduino.h>
#include <protocolo.h>

#include "config.h"
#include "pines.h"
#include "sensor_sombra.h"

namespace sensor_sombra {
namespace {
enum class Estado : uint8_t { Calibrando, Listo };

Estado estado = Estado::Calibrando;
uint32_t inicio_calibracion_ms = 0;
uint32_t suma_calibracion = 0;
uint32_t lecturas_calibracion = 0;
uint16_t luz_ambiente = 0;

uint32_t ultima_lectura_ms = 0;
uint8_t nivel_actual = 0;

uint8_t nivel_reportado = 0;
uint32_t ultimo_reporte_ms = 0;
bool hay_cambio = false;

uint8_t calcularNivel(uint16_t luz) {
  const uint16_t luz_tapada = luz_ambiente * config::LUZ_TAPADA_PCT / 100;
  // Estas dos comparaciones van antes del map() para que nunca divida entre 0.
  if (luz >= luz_ambiente) {
    return 0;
  }
  if (luz <= luz_tapada) {
    return protocolo::NIVEL_SOMBRA_MAX;
  }
  const long nivel = map(luz, luz_ambiente, luz_tapada, 0, protocolo::NIVEL_SOMBRA_MAX);
  return nivel < config::ZONA_MUERTA_NIVEL ? 0 : static_cast<uint8_t>(nivel);
}

void terminarCalibracion() {
  luz_ambiente = suma_calibracion / lecturas_calibracion;
  estado = Estado::Listo;
  Serial.printf("[sombra] Calibrada. Luz ambiente = %u\n", luz_ambiente);
  if (luz_ambiente < config::LUZ_AMBIENTE_MIN) {
    Serial.println("[sombra] Aviso: hay muy poca luz, la sombra casi no se va a notar");
  }
}

void revisarCambio(uint32_t ahora_ms) {
  if (ahora_ms - ultimo_reporte_ms < config::REPORTE_MIN_CADA_MS) {
    return;
  }
  const int diferencia = abs(static_cast<int>(nivel_actual) - nivel_reportado);
  // El 0 siempre se reporta: el osito necesita saber que la mano se fue.
  const bool se_destapo = nivel_actual == 0 && nivel_reportado != 0;
  const bool sigue_tapada =
      nivel_actual > 0 && ahora_ms - ultimo_reporte_ms >= config::REPETIR_TAPADA_CADA_MS;

  if (diferencia >= config::CAMBIO_MINIMO_NIVEL || se_destapo || sigue_tapada) {
    nivel_reportado = nivel_actual;
    ultimo_reporte_ms = ahora_ms;
    hay_cambio = true;
  }
}
}  // namespace

void iniciar() {
  inicio_calibracion_ms = millis();
  Serial.println("[sombra] Calibrando, no tapes la ranita...");
}

void actualizar() {
  const uint32_t ahora_ms = millis();
  if (ahora_ms - ultima_lectura_ms < config::LECTURA_CADA_MS) {
    return;
  }
  ultima_lectura_ms = ahora_ms;
  const uint16_t luz = analogRead(pines::LDR);  // 0 = oscuro, 4095 = mucha luz

  switch (estado) {
    case Estado::Calibrando:
      suma_calibracion += luz;
      ++lecturas_calibracion;
      if (ahora_ms - inicio_calibracion_ms >= config::CALIBRACION_MS) {
        terminarCalibracion();
      }
      break;

    case Estado::Listo:
      nivel_actual = calcularNivel(luz);
      revisarCambio(ahora_ms);
      break;
  }
}

bool calibrando() {
  return estado == Estado::Calibrando;
}

uint8_t nivel() {
  return nivel_actual;
}

bool huboCambio() {
  const bool habia = hay_cambio;
  hay_cambio = false;
  return habia;
}
}  // namespace sensor_sombra
