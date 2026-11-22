// Osito: gateway y director musical. Recibe a los animales por ESP-NOW, decide qué suena
// y (cuando esté configurado) reporta al broker. Aquí solo se llaman los módulos.
#include <esp_random.h>
#include <protocolo.h>

#include "animales.h"
#include "canciones.h"
#include "config.h"
#include "director.h"
#include "entradas.h"
#include "estado_animales.h"
#include "luces.h"
#include "nube.h"
#include "simulacion.h"
#include "sintetizador_registro.h"
#include "transporte_simulado.h"

namespace {
// La ruta de audio todavía no se elige: cambiar solo esta línea cuando se confirme.
SintetizadorRegistro sintetizador;
// Falta la información para el broker real (ver transporte_mqtt.h): por ahora, simulado.
TransporteSimulado broker;

void anunciarModo() {
  const protocolo::Modo modo = director::modo();
  luces::mostrarModo(modo);
  animales::avisarModo(modo);
  Serial.printf("[osito] modo=%s cancion=%s\n", protocolo::NOMBRE_MODO[protocolo::aEntero(modo)],
                director::cancion());
}

// Request/Response: el dashboard sabe que el cambio de verdad ocurrió solo con este ack.
void atenderPedidosModo(uint32_t ahora_ms) {
  contrato_mqtt::PedidoModo pedido;
  while (nube::sacarPedidoModo(pedido)) {
    const bool cancion_existe = !pedido.trae_cancion || canciones::buscar(pedido.cancion);
    const bool ok = pedido.modo_valido && cancion_existe;
    if (ok) {
      if (pedido.trae_cancion) {
        director::elegirCancion(pedido.cancion);
      }
      director::cambiarModo(pedido.modo, ahora_ms);
      anunciarModo();
    } else {
      Serial.printf("[osito] Pedido de modo %ld rechazado (%s)\n", static_cast<long>(pedido.id),
                    pedido.modo_valido ? "esa canción no existe" : "modo desconocido");
    }
    nube::responderModo(pedido.id, ok, director::modo());
  }
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
  nube::iniciar(broker);
  simulacion::iniciar(broker);
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
    nube::alEvento(recibido);
  }
  estado_animales::CambioConexion cambio;
  while (animales::siguienteCambio(cambio)) {
    nube::alCambio(cambio);
    if (!cambio.conectado) {
      director::avisoDesconexion();
    }
  }

  if (entradas::sePresionoModo()) {
    director::siguienteModo(ahora_ms);
    anunciarModo();
  }
  atenderPedidosModo(ahora_ms);
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

  nube::actualizar();
  luces::alerta(estado_animales::algunoSeDesconecto(ahora_ms));
  luces::actualizar();
}
