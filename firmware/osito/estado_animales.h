// Quién está conectado y quién está tocando. No usa Arduino: recibe la hora como parámetro
// para poder probarlo en la compu sin placa (firmware/osito/pruebas).
// Lo recibido por radio y lo simulado se guardan por separado para no confundirlos.
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>

namespace estado_animales {
enum class Fuente : uint8_t { Radio, Simulacion };
inline constexpr const char* NOMBRE_FUENTE[] = {"RADIO", "SIMULACIÓN"};
constexpr size_t NUM_FUENTES = std::size(NOMBRE_FUENTE);

struct CambioConexion {
  protocolo::Nodo nodo;
  Fuente fuente;
  bool conectado;
};

void reiniciar();

// Cualquier mensaje válido, latido incluido.
void registrarMensaje(protocolo::Nodo nodo, Fuente fuente, uint32_t ahora_ms);
// Un gesto del niño (sombra, tecla, agitar): lo mantiene activo config::ACTIVO_TRAS_GESTO_MS.
void registrarGesto(protocolo::Nodo nodo, uint32_t ahora_ms);
// Teclas del gatito sostenidas: mientras haya una, sigue activo aunque pase el tiempo.
void sostenerTecla(protocolo::Nodo nodo, uint8_t tecla, bool presionada);

bool estaConectado(protocolo::Nodo nodo, Fuente fuente, uint32_t ahora_ms);
bool estaConectado(protocolo::Nodo nodo, uint32_t ahora_ms);  // por radio o simulado
bool estaActivo(protocolo::Nodo nodo, uint32_t ahora_ms);
// Ya había mandado algo y dejó de hacerlo: es la alerta de "animalito desconectado".
// Un animal que nunca apareció no cuenta, si no la alerta estaría siempre prendida.
bool seDesconecto(protocolo::Nodo nodo, uint32_t ahora_ms);
bool algunoSeDesconecto(uint32_t ahora_ms);

// Devuelve uno a uno los cambios de conexión desde la última llamada.
bool sacarCambio(uint32_t ahora_ms, CambioConexion& cambio);
}  // namespace estado_animales
