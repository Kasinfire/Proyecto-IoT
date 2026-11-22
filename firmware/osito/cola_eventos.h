// Eventos que esperan el acuse de Node-RED. Un evento solo se borra cuando llega
// orquesta/sync/ack con hasta_seq >= su seq; publicarlo no basta (PubSubClient es QoS 0).
// Si se llena, se descarta el más viejo y se cuenta (límite: config::CAPACIDAD_COLA_MQTT).
// Vive en RAM: se pierde si el osito se reinicia. Pendiente: pasarla a LittleFS (sección 8).
// No usa Arduino.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cola_eventos {
void reiniciar();

uint32_t reservarSeq();  // el seq que lleva el siguiente evento
void agregar(uint32_t seq, uint8_t nodo, const char* json);  // nodo = aEntero(Nodo)

// El más viejo que todavía no se ha publicado en esta vuelta.
bool siguientePorPublicar(uint32_t& seq, uint8_t& nodo, const char*& json);
void marcarPublicado(uint32_t seq);
// Al reconectar, o si el acuse no llega, se vuelve a publicar todo lo pendiente
// (Node-RED descarta duplicados por seq).
void republicarTodo();

size_t confirmarHasta(uint32_t hasta_seq);  // devuelve cuántos borró
size_t pendientes();
uint32_t descartados();
}  // namespace cola_eventos
