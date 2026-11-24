#include "contrato_mqtt.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace contrato_mqtt {
namespace {
using protocolo::Nodo;

bool cupo(int escritos, size_t tamano) {
  return escritos > 0 && static_cast<size_t>(escritos) < tamano;
}

bool topico(Nodo nodo, const char* ultimo, char* destino, size_t tamano) {
  const uint8_t i = protocolo::aEntero(nodo);
  return i < protocolo::NUM_NODOS &&
         cupo(snprintf(destino, tamano, "orquesta/%s/%s", protocolo::NOMBRE_NODO[i], ultimo),
              tamano);
}

// Apunta justo después de "clave": (saltando espacios), o nullptr.
const char* valorDe(const char* json, const char* clave) {
  char buscado[24];
  if (!cupo(snprintf(buscado, sizeof(buscado), "\"%s\"", clave), sizeof(buscado))) {
    return nullptr;
  }
  const char* p = strstr(json, buscado);
  if (p == nullptr) {
    return nullptr;
  }
  p += strlen(buscado);
  while (*p == ' ') {
    ++p;
  }
  if (*p != ':') {
    return nullptr;
  }
  ++p;
  while (*p == ' ') {
    ++p;
  }
  return p;
}

bool leerEntero(const char* json, const char* clave, long long& valor) {
  const char* p = valorDe(json, clave);
  if (p == nullptr) {
    return false;
  }
  char* fin = nullptr;
  valor = strtoll(p, &fin, 10);
  return fin != p;
}

bool leerBooleano(const char* json, const char* clave, bool& valor) {
  const char* p = valorDe(json, clave);
  if (p == nullptr) {
    return false;
  }
  if (strncmp(p, "true", 4) == 0) {
    valor = true;
    return true;
  }
  if (strncmp(p, "false", 5) == 0) {
    valor = false;
    return true;
  }
  return false;
}

bool leerTexto(const char* json, const char* clave, char* destino, size_t tamano) {
  const char* p = valorDe(json, clave);
  if (p == nullptr || *p != '"') {
    return false;
  }
  ++p;
  const char* fin = strchr(p, '"');
  if (fin == nullptr || static_cast<size_t>(fin - p) >= tamano) {
    return false;
  }
  memcpy(destino, p, fin - p);
  destino[fin - p] = '\0';
  return true;
}
}  // namespace

bool topicoEvento(Nodo nodo, char* destino, size_t tamano) {
  return topico(nodo, "evento", destino, tamano);
}

bool topicoEstado(Nodo nodo, char* destino, size_t tamano) {
  return topico(nodo, "estado", destino, tamano);
}

bool armarEvento(uint32_t seq, uint32_t t, const interprete::Evento& evento, char* destino,
                 size_t tamano) {
  const uint8_t i = protocolo::aEntero(evento.nodo);
  if (i >= protocolo::NUM_NODOS) {
    return false;
  }
  const char* nodo = protocolo::NOMBRE_NODO[i];
  const unsigned long s = seq;
  const unsigned long tt = t;
  int escritos = 0;
  switch (evento.clase) {
    case interprete::Clase::Nota:
      escritos = snprintf(destino, tamano,
                          "{\"seq\":%lu,\"t\":%lu,\"nodo\":\"%s\",\"tipo\":\"nota\","
                          "\"nota\":\"%s\",\"vel\":%u}",
                          s, tt, nodo, protocolo::NOMBRE_NOTA[evento.valor], evento.velocidad);
      break;
    case interprete::Clase::Sombra:
      escritos = snprintf(destino, tamano,
                          "{\"seq\":%lu,\"t\":%lu,\"nodo\":\"%s\",\"tipo\":\"sombra\",\"nivel\":%u}",
                          s, tt, nodo, evento.valor);
      break;
    case interprete::Clase::Agitar:
      escritos = snprintf(
          destino, tamano,
          "{\"seq\":%lu,\"t\":%lu,\"nodo\":\"%s\",\"tipo\":\"agitar\",\"intensidad\":%u}", s, tt,
          nodo, evento.valor);
      break;
    default:
      return false;
  }
  return cupo(escritos, tamano);
}

bool armarEstado(bool en_linea, char* destino, size_t tamano) {
  return cupo(snprintf(destino, tamano, "{\"estado\":\"%s\"}", en_linea ? "online" : "offline"),
              tamano);
}

