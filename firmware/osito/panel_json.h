// Estado del panel DJ para la página: {"tempo":110,"eco":true,"ritmo":"pop",...}. No usa Arduino.
#pragma once

#include <cstddef>

#include "director.h"

namespace panel_json {
bool armar(const director::Panel& panel, char* destino, size_t tamano);
}  // namespace panel_json
