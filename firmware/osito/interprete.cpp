#include "interprete.h"

#include <cstdio>

namespace interprete {
namespace {
using protocolo::Nodo;
using protocolo::TipoMensaje;
}  // namespace

Evento interpretar(const enlace::Mensaje& mensaje) {
  Evento evento = {mensaje.emisor(), Clase::Invalido, 0, 0, false};

  // Cada animal solo puede mandar su propio gesto: si la ranita manda una Nota es un error.
  switch (mensaje.tipo()) {
    case TipoMensaje::Latido:
      if (mensaje.tamano == 0) {
        evento.clase = Clase::Latido;
      }
      break;

    case TipoMensaje::Sombra: {
      protocolo::PayloadSombra sombra;
      if (evento.nodo == Nodo::Ranita && mensaje.leer(sombra) &&
          sombra.nivel <= protocolo::NIVEL_SOMBRA_MAX) {
        evento.clase = Clase::Sombra;
        evento.valor = sombra.nivel;
        // Nivel 0 = la mano se fue: no es gesto, deja correr los ~2 s de gracia.
        evento.es_gesto = sombra.nivel > 0;
      }
      break;
    }

    case TipoMensaje::Nota: {
      protocolo::PayloadNota nota;
      if (evento.nodo == Nodo::Gatito && mensaje.leer(nota) && nota.nota < protocolo::NUM_NOTAS &&
          nota.velocidad <= protocolo::VELOCIDAD_MIDI_MAX) {
        evento.clase = Clase::Nota;
        evento.valor = nota.nota;
        evento.velocidad = nota.velocidad;
        evento.es_gesto = true;  // presionar y soltar son parte del mismo toque
      }
      break;
    }

    case TipoMensaje::Agitar: {
      protocolo::PayloadAgitar agitar;
      if (evento.nodo == Nodo::Pollito && mensaje.leer(agitar) &&
          agitar.intensidad <= protocolo::INTENSIDAD_AGITAR_MAX) {
        evento.clase = Clase::Agitar;
        evento.valor = agitar.intensidad;
        evento.es_gesto = agitar.intensidad > 0;
      }
      break;
    }

    default:
      break;
  }
  return evento;
}

void describir(const Evento& evento, char* destino, size_t tamano) {
  const uint8_t i = protocolo::aEntero(evento.nodo);
  const char* nodo = i < protocolo::NUM_NODOS ? protocolo::NOMBRE_NODO[i] : "?";
  switch (evento.clase) {
    case Clase::Latido:
      snprintf(destino, tamano, "%s latido", nodo);
      break;
    case Clase::Sombra:
      snprintf(destino, tamano, "%s sombra nivel=%u", nodo, evento.valor);
      break;
    case Clase::Nota:
      if (evento.velocidad == protocolo::VELOCIDAD_SOLTO) {
        snprintf(destino, tamano, "%s nota %s solto", nodo, protocolo::NOMBRE_NOTA[evento.valor]);
      } else {
        snprintf(destino, tamano, "%s nota %s vel=%u", nodo, protocolo::NOMBRE_NOTA[evento.valor],
                 evento.velocidad);
      }
      break;
    case Clase::Agitar:
      snprintf(destino, tamano, "%s agitar intensidad=%u", nodo, evento.valor);
      break;
    case Clase::Invalido:
      snprintf(destino, tamano, "%s mensaje invalido", nodo);
      break;
  }
}
}  // namespace interprete
