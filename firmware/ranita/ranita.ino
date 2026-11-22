// Ranita: acompañamiento (violín). La sombra de la mano sobre la LDR controla
// su parte de la canción, como un theremín, y se la avisa al osito.
#include <protocolo.h>

#include "config.h"
#include "enlace.h"
#include "mejillas.h"
#include "sensor_sombra.h"

void setup() {
  Serial.begin(config::BAUDIOS);
  sensor_sombra::iniciar();
  mejillas::iniciar();
  enlace::iniciar();
}

void loop() {
  sensor_sombra::actualizar();
  mejillas::actualizar();
  enlace::actualizar();

  if (sensor_sombra::huboCambio()) {
    enlace::enviar(protocolo::PayloadSombra{sensor_sombra::nivel()});
  }
}
