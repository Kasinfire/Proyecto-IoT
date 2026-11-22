// Puente entre el osito y el broker: arma eventos y estados, los publica en orden cuando hay
// conexión y solo borra un evento cuando Node-RED lo confirma por orquesta/sync/ack.
// La música no depende de esto: si no hay broker, todo sigue sonando y los eventos esperan.
#pragma once

#include <protocolo.h>

#include "animales.h"
#include "contrato_mqtt.h"
#include "estado_animales.h"
#include "transporte_mqtt.h"

namespace nube {
void iniciar(TransporteMqtt& transporte);
void actualizar();

void alEvento(const animales::Recibido& recibido);
void alCambio(const estado_animales::CambioConexion& cambio);

// Pedidos de orquesta/control/modo. Quien lo aplica responde con responderModo().
bool sacarPedidoModo(contrato_mqtt::PedidoModo& pedido);
void responderModo(int32_t id, bool ok, protocolo::Modo modo);
}  // namespace nube
