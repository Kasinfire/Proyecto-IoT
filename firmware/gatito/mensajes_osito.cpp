// El osito manda la luz de Simón Dice a todos con la clave de grupo; cada animal
// se queda solo con las que traen su nodo.
#include "mensajes_osito.h"

#include <Arduino.h>
#include <enlace.h>
#include <protocolo.h>

#include "luces.h"

namespace mensajes_osito {
namespace {
void alRecibirLuz(const protocolo::PayloadLuz& luz) {
  const bool es_para_mi = luz.nodo_destino == protocolo::aEntero(protocolo::Nodo::Gatito);
  const bool led_valido = luz.led < protocolo::NUM_NOTAS || luz.led == protocolo::LED_TODOS;
  if (!es_para_mi || !led_valido) {
    return;
  }
  Serial.printf("[osito] Simón Dice: luz led=%u por %u ms\n", luz.led, luz.duracion_ms);
  luces::pedirLuz(luz.led, luz.duracion_ms);
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