bool armarEventoModo(uint32_t seq, uint32_t t, protocolo::Modo modo, const char* cancion,
                     Origen origen, char* destino, size_t tamano) {
  static constexpr const char* ORIGENES[] = {"arranque", "boton", "teclado", "nodered"};
  const uint8_t m = protocolo::aEntero(modo);
  const uint8_t o = static_cast<uint8_t>(origen);
  if (m >= protocolo::NUM_MODOS || o >= sizeof(ORIGENES) / sizeof(ORIGENES[0]) ||
      cancion == nullptr) {
    return false;
  }
  return cupo(snprintf(destino, tamano,
                       "{\"seq\":%lu,\"t\":%lu,\"nodo\":\"osito\",\"tipo\":\"modo\","
                       "\"modo\":\"%s\",\"cancion\":\"%s\",\"origen\":\"%s\"}",
                       static_cast<unsigned long>(seq), static_cast<unsigned long>(t),
                       protocolo::NOMBRE_MODO[m], cancion, ORIGENES[o]),
              tamano);
}

bool armarEventoSimon(uint32_t seq, uint32_t t, Nodo pedido, simon::Resultado resultado,
                      uint8_t nivel, char* destino, size_t tamano) {
  // Los nombres que espera la interfaz web (nodered/README.md).
  static constexpr const char* RESULTADOS[] = {"", "acierto", "error", "tiempo"};
  const uint8_t n = protocolo::aEntero(pedido);
  const uint8_t r = static_cast<uint8_t>(resultado);
  if (n >= protocolo::NUM_NODOS || r >= sizeof(RESULTADOS) / sizeof(RESULTADOS[0])) {
    return false;
  }
  const unsigned long s = seq;
  const unsigned long tt = t;
  if (resultado == simon::Resultado::Nada) {
    return cupo(snprintf(destino, tamano,
                         "{\"seq\":%lu,\"t\":%lu,\"nodo\":\"osito\",\"tipo\":\"simon\","
                         "\"pedido\":\"%s\",\"nivel\":%u}",
                         s, tt, protocolo::NOMBRE_NODO[n], nivel),
                tamano);
  }
  return cupo(snprintf(destino, tamano,
                       "{\"seq\":%lu,\"t\":%lu,\"nodo\":\"osito\",\"tipo\":\"simon\","
                       "\"pedido\":\"%s\",\"nivel\":%u,\"resultado\":\"%s\"}",
                       s, tt, protocolo::NOMBRE_NODO[n], nivel, RESULTADOS[r]),
              tamano);
}

bool agregarArranque(char* json, size_t tamano, uint32_t arranque) {
  const size_t largo = strlen(json);
  if (largo < 2 || json[largo - 1] != '}') {
    return false;
  }
  char campo[32];
  const int n = snprintf(campo, sizeof(campo), ",\"arranque\":%lu}",
                         static_cast<unsigned long>(arranque));
  if (largo - 1 + n >= tamano) {
    return false;
  }
  strcpy(json + largo - 1, campo);
  return true;
}

bool armarVolumen(uint8_t valor, char* destino, size_t tamano) {
  return cupo(snprintf(destino, tamano, "{\"valor\":%u}", valor), tamano);
}

bool armarCola(size_t pendientes, uint32_t descartados, char* destino, size_t tamano) {
  return cupo(snprintf(destino, tamano, "{\"pendientes\":%u,\"descartados\":%lu}",
                       static_cast<unsigned>(pendientes), static_cast<unsigned long>(descartados)),
              tamano);
}

bool leerAviso(const char* json, bool& silenciar) {
  return json != nullptr && leerBooleano(json, "silenciar", silenciar);
}

bool leerPedidoModo(const char* json, PedidoModo& pedido) {
  pedido = {};
  long long id = 0;
  if (json == nullptr || !leerEntero(json, "id", id)) {
    return false;
  }
  pedido.id = static_cast<int32_t>(id);
  char modo[16];
  if (leerTexto(json, "modo", modo, sizeof(modo))) {
    for (size_t m = 0; m < protocolo::NUM_MODOS; ++m) {
      if (strcmp(modo, protocolo::NOMBRE_MODO[m]) == 0) {
        pedido.modo_valido = true;
        pedido.modo = static_cast<protocolo::Modo>(m);
      }
    }
  }
  pedido.trae_cancion = leerTexto(json, "cancion", pedido.cancion, sizeof(pedido.cancion));
  return true;
}

bool armarAckModo(int32_t id, bool ok, protocolo::Modo modo, char* destino, size_t tamano) {
  return cupo(snprintf(destino, tamano, "{\"id\":%ld,\"ok\":%s,\"modo\":\"%s\"}",
                       static_cast<long>(id), ok ? "true" : "false",
                       protocolo::NOMBRE_MODO[protocolo::aEntero(modo)]),
              tamano);
}

bool leerSyncAck(const char* json, uint32_t& hasta_seq) {
  long long valor = 0;
  if (json == nullptr || !leerEntero(json, "hasta_seq", valor) || valor < 0 ||
      valor > UINT32_MAX) {
    return false;
  }
  hasta_seq = static_cast<uint32_t>(valor);
  return true;
}
}  // namespace contrato_mqtt
