#pragma once
#include <cstdint>

namespace config {
constexpr uint32_t BAUDIOS = 115200;
// Sensibilidad. Más sensible: menos CAMBIOS_PARA_GESTO o más VENTANA_AGITACION_MS. Con 1 cambio basta
// inclinarlo (o un golpe en la mesa): puede sonar solo y contar como error en Simón.
constexpr uint32_t VENTANA_AGITACION_MS = 300;
constexpr uint8_t CAMBIOS_PARA_GESTO = 2;  // un cierre y una apertura de la bolita
constexpr uint32_t ANTIRREBOTE_AGITACION_MS = 2;
constexpr uint8_t CAMBIOS_PARA_INTENSIDAD_MAX = 10;
// El osito cuenta con esto para pandereta, platillo y pío (SACUDIDAS_* en firmware/osito/config.h).
constexpr uint32_t INTERVALO_ENTRE_GESTOS_MS = 350;
constexpr uint32_t DURACION_DESTELLO_MS = 100;
}  // namespace config
