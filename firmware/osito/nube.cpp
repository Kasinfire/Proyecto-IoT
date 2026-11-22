#include "nube.h"

#include <Arduino.h>
#include <cola_circular.h>

#include <cstring>
#include <ctime>

#include "cola_eventos.h"
#include "config.h"

namespace nube {
namespace {
using estado_animales::Fuente;
using protocolo::Nodo;

TransporteMqtt* transporte = nullptr;
bool estaba_conectado = false;
uint32_t ultima_publicacion_ms = 0;
ColaCircular<contrato_mqtt::PedidoModo, config::PEDIDOS_MODO_MAX> pedidos;

bool estado_pendiente[protocolo::NUM_NODOS] = {};
bool estado_en_linea[protocolo::NUM_NODOS] = {};

uint8_t ultimo_nivel_sombra = 0;
uint32_t ultima_sombra_ms = 0;

// Un dato simulado no debe llegar nunca al dashboard real.
bool sePuedePublicar(Fuente fuente) {
  return fuente == Fuente::Radio || transporte->esSimulado();
}

uint32_t horaUnix() {
  const time_t ahora = time(nullptr);
  return ahora >= static_cast<time_t>(config::HORA_VALIDA_DESDE) ? static_cast<uint32_t>(ahora) : 0;
}

bool sombraVale(const interprete::Evento& evento, uint32_t ahora_ms) {
  const bool cambio_de_estado = (evento.valor == 0) != (ultimo_nivel_sombra == 0);
  if (!cambio_de_estado && ahora_ms - ultima_sombra_ms < config::SOMBRA_EVENTO_CADA_MS) {
    return false;
  }
  ultimo_nivel_sombra = evento.valor;
  ultima_sombra_ms = ahora_ms;
  return true;
}

void alMensaje(const char* topico, const char* payload) {
  if (strcmp(topico, contrato_mqtt::TOPICO_MODO) == 0) {
    contrato_mqtt::PedidoModo pedido;
    if (!contrato_mqtt::leerPedidoModo(payload, pedido)) {
      Serial.println("[nube] Pedido de modo sin \"id\": se ignora (no hay a quién responder)");
    } else if (!pedidos.meter(pedido)) {
      Serial.println("[nube] Demasiados pedidos de modo juntos: se ignora uno");
    }
  } else if (strcmp(topico, contrato_mqtt::TOPICO_SYNC_ACK) == 0) {
    uint32_t hasta_seq = 0;
    if (contrato_mqtt::leerSyncAck(payload, hasta_seq)) {
      const size_t borrados = cola_eventos::confirmarHasta(hasta_seq);
      Serial.printf("[nube] Acuse hasta_seq=%lu: borré %u, quedan %u\n",
                    static_cast<unsigned long>(hasta_seq), static_cast<unsigned>(borrados),
                    static_cast<unsigned>(cola_eventos::pendientes()));
    }
  }
}

void publicarEstados() {
  char topico[config::TOPICO_MAX];
  char json[config::EVENTO_JSON_MAX];
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    if (!estado_pendiente[i] || !contrato_mqtt::topicoEstado(static_cast<Nodo>(i), topico,
                                                             sizeof(topico))) {
      continue;
    }
    contrato_mqtt::armarEstado(estado_en_linea[i], json, sizeof(json));
    // Retenido: el dashboard ve el último estado aunque se conecte después.
    if (transporte->publicar(topico, json, true)) {
      estado_pendiente[i] = false;
    }
  }
}

void publicarEventos(uint32_t ahora_ms) {
  char topico[config::TOPICO_MAX];
  for (size_t n = 0; n < config::PUBLICAR_POR_VUELTA; ++n) {
    uint32_t seq = 0;
    uint8_t nodo = 0;
    const char* json = nullptr;
    if (!cola_eventos::siguientePorPublicar(seq, nodo, json) ||
        !contrato_mqtt::topicoEvento(static_cast<Nodo>(nodo), topico, sizeof(topico))) {
      break;
    }
    // TODO(equipo): cifrar `json` con AES-128-GCM y mandarlo en Base64 cuando se acuerden con
    // Node-RED la clave en NVS y qué va como AAD. Hoy solo sale por el transporte simulado.
    if (!transporte->publicar(topico, json, false)) {
      break;
    }
    cola_eventos::marcarPublicado(seq);
    ultima_publicacion_ms = ahora_ms;
  }
}
}  // namespace

void iniciar(TransporteMqtt& t) {
  transporte = &t;
  cola_eventos::reiniciar();
  transporte->iniciar(alMensaje);
}

void actualizar() {
  transporte->actualizar();
  const bool conectado = transporte->conectado();
  const uint32_t ahora_ms = millis();

  if (conectado && !estaba_conectado) {
    // Al reconectar se reenvía todo lo pendiente en orden; Node-RED descarta duplicados por seq.
    cola_eventos::republicarTodo();
    for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
      estado_pendiente[i] = static_cast<Nodo>(i) != Nodo::Osito;
    }
    Serial.printf("[nube] %s conectado: %u eventos pendientes\n", transporte->nombre(),
                  static_cast<unsigned>(cola_eventos::pendientes()));
  }
  estaba_conectado = conectado;
  if (!conectado) {
    return;
  }

  publicarEstados();
  uint32_t seq = 0;
  uint8_t nodo = 0;
  const char* json = nullptr;
  const bool todo_publicado = !cola_eventos::siguientePorPublicar(seq, nodo, json);
  if (todo_publicado && cola_eventos::pendientes() > 0 &&
      ahora_ms - ultima_publicacion_ms >= config::REPUBLICAR_SIN_ACK_MS) {
    Serial.println("[nube] No llegó el acuse: vuelvo a publicar lo pendiente");
    cola_eventos::republicarTodo();
  }
  publicarEventos(ahora_ms);
}

void alEvento(const animales::Recibido& recibido) {
  const interprete::Evento& evento = recibido.evento;
  if (!sePuedePublicar(recibido.fuente)) {
    return;
  }
  if (evento.clase == interprete::Clase::Sombra && !sombraVale(evento, millis())) {
    return;
  }
  char json[config::EVENTO_JSON_MAX];
  const uint32_t seq = cola_eventos::reservarSeq();
  if (contrato_mqtt::armarEvento(seq, horaUnix(), evento, json, sizeof(json))) {
    const uint32_t antes = cola_eventos::descartados();
    cola_eventos::agregar(seq, protocolo::aEntero(evento.nodo), json);
    if (cola_eventos::descartados() != antes) {
      Serial.printf("[nube] Cola llena: descarté el evento más viejo (%lu en total)\n",
                    static_cast<unsigned long>(cola_eventos::descartados()));
    }
  }
}

void alCambio(const estado_animales::CambioConexion& cambio) {
  if (!sePuedePublicar(cambio.fuente)) {
    return;
  }
  const uint8_t i = protocolo::aEntero(cambio.nodo);
  estado_en_linea[i] = cambio.conectado;
  estado_pendiente[i] = true;
}

bool sacarPedidoModo(contrato_mqtt::PedidoModo& pedido) {
  return pedidos.sacar(pedido);
}

void responderModo(int32_t id, bool ok, protocolo::Modo modo) {
  char json[config::EVENTO_JSON_MAX];
  contrato_mqtt::armarAckModo(id, ok, modo, json, sizeof(json));
  if (!transporte->publicar(contrato_mqtt::TOPICO_MODO_ACK, json, false)) {
    Serial.printf("[nube] No pude responder el modo (sin conexión): %s\n", json);
  }
}
}  // namespace nube
