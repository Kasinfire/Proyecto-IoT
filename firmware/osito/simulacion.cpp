#include "simulacion.h"

#include <Arduino.h>
#include <enlace.h>
#include <protocolo.h>

#include <cstdlib>
#include <cstring>

#include "animales.h"
#include "config.h"
#include "contrato_mqtt.h"
#include "entradas.h"
#include "teclado_dj.h"

namespace simulacion {
namespace {
using protocolo::Nodo;

TransporteSimulado* broker = nullptr;
char linea[config::LINEA_SIM_MAX];
size_t largo = 0;
// Animales con latido simulado automático ("sim vivo").
bool vivo[protocolo::NUM_NODOS] = {};
uint32_t ultimo_latido_ms = 0;

void ayuda() {
  Serial.println("[SIMULACIÓN] Comandos:");
  Serial.println("  sim <ranita|gatito|pollito> latido");
  Serial.println("  sim ranita sombra <0-100>");
  Serial.println("  sim gatito nota <do|re|mi|fa|sol|la|si> <0-127>   (0 = soltó)");
  Serial.println("  sim pollito agitar <0-100>");
  Serial.println("  sim vivo <ranita|gatito|pollito|todos> <on|off>   (latido cada 1 s)");
  Serial.println("  sim boton                                          (botón de modo)");
  Serial.println("  sim tecla <1-9|0|A-D|*|#>                          (teclado DJ)");
  Serial.println("  sim mqtt <on|off>                                  (broker simulado)");
  Serial.println("  sim mqtt modo {\"id\":1,\"modo\":\"libre\"}            (pedido de Node-RED)");
  Serial.println("  sim mqtt ack <hasta_seq>                           (acuse de Node-RED)");
}

bool buscar(const char* texto, const char* const* nombres, size_t cuantos, uint8_t& indice) {
  for (size_t i = 0; i < cuantos; ++i) {
    if (texto != nullptr && strcmp(texto, nombres[i]) == 0) {
      indice = i;
      return true;
    }
  }
  return false;
}

bool numero(const char* texto, uint8_t& valor) {
  if (texto == nullptr) {
    return false;
  }
  char* fin = nullptr;
  const long leido = strtol(texto, &fin, 10);
  if (*fin != '\0' || leido < 0 || leido > UINT8_MAX) {
    return false;
  }
  valor = leido;
  return true;
}

// Arma el mismo Mensaje que entregaría enlace::recibir(), pero sin pasar por la radio.
template <typename T>
enlace::Mensaje armar(Nodo nodo, const T* payload, protocolo::TipoMensaje tipo) {
  enlace::Mensaje mensaje = {};
  mensaje.cabecera = {protocolo::VERSION, protocolo::aEntero(tipo), protocolo::aEntero(nodo), 0, 0};
  if (payload != nullptr) {
    memcpy(mensaje.payload, payload, sizeof(T));
    mensaje.tamano = sizeof(T);
  }
  return mensaje;
}

void latido(Nodo nodo) {
  animales::inyectarSimulado(
      armar<uint8_t>(nodo, nullptr, protocolo::TipoMensaje::Latido));
}

template <typename T>
void inyectar(Nodo nodo, const T& payload) {
  animales::inyectarSimulado(armar(nodo, &payload, protocolo::TipoDe<T>::valor));
}

void comandoVivo(const char* quien, const char* estado) {
  const bool encender = estado != nullptr && strcmp(estado, "on") == 0;
  if (estado == nullptr || (!encender && strcmp(estado, "off") != 0)) {
    ayuda();
    return;
  }
  uint8_t nodo = 0;
  const bool todos = quien != nullptr && strcmp(quien, "todos") == 0;
  if (!todos && (!buscar(quien, protocolo::NOMBRE_NODO, protocolo::NUM_NODOS, nodo) ||
                 nodo == protocolo::aEntero(Nodo::Osito))) {
    ayuda();
    return;
  }
  for (size_t i = 1; i < protocolo::NUM_NODOS; ++i) {
    if (todos || i == nodo) {
      vivo[i] = encender;
    }
  }
  Serial.printf("[SIMULACIÓN] latido automático de %s: %s\n", todos ? "todos" : quien,
                encender ? "on" : "off");
}

void comandoAnimal(const char* animal, const char* que, const char* arg1, const char* arg2) {
  uint8_t nodo = 0;
  if (!buscar(animal, protocolo::NOMBRE_NODO, protocolo::NUM_NODOS, nodo) || que == nullptr) {
    ayuda();
    return;
  }
  uint8_t valor = 0;
  uint8_t velocidad = 0;
  // Se inyecta tal cual, aunque el valor esté fuera de rango: así se prueba que el
  // intérprete lo rechaza.
  if (strcmp(que, "latido") == 0) {
    latido(static_cast<Nodo>(nodo));
  } else if (strcmp(que, "sombra") == 0 && numero(arg1, valor)) {
    inyectar(static_cast<Nodo>(nodo), protocolo::PayloadSombra{valor});
  } else if (strcmp(que, "agitar") == 0 && numero(arg1, valor)) {
    inyectar(static_cast<Nodo>(nodo), protocolo::PayloadAgitar{valor});
  } else if (strcmp(que, "nota") == 0 &&
             buscar(arg1, protocolo::NOMBRE_NOTA, protocolo::NUM_NOTAS, valor) &&
             numero(arg2, velocidad)) {
    inyectar(static_cast<Nodo>(nodo), protocolo::PayloadNota{valor, velocidad});
  } else {
    ayuda();
  }
}

// La línea completa, antes de partirla: el JSON del pedido de modo trae espacios.
bool comandoMqtt(const char* texto) {
  constexpr char PREFIJO[] = "sim mqtt ";
  constexpr char MODO[] = "modo ";
  constexpr char ACK[] = "ack ";
  if (strncmp(texto, PREFIJO, strlen(PREFIJO)) != 0) {
    return false;
  }
  const char* resto = texto + strlen(PREFIJO);
  if (strcmp(resto, "on") == 0 || strcmp(resto, "off") == 0) {
    broker->conectar(strcmp(resto, "on") == 0);
  } else if (strncmp(resto, MODO, strlen(MODO)) == 0) {
    broker->recibir(contrato_mqtt::TOPICO_MODO, resto + strlen(MODO));
  } else if (strncmp(resto, ACK, strlen(ACK)) == 0) {
    char json[config::EVENTO_JSON_MAX];
    snprintf(json, sizeof(json), "{\"hasta_seq\":%s}", resto + strlen(ACK));
    broker->recibir(contrato_mqtt::TOPICO_SYNC_ACK, json);
  } else {
    ayuda();
  }
  return true;
}

void ejecutar() {
  if (comandoMqtt(linea)) {
    return;
  }
  char* palabras[config::PALABRAS_SIM_MAX] = {};
  size_t cuantas = 0;
  for (char* p = strtok(linea, " "); p != nullptr && cuantas < config::PALABRAS_SIM_MAX;
       p = strtok(nullptr, " ")) {
    palabras[cuantas++] = p;
  }
  if (cuantas == 0) {
    return;
  }
  if (strcmp(palabras[0], "sim") != 0) {
    ayuda();
    return;
  }
  if (palabras[1] != nullptr && strcmp(palabras[1], "vivo") == 0) {
    comandoVivo(palabras[2], palabras[3]);
  } else if (palabras[1] != nullptr && strcmp(palabras[1], "boton") == 0) {
    Serial.println("[SIMULACIÓN] botón de modo presionado");
    entradas::simularPresionModo();
  } else if (palabras[1] != nullptr && strcmp(palabras[1], "tecla") == 0 && palabras[2] != nullptr &&
             palabras[2][1] == '\0') {
    Serial.printf("[SIMULACIÓN] tecla DJ %c\n", palabras[2][0]);
    teclado_dj::simularTecla(palabras[2][0]);
  } else {
    comandoAnimal(palabras[1], palabras[2], palabras[3], palabras[4]);
  }
}
}  // namespace

void iniciar(TransporteSimulado& broker_simulado) {
  broker = &broker_simulado;
  broker->iniciar(nullptr);  // simulador aislado: nunca alimenta nube ni publica en MQTTS real
  Serial.println("[SIMULACIÓN] Escribe \"ayuda\" para inyectar mensajes de prueba");
}

void actualizar() {
  // Carácter por carácter, para no bloquear esperando el Enter.
  while (Serial.available() > 0) {
    const char c = Serial.read();
    if (c == '\n') {
      linea[largo] = '\0';
      ejecutar();
      largo = 0;
    } else if (c != '\r' && largo < sizeof(linea) - 1) {
      linea[largo++] = c;
    }
  }

  const uint32_t ahora_ms = millis();
  if (ahora_ms - ultimo_latido_ms >= protocolo::LATIDO_CADA_MS) {
    ultimo_latido_ms = ahora_ms;
    for (size_t i = 1; i < protocolo::NUM_NODOS; ++i) {
      if (vivo[i]) {
        latido(static_cast<Nodo>(i));
      }
    }
  }
}
}  // namespace simulacion
