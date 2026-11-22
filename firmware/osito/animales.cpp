#include "animales.h"

#include <Arduino.h>
#include <cola_circular.h>
#include <protocolo.h>

#include "config.h"

namespace animales {
namespace {
using estado_animales::Fuente;
using protocolo::Nodo;

bool radio_lista = false;
uint32_t ultimo_latido_ms = 0;
uint32_t ultimo_reporte_ms = 0;
ColaCircular<Recibido, config::CAPACIDAD_EVENTOS> eventos;
ColaCircular<estado_animales::CambioConexion, config::CAPACIDAD_CAMBIOS> cambios;

const char* nombreFuente(Fuente fuente) {
  return estado_animales::NOMBRE_FUENTE[protocolo::aEntero(fuente)];
}

void procesar(const enlace::Mensaje& mensaje, Fuente fuente) {
  const uint32_t ahora_ms = millis();
  const interprete::Evento evento = interprete::interpretar(mensaje);
  char texto[48];
  interprete::describir(evento, texto, sizeof(texto));

  if (evento.clase == interprete::Clase::Invalido) {
    Serial.printf("[%s] descartado: %s (tipo %u, %u bytes)\n", nombreFuente(fuente), texto,
                  mensaje.cabecera.tipo, mensaje.tamano);
    return;
  }

  estado_animales::registrarMensaje(evento.nodo, fuente, ahora_ms);
  if (evento.clase == interprete::Clase::Latido) {
    if (config::IMPRIMIR_LATIDOS) {
      Serial.printf("[%s] <- %s\n", nombreFuente(fuente), texto);
    }
    return;
  }

  if (evento.es_gesto) {
    estado_animales::registrarGesto(evento.nodo, ahora_ms);
  }
  if (evento.clase == interprete::Clase::Nota) {
    estado_animales::sostenerTecla(evento.nodo, evento.valor,
                                   evento.velocidad != protocolo::VELOCIDAD_SOLTO);
  }
  Serial.printf("[%s] <- %s\n", nombreFuente(fuente), texto);

  if (!eventos.meter({evento, fuente})) {
    Serial.println("[animales] Cola de eventos llena: se perdió uno");
  }
}

void reportarCambios(uint32_t ahora_ms) {
  estado_animales::CambioConexion cambio;
  while (estado_animales::sacarCambio(ahora_ms, cambio)) {
    Serial.printf("[%s] %s %s\n", nombreFuente(cambio.fuente),
                  protocolo::NOMBRE_NODO[protocolo::aEntero(cambio.nodo)],
                  cambio.conectado ? "se conectó" : "se desconectó (sin latido)");
    cambios.meter(cambio);
  }
}

void reportarEstado(uint32_t ahora_ms) {
  Serial.print("[estado]");
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    const Nodo nodo = static_cast<Nodo>(i);
    if (nodo == Nodo::Osito) {
      continue;
    }
    const bool por_radio = estado_animales::estaConectado(nodo, Fuente::Radio, ahora_ms);
    const bool simulado = estado_animales::estaConectado(nodo, Fuente::Simulacion, ahora_ms);
    Serial.printf(" %s=%s%s%s", protocolo::NOMBRE_NODO[i],
                  por_radio ? "RADIO" : (simulado ? "" : "desconectado"),
                  simulado ? (por_radio ? "+SIMULACIÓN" : "SIMULACIÓN") : "",
                  estado_animales::estaActivo(nodo, ahora_ms) ? "(activo)" : "");
  }
  Serial.printf(" | radio %s\n", radio_lista ? "lista" : "APAGADA");
}
}  // namespace

void iniciar() {
  radio_lista = enlace::iniciar(Nodo::Osito);
  if (!radio_lista) {
    Serial.println("[animales] Radio APAGADA: solo funcionará la SIMULACIÓN");
  }
}

void actualizar() {
  enlace::Mensaje mensaje;
  while (enlace::recibir(mensaje)) {
    procesar(mensaje, Fuente::Radio);
  }

  const uint32_t ahora_ms = millis();
  // La librería no manda latido desde el osito; lo mando aquí para que los animales
  // puedan saber con enlace::estaConectado(Nodo::Osito) que sigo vivo.
  if (radio_lista && ahora_ms - ultimo_latido_ms >= protocolo::LATIDO_CADA_MS) {
    ultimo_latido_ms = ahora_ms;
    enlace::enviarLatido();
  }

  reportarCambios(ahora_ms);
  if (ahora_ms - ultimo_reporte_ms >= config::REPORTE_ESTADO_CADA_MS) {
    ultimo_reporte_ms = ahora_ms;
    reportarEstado(ahora_ms);
  }
}

bool radioLista() {
  return radio_lista;
}

void inyectarSimulado(const enlace::Mensaje& mensaje) {
  procesar(mensaje, Fuente::Simulacion);
}

bool siguiente(Recibido& recibido) {
  return eventos.sacar(recibido);
}

bool siguienteCambio(estado_animales::CambioConexion& cambio) {
  return cambios.sacar(cambio);
}

bool avisarModo(protocolo::Modo modo) {
  const bool enviado = radio_lista && enlace::enviar(protocolo::PayloadModo{protocolo::aEntero(modo)});
  Serial.printf("[RADIO] -> todos modo=%s%s\n", protocolo::NOMBRE_MODO[protocolo::aEntero(modo)],
                enviado ? "" : " (NO enviado: radio apagada)");
  return enviado;
}

bool pedirLuz(const protocolo::PayloadLuz& luz) {
  const bool enviado = radio_lista && enlace::enviar(luz);
  Serial.printf("[RADIO] -> luz %s led=%u %u ms%s\n", protocolo::NOMBRE_NODO[luz.nodo_destino],
                luz.led, luz.duracion_ms, enviado ? "" : " (NO enviado: radio apagada)");
  return enviado;
}
}  // namespace animales
