#include "nube.h"

#include <Arduino.h>
#include <cola_circular.h>
#include <esp_random.h>

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
uint32_t arranque = 0;  // distinto en cada encendido
bool inyectando_simulado = false;  // true mientras se procesa un "sim mqtt ..."
ColaCircular<contrato_mqtt::TrozoCancion, config::TROZOS_CANCION_MAX> trozos;
ColaCircular<char, config::TECLAS_REMOTAS_MAX> teclas_remotas;
char panel[config::EVENTO_JSON_MAX] = {};
bool hay_panel = false;
uint32_t ultimo_panel_ms = 0;
bool hay_aviso = false;
bool aviso_silenciar = false;
bool hay_volumen = false;
uint8_t volumen_pendiente = 0;
uint32_t ultimo_volumen_ms = 0;
bool cola_publicada = false;
size_t ultimos_pendientes = 0;
uint32_t ultimos_descartados = 0;
uint32_t ultima_cola_ms = 0;

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
      return;
    }
    pedido.simulado = inyectando_simulado;
    if (!pedidos.meter(pedido)) {
      Serial.println("[nube] Demasiados pedidos de modo juntos: se ignora uno");
    }
  } else if (strcmp(topico, contrato_mqtt::TOPICO_CANCION) == 0) {
    contrato_mqtt::TrozoCancion trozo;
    // Uno inválido o que no cupo deja la carga incompleta: al final se responde que falló.
    if (!contrato_mqtt::leerTrozoCancion(payload, trozo) || !trozos.meter(trozo)) {
      Serial.println("[nube] Trozo de canción inválido o de más: se ignora");
    }
  } else if (strcmp(topico, contrato_mqtt::TOPICO_TECLA_REMOTA) == 0) {
    char tecla = 0;
    if (contrato_mqtt::leerTecla(payload, tecla)) {
      teclas_remotas.meter(tecla);
    }
  } else if (strcmp(topico, contrato_mqtt::TOPICO_AVISO) == 0) {
    if (contrato_mqtt::leerAviso(payload, aviso_silenciar)) {
      hay_aviso = true;
    }
  } else if (strcmp(topico, contrato_mqtt::TOPICO_SYNC_ACK) == 0) {
    if (inyectando_simulado && !transporte->esSimulado()) {
      // La cola real solo se borra con el acuse de Node-RED, nunca con uno simulado.
      Serial.println("[MQTT-SIMULACIÓN] Acuse ignorado: solo Node-RED puede vaciar la cola real");
      return;
    }
    uint32_t hasta_seq = 0;
    if (contrato_mqtt::leerSyncAck(payload, hasta_seq)) {
      const size_t borrados = cola_eventos::confirmarHasta(hasta_seq);
      Serial.printf("[nube] Acuse hasta_seq=%lu: borré %u, quedan %u\n",
                    static_cast<unsigned long>(hasta_seq), static_cast<unsigned>(borrados),
                    static_cast<unsigned>(cola_eventos::pendientes()));
    }
  }
}

// Evento del propio osito: misma cola con acuse que los de los animales.
void encolarDelOsito(const char* json, uint32_t seq) {
  cola_eventos::agregar(seq, protocolo::aEntero(Nodo::Osito), json);
}

void publicarVolumen(uint32_t ahora_ms) {
  if (!hay_volumen || ahora_ms - ultimo_volumen_ms < config::VOLUMEN_CADA_MS) {
    return;
  }
  char json[config::EVENTO_JSON_MAX];
  contrato_mqtt::armarVolumen(volumen_pendiente, json, sizeof(json));
  if (transporte->publicar(contrato_mqtt::TOPICO_VOLUMEN, json, true)) {
    hay_volumen = false;
    ultimo_volumen_ms = ahora_ms;
  }
}

void publicarPanel(uint32_t ahora_ms) {
  if (hay_panel && ahora_ms - ultimo_panel_ms >= config::PANEL_CADA_MS &&
      transporte->publicar(contrato_mqtt::TOPICO_PANEL, panel, true)) {
    hay_panel = false;
    ultimo_panel_ms = ahora_ms;
  }
}

