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
#include "simon.h"

namespace contrato_mqtt {
constexpr char TOPICO_MODO[] = "orquesta/control/modo";
constexpr char TOPICO_MODO_ACK[] = "orquesta/control/modo/ack";
constexpr char TOPICO_SYNC_ACK[] = "orquesta/sync/ack";
// Node-RED -> osito: {"silenciar":true|false}. Calla el sonido de desconexión (el LED sigue).
constexpr char TOPICO_AVISO[] = "orquesta/control/aviso";
// Osito -> Node-RED, retenidos (estado, no historial): {"valor":0..127} y
// {"pendientes":N,"descartados":D} del buffer de eventos.
// Canción creada en la página, por trozos (ver cancion_recibida.h), y su acuse:
//   {"carga":5,"nombre":"m-ab12","pulso":250}
//   {"carga":5,"parte":2,"desde":0,"pasos":"60.2.0,62.2.0"}
//   {"carga":5,"totales":"8,8,20,6"}          -> {"carga":5,"ok":true}
constexpr char TOPICO_CANCION[] = "orquesta/control/cancion";
constexpr char TOPICO_CANCION_ACK[] = "orquesta/control/cancion/ack";
// Panel DJ: {"tecla":"8"} en los dos sentidos, y el estado (retenido).
constexpr char TOPICO_TECLA_REMOTA[] = "orquesta/control/tecla";
constexpr char TOPICO_TECLA[] = "orquesta/osito/tecla";
constexpr char TOPICO_PANEL[] = "orquesta/osito/panel";
constexpr char TOPICO_VOLUMEN[] = "orquesta/osito/volumen";
constexpr char TOPICO_COLA[] = "orquesta/osito/cola";
constexpr size_t CANCION_MAX = 24;

// orquesta/{nodo}/evento y orquesta/{nodo}/estado
bool topicoEvento(protocolo::Nodo nodo, char* destino, size_t tamano);
bool topicoEstado(protocolo::Nodo nodo, char* destino, size_t tamano);

// {"seq":1532,"t":1762900000,"nodo":"gatito","tipo":"nota","nota":"mi","vel":90}
// t = 0 si todavía no hay hora de NTP.
bool armarEvento(uint32_t seq, uint32_t t, const interprete::Evento& evento, char* destino,
                 size_t tamano);
bool armarEstado(bool en_linea, char* destino, size_t tamano);

// Eventos del propio osito (van por la cola con acuse a orquesta/osito/evento):
// {"seq":..,"t":..,"nodo":"osito","tipo":"modo","modo":"libre","cancion":"prueba","origen":"teclado"}
enum class Origen : uint8_t { Arranque, Boton, Teclado, NodeRed };
bool armarEventoModo(uint32_t seq, uint32_t t, protocolo::Modo modo, const char* cancion,
                     Origen origen, char* destino, size_t tamano);
// Pedido: {"seq":..,"t":..,"nodo":"osito","tipo":"simon","pedido":"ranita","nivel":2}
// Resultado: igual + "resultado":"acierto|error|tiempo". Resultado::Nada = es el pedido.
bool armarEventoSimon(uint32_t seq, uint32_t t, protocolo::Nodo pedido, simon::Resultado resultado,
                      uint8_t nivel, char* destino, size_t tamano);
// Agrega "arranque":<n> al final de un evento: seq vuelve a 1 en cada encendido, y con esto
// Node-RED distingue un evento nuevo de uno repetido.
bool agregarArranque(char* json, size_t tamano, uint32_t arranque);
bool armarVolumen(uint8_t valor, char* destino, size_t tamano);
bool armarCola(size_t pendientes, uint32_t descartados, char* destino, size_t tamano);
// Lo que cabe en un mensaje de TOPICO_CANCION.
constexpr size_t PASOS_TEXTO_MAX = 200;
struct TrozoCancion {
  enum class Tipo : uint8_t { Inicio, Pasos, Fin } tipo;
  int32_t carga;
  char nombre[CANCION_MAX];
  uint32_t pulso_ms;
  uint32_t parte;
  uint32_t desde;
  char pasos[PASOS_TEXTO_MAX];
  uint32_t totales[protocolo::NUM_NODOS];
};
bool leerTrozoCancion(const char* json, TrozoCancion& trozo);
bool armarAckCancion(int32_t carga, bool ok, char* destino, size_t tamano);
// Solo las 16 teclas y 'd' (D mantenida).
bool leerTecla(const char* json, char& tecla);
bool armarTecla(char tecla, char* destino, size_t tamano);
// {"silenciar":true|false}; false si no lo trae.
bool leerAviso(const char* json, bool& silenciar);

struct PedidoModo {
  int32_t id;
  bool modo_valido;
  protocolo::Modo modo;
  bool trae_cancion;
  char cancion[CANCION_MAX];
  bool simulado;  // llegó por "sim mqtt": su acuse no va al broker real
};
// {"id":<n>,"modo":"cancion|libre|simon","cancion":"estrellita"}. false si no trae "id":
// sin id no hay a quién responder.
bool leerPedidoModo(const char* json, PedidoModo& pedido);
// {"id":<n>,"ok":true,"modo":"cancion"}
bool armarAckModo(int32_t id, bool ok, protocolo::Modo modo, char* destino, size_t tamano);

// {"hasta_seq":<n>}
// {"arranque":A,"hasta_seq":N}. Un acuse viejo sin arranque deja trae_arranque en false.
bool leerSyncAck(const char* json, uint32_t& hasta_seq, uint32_t& arranque, bool& trae_arranque);
}  // namespace contrato_mqtt
