// Tópicos y JSON de MQTT según la sección 6 de orquesta-proyecto.md. No usa Arduino.
// Solo entiende objetos JSON planos como los de la especificación (sin anidar ni arreglos).
//
// Esto lo tengo que acordar con quien arme Node-RED:
//   - Campos de sombra y agitar: "nivel" e "intensidad" (la especificación solo trae el de nota).
//   - Estado de un animal: {"estado":"online"} / {"estado":"offline"}, sin RSSI: la librería
//     no lo expone y no se cambia aquí.
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>

#include "interprete.h"

namespace contrato_mqtt {
constexpr char TOPICO_MODO[] = "orquesta/control/modo";
constexpr char TOPICO_MODO_ACK[] = "orquesta/control/modo/ack";
constexpr char TOPICO_SYNC_ACK[] = "orquesta/sync/ack";
constexpr size_t CANCION_MAX = 24;

// orquesta/{nodo}/evento y orquesta/{nodo}/estado
bool topicoEvento(protocolo::Nodo nodo, char* destino, size_t tamano);
bool topicoEstado(protocolo::Nodo nodo, char* destino, size_t tamano);

// {"seq":1532,"t":1762900000,"nodo":"gatito","tipo":"nota","nota":"mi","vel":90}
// t = 0 si todavía no hay hora de NTP.
bool armarEvento(uint32_t seq, uint32_t t, const interprete::Evento& evento, char* destino,
                 size_t tamano);
bool armarEstado(bool en_linea, char* destino, size_t tamano);

struct PedidoModo {
  int32_t id;
  bool modo_valido;
  protocolo::Modo modo;
  bool trae_cancion;
  char cancion[CANCION_MAX];
};
// {"id":<n>,"modo":"cancion|libre|simon","cancion":"estrellita"}. false si no trae "id":
// sin id no hay a quién responder.
bool leerPedidoModo(const char* json, PedidoModo& pedido);
// {"id":<n>,"ok":true,"modo":"cancion"}
bool armarAckModo(int32_t id, bool ok, protocolo::Modo modo, char* destino, size_t tamano);

// {"hasta_seq":<n>}
bool leerSyncAck(const char* json, uint32_t& hasta_seq);
}  // namespace contrato_mqtt
