#include "panel_json.h"

#include <cstdio>

#include "config.h"

namespace panel_json {
bool armar(const director::Panel& panel, char* destino, size_t tamano) {
  static constexpr const char* LOOP[] = {"vacio", "grabando", "sonando", "parado"};
  const unsigned avance = panel.avance / config::PANEL_AVANCE_PASO * config::PANEL_AVANCE_PASO;
  const int n = snprintf(
      destino, tamano,
      "{\"tempo\":%u,\"eco\":%s,\"ritmo\":\"%s\",\"acordes\":%s,\"loop\":\"%s\",\"capas\":%u,"
      "\"callados\":%u,\"nivel\":%u,\"avance\":%u}",
      panel.tempo, panel.eco ? "true" : "false",
      acompanamiento::NOMBRE_RITMO[static_cast<uint8_t>(panel.ritmo)],
      panel.acordes ? "true" : "false", LOOP[static_cast<uint8_t>(panel.loop)], panel.capas,
      panel.callados, panel.nivel, avance);
  return n > 0 && static_cast<size_t>(n) < tamano;
}
}  // namespace panel_json
