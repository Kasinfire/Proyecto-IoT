#include "enlace_simulado.h"

#include <Arduino.h>

#include "config.h"
#include "luces.h"

namespace {

constexpr size_t LINEA_MAX = 16;

bool hay_osito = true;
String linea;

void procesarLinea() {
  linea.trim();
  linea.toUpperCase();
  if (linea.length() < 2) {
    return;
  }

  if (linea[0] == 'L') {
    if (linea[1] == 'A') {
      luces::pedirLuz(protocolo::LED_TODOS, config::LUZ_PRUEBA_MS);
      Serial.println("# LUZ todas");
    } else if (linea[1] >= '0' && linea[1] < '0' + static_cast<char>(protocolo::NUM_NOTAS)) {
      luces::pedirLuz(linea[1] - '0', config::LUZ_PRUEBA_MS);
      Serial.printf("# LUZ tecla=%c\n", linea[1]);
    }
  } else if (linea[0] == 'O') {
    hay_osito = (linea[1] == '1');
    Serial.printf("# Osito %s\n", hay_osito ? "conectado" : "desconectado");
  }
}

}  // namespace

namespace enlace_simulado {

void iniciar() {
  linea.reserve(LINEA_MAX);
}

void actualizar() {
  // Se lee carácter por carácter para no bloquear esperando el Enter.
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n') {
      procesarLinea();
      linea = "";
    } else if (c != '\r' && linea.length() < LINEA_MAX) {
      linea += c;
    }
  }
}

bool enviarNota(protocolo::Nota nota, uint8_t velocidad) {
  Serial.printf("# NOTA %s vel=%u\n", protocolo::NOMBRE_NOTA[protocolo::aEntero(nota)], velocidad);
  return true;
}

bool hayOsito() {
  return hay_osito;
}

}  // namespace enlace_simulado
