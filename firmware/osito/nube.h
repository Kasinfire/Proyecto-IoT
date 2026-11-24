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
// simulado: el pedido llegó por "sim mqtt"; su acuse solo se imprime.
void responderModo(int32_t id, bool ok, protocolo::Modo modo, bool simulado);
// Eventos del propio osito, por la cola con acuse. simulado: nació de "sim ..." y no va al
// broker real.
void alModo(protocolo::Modo modo, const char* cancion, contrato_mqtt::Origen origen,
            bool simulado);
// resultado = Resultado::Nada para el pedido; luego otro con el resultado.
void alSimon(protocolo::Nodo pedido, simon::Resultado resultado, uint8_t nivel, bool simulado);
// Volumen del potenciómetro (retenido, a lo más cada VOLUMEN_CADA_MS).
void alVolumen(uint8_t valor);
// Pedido de orquesta/control/aviso: true una sola vez por pedido.
bool sacarAviso(bool& silenciar);
// Para "sim mqtt modo/ack": entra como si viniera del broker, marcado como SIMULACIÓN.
void inyectarSimulado(const char* topico, const char* payload);
}  // namespace nube
