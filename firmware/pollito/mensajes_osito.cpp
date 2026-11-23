// El osito manda la luz de Simón Dice a todos con la clave de grupo; cada animal
// se queda solo con las que traen su nodo.
#include "mensajes_osito.h"

#include <Arduino.h>
#include <enlace.h>
#include <protocolo.h>

#include "led_estado.h"

namespace mensajes_osito {
namespace {
constexpr uint8_t UNICO_LED = 0;

void alRecibirLuz(const protocolo::PayloadLuz& luz) {
  const bool es_para_mi = luz.nodo_destino == protocolo::aEntero(protocolo::Nodo::Pollito);
  const bool led_valido = luz.led == UNICO_LED || luz.led == protocolo::LED_TODOS;
  if (!es_para_mi || !led_valido) {
    return;
  }
  Serial.printf("[osito] Simón Dice: luz por %u ms\n", luz.duracion_ms);
  led_estado::encenderPor(luz.duracion_ms);
}
}  // namespace

void actualizar() {
  enlace::Mensaje mensaje;
  while (enlace::recibir(mensaje)) {
    protocolo::PayloadLuz luz;
    protocolo::PayloadModo modo;
    if (mensaje.leer(luz)) {
      alRecibirLuz(luz);
    } else if (mensaje.leer(modo) && modo.modo < protocolo::NUM_MODOS) {
      Serial.printf("[osito] Modo %s\n", protocolo::NOMBRE_MODO[modo.modo]);
    }
  }
}
}  // namespace mensajes_osito
