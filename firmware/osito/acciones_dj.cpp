#include "acciones_dj.h"

#include "config.h"

namespace acciones_dj {
Accion deTecla(char tecla) {
  using protocolo::Modo;
  using protocolo::Nodo;
  switch (tecla) {
    case 'A':
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
    case '*':
      return {Tipo::GrabarLoop, Modo::Cancion, Nodo::Osito, 0};
    case '#':
      return {Tipo::AlternarLoop, Modo::Cancion, Nodo::Osito, 0};
    case 'D':
      return {Tipo::BorrarLoop, Modo::Cancion, Nodo::Osito, 0};
    case '7':
      return {Tipo::CambiarTempo, Modo::Cancion, Nodo::Osito, -config::TEMPO_PASO_PCT};
    case '9':
      return {Tipo::CambiarTempo, Modo::Cancion, Nodo::Osito, config::TEMPO_PASO_PCT};
    case '4':
      return {Tipo::Eco, Modo::Cancion, Nodo::Osito, 0};
    case '5':
      return {Tipo::Redoble, Modo::Cancion, Nodo::Osito, 0};
    case '6':
      return {Tipo::Subida, Modo::Cancion, Nodo::Osito, 0};
    case '8':
      return {Tipo::Octava, Modo::Cancion, Nodo::Osito, 0};
    case '0':
      return {Tipo::Pio, Modo::Cancion, Nodo::Osito, 0};
    default:
      return {Tipo::Ninguna, Modo::Cancion, Nodo::Osito, 0};
  }
}
}  // namespace acciones_dj
