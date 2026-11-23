// Ranita: acompañamiento (violín). La sombra de la mano sobre la LDR controla
// su parte de la canción, como un theremín, y se la avisa al osito.
#include <enlace.h>
#include <protocolo.h>

#include "config.h"
#include "mejillas.h"
#include "mensajes_osito.h"
#include "sensor_sombra.h"

void setup() {
  Serial.begin(config::BAUDIOS);
  // Primero el enlace: prender el Wi-Fi tarda y no debe comerse el tiempo de calibración.
  enlace::iniciar(protocolo::Nodo::Ranita);
  sensor_sombra::iniciar();
  mejillas::iniciar();
}

void loop() {
  sensor_sombra::actualizar();
  mensajes_osito::actualizar();
  mejillas::actualizar();
  enlace::actualizar();

  if (sensor_sombra::huboCambio()) {
    enlace::enviar(protocolo::PayloadSombra{sensor_sombra::nivel()});
  }
}
