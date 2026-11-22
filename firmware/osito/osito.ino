// Osito: gateway y director musical. Recibe a los animales por ESP-NOW, decide qué suena
// y (cuando esté configurado) reporta al broker. Aquí solo se llaman los módulos.
#include <esp_random.h>
#include <protocolo.h>

#include "animales.h"
#include "config.h"
#include "director.h"
#include "entradas.h"
#include "estado_animales.h"
#include "luces.h"
#include "simulacion.h"
#include "sintetizador_registro.h"

namespace {
// La ruta de audio todavía no se elige: cambiar solo esta línea cuando se confirme.
SintetizadorRegistro sintetizador;

void anunciarModo() {
  const protocolo::Modo modo = director::modo();
  luces::mostrarModo(modo);
  animales::avisarModo(modo);
  Serial.printf("[osito] modo=%s cancion=%s\n", protocolo::NOMBRE_MODO[protocolo::aEntero(modo)],
                director::cancion());
}
}  // namespace

void setup() {
  Serial.begin(config::BAUDIOS);
  Serial.println("[osito] Arrancando");
  entradas::iniciar();
  luces::iniciar();
  sintetizador.iniciar();
  animales::iniciar();
  director::iniciar(sintetizador, esp_random(), millis());
  director::volumen(entradas::volumen());
  simulacion::iniciar();
  anunciarModo();
}

void loop() {
  simulacion::actualizar();
  entradas::actualizar();
  animales::actualizar();

  const uint32_t ahora_ms = millis();
  animales::Recibido recibido;
  while (animales::siguiente(recibido)) {
    director::alEvento(recibido.evento, ahora_ms);
  }
  estado_animales::CambioConexion cambio;
  while (animales::siguienteCambio(cambio)) {
    if (!cambio.conectado) {
      director::avisoDesconexion();
    }
  }

  if (entradas::sePresionoModo()) {
    director::siguienteModo(ahora_ms);
    anunciarModo();
  }
  if (entradas::huboCambioVolumen()) {
    director::volumen(entradas::volumen());
  }

  director::actualizar(ahora_ms);
  sintetizador.actualizar();

  if (director::huboPulso()) {
    luces::pulso();
  }
  protocolo::PayloadLuz luz;
  if (director::sacarLuz(luz)) {
    animales::pedirLuz(luz);
  }
  simon::Resultado resultado;
  if (director::sacarResultado(resultado)) {
    Serial.printf("[simon] %s (aciertos=%u errores=%u)\n",
                  simon::NOMBRE_RESULTADO[protocolo::aEntero(resultado)], simon::aciertos(),
                  simon::errores());
  }

  luces::alerta(estado_animales::algunoSeDesconecto(ahora_ms));
  luces::actualizar();
}