// Cuántos eventos esperan el acuse: la interfaz lo muestra al volver la red.
void publicarCola(uint32_t ahora_ms) {
  const size_t pendientes = cola_eventos::pendientes();
  const uint32_t descartados = cola_eventos::descartados();
  const bool cambio = !cola_publicada || pendientes != ultimos_pendientes ||
                      descartados != ultimos_descartados;
  if (!cambio || ahora_ms - ultima_cola_ms < config::COLA_ESTADO_CADA_MS) {
    return;
  }
  char json[config::EVENTO_JSON_MAX];
  contrato_mqtt::armarCola(pendientes, descartados, json, sizeof(json));
  if (transporte->publicar(contrato_mqtt::TOPICO_COLA, json, true)) {
    cola_publicada = true;
    ultimos_pendientes = pendientes;
    ultimos_descartados = descartados;
    ultima_cola_ms = ahora_ms;
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
    // El transporte real cifra cada evento con AES-GCM (ver transporte_mqtt_real.cpp).
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
  arranque = esp_random() % 1000000;  // 6 dígitos: cabe en el evento más largo
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
    hay_panel = panel[0] != '\0';
    cola_publicada = false;  // al volver se publica enseguida cuántos esperan
    return;
  }
  publicarCola(ahora_ms);
  publicarVolumen(ahora_ms);
  publicarPanel(ahora_ms);

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
  if (contrato_mqtt::armarEvento(seq, horaUnix(), evento, json, sizeof(json)) &&
      contrato_mqtt::agregarArranque(json, sizeof(json), arranque)) {
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

void alModo(protocolo::Modo modo, const char* cancion, contrato_mqtt::Origen origen,
            bool simulado) {
  if (simulado && !transporte->esSimulado()) {
    return;
  }
  char json[config::EVENTO_JSON_MAX];
  const uint32_t seq = cola_eventos::reservarSeq();
  if (contrato_mqtt::armarEventoModo(seq, horaUnix(), modo, cancion, origen, json, sizeof(json)) &&
      contrato_mqtt::agregarArranque(json, sizeof(json), arranque)) {
    encolarDelOsito(json, seq);
  }
}

void alSimon(protocolo::Nodo pedido, simon::Resultado resultado, uint8_t nivel, bool simulado) {
  if (simulado && !transporte->esSimulado()) {
    return;
  }
  char json[config::EVENTO_JSON_MAX];
  const uint32_t seq = cola_eventos::reservarSeq();
  if (contrato_mqtt::armarEventoSimon(seq, horaUnix(), pedido, resultado, nivel, json,
                                      sizeof(json)) &&
      contrato_mqtt::agregarArranque(json, sizeof(json), arranque)) {
    encolarDelOsito(json, seq);
  }
}

void alVolumen(uint8_t valor) {
  volumen_pendiente = valor;
  hay_volumen = true;
}

bool sacarAviso(bool& silenciar) {
  if (!hay_aviso) {
    return false;
  }
  hay_aviso = false;
  silenciar = aviso_silenciar;
  return true;
}

bool sacarTrozoCancion(contrato_mqtt::TrozoCancion& trozo) {
  return trozos.sacar(trozo);
}

void responderCancion(int32_t carga, bool ok) {
  char json[config::EVENTO_JSON_MAX];
  contrato_mqtt::armarAckCancion(carga, ok, json, sizeof(json));
  transporte->publicar(contrato_mqtt::TOPICO_CANCION_ACK, json, false);
}

bool sacarTeclaRemota(char& tecla) {
  return teclas_remotas.sacar(tecla);
}

void alTecla(char tecla) {
  char json[config::EVENTO_JSON_MAX];
  contrato_mqtt::armarTecla(tecla, json, sizeof(json));
  transporte->publicar(contrato_mqtt::TOPICO_TECLA, json, false);
}

void alPanel(const char* json) {
  if (strcmp(json, panel) != 0) {
    strncpy(panel, json, sizeof(panel) - 1);
    hay_panel = true;
  }
}

void inyectarSimulado(const char* topico, const char* payload) {
  inyectando_simulado = true;
  alMensaje(topico, payload);
  inyectando_simulado = false;
}

void responderModo(int32_t id, bool ok, protocolo::Modo modo, bool simulado) {
  char json[config::EVENTO_JSON_MAX];
  contrato_mqtt::armarAckModo(id, ok, modo, json, sizeof(json));
  if (simulado && !transporte->esSimulado()) {
    Serial.printf("[MQTT-SIMULACIÓN] Acuse (no se publica en el broker real): %s\n", json);
    return;
  }
  if (!transporte->publicar(contrato_mqtt::TOPICO_MODO_ACK, json, false)) {
    Serial.printf("[nube] No pude responder el modo (sin conexión): %s\n", json);
  }
}
}  // namespace nube
