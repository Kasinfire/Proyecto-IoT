// Recibe a la ranita, el gatito y el pollito: lo que llega por ESP-NOW y lo que inyecta la
// simulación pasan por el mismo camino, pero siempre etiquetados con su fuente.
// También manda el latido del osito, que la librería no manda.
#pragma once

#include <enlace.h>

#include "estado_animales.h"
#include "interprete.h"

namespace animales {
struct Recibido {
  interprete::Evento evento;
  estado_animales::Fuente fuente;
};

// En el osito, llama a iniciar() después de conectar al Wi-Fi para usar el mismo canal.
void iniciar();
void actualizar();

bool radioLista();

// Lo usa el módulo de simulación; nunca se marca como Fuente::Radio.
void inyectarSimulado(const enlace::Mensaje& mensaje);

// Gestos ya validados, en orden de llegada (sin latidos).
bool siguiente(Recibido& recibido);
// Conexiones y desconexiones, para el aviso sonoro y para MQTT.
bool siguienteCambio(estado_animales::CambioConexion& cambio);

// Osito -> animales, por difusión con la clave de grupo. false si la radio está apagada.
bool avisarModo(protocolo::Modo modo);
bool pedirLuz(const protocolo::PayloadLuz& luz);
}  // namespace animales
