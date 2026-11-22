// Prueba de Antirrebote: presiona el botón 20 veces y el contador debe marcar 20.
// Botón de la tecla "do" del gatito, a GND.

#include <antirrebote.h>

namespace {
constexpr uint8_t PIN_BOTON = 13;
constexpr uint32_t ESTABLE_MS = 25;

Antirrebote boton(PIN_BOTON, ESTABLE_MS, true);
uint32_t pulsaciones = 0;
}  // namespace

void setup() {
  Serial.begin(115200);
  boton.iniciar();
  Serial.println("# Prueba de antirrebote lista");
}

void loop() {
  boton.actualizar();

  if (boton.sePresiono()) {
    pulsaciones++;
    Serial.printf("# presionado, total=%lu\n", static_cast<unsigned long>(pulsaciones));
  }
  if (boton.seSolto()) {
    Serial.println("# soltado");
  }
}
