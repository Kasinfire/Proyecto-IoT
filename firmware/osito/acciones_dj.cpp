#include "acciones_dj.h"

#include "config.h"

namespace acciones_dj {
namespace {
using protocolo::Modo;
using protocolo::Nodo;

Accion simple(Tipo tipo, int cuanto = 0) {
  return {tipo, Modo::Cancion, Nodo::Osito, cuanto};
}

Accion enSimon(char tecla) {
  switch (tecla) {
    case '7':
      return simple(Tipo::SimonNivel, -1);
    case '9':
      return simple(Tipo::SimonNivel, 1);
    case '*':
      return simple(Tipo::SimonEmpezar);
    case '#':
      return simple(Tipo::SimonRepetir);
    case '0':
      return simple(Tipo::SimonPista);
    default:
      return simple(Tipo::Ninguna);
  }
}
}  // namespace

Accion deTecla(char tecla, Modo modo_actual) {
  switch (tecla) {  // estas sirven igual en los tres modos
    case 'A':
      if (modo_actual == Modo::Cancion) {
        return simple(Tipo::SiguienteCancion);
      }
      return {Tipo::CambiarModo, Modo::Cancion, Nodo::Osito, 0};
    case 'B':
      return {Tipo::CambiarModo, Modo::Libre, Nodo::Osito, 0};
    case 'C':
      return {Tipo::CambiarModo, Modo::Simon, Nodo::Osito, 0};
    case '1':
      return {Tipo::AlternarSilencio, Modo::Cancion, Nodo::Ranita, 0};
    case '2':
      return {Tipo::AlternarSilencio, Modo::Cancion, Nodo::Gatito, 0};
    case '3':
      return {Tipo::AlternarSilencio, Modo::Cancion, Nodo::Pollito, 0};
    default:
      break;
  }
  if (modo_actual == Modo::Simon) {
    return enSimon(tecla);
  }
  switch (tecla) {
    case '*':
      return simple(modo_actual == Modo::Cancion ? Tipo::Guia : Tipo::GrabarLoop);
    case '#':
      return simple(Tipo::AlternarLoop);
    case 'D':
      return simple(Tipo::DeshacerLoop);
    case TECLA_D_LARGA:
      return simple(Tipo::BorrarLoop);
    case '7':
      return simple(Tipo::CambiarTempo, -config::TEMPO_PASO_PCT);
    case '9':
      return simple(Tipo::CambiarTempo, config::TEMPO_PASO_PCT);
    case '4':
      return simple(Tipo::Eco);
    case '5':
      return simple(Tipo::Redoble);
    case '6':
      return simple(Tipo::Subida);
    case '8':
      return simple(Tipo::Ritmo);
    case '0':
      return simple(Tipo::Acordes);
    default:
      return simple(Tipo::Ninguna);
  }
}
}  // namespace acciones_dj
