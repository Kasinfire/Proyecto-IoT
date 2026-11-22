// Director musical: según el modo decide qué notas mandar al Sintetizador.
//   Canción: la base del osito suena siempre; cada parte suena mientras su animal está activo.
//   Libre:   cada gesto toca una nota suelta de la escala pentatónica.
//   Simón:   pide un animal/tecla (ver simon.h) y suena distinto si acierta o falla.
// No usa Arduino: recibe la hora como parámetro (se prueba en la compu).
#pragma once

#include <protocolo.h>

#include <cstdint>

#include "canciones.h"
#include "interprete.h"
#include "simon.h"
#include "sintetizador.h"

namespace director {
void iniciar(Sintetizador& sintetizador, uint32_t semilla, uint32_t ahora_ms);
void actualizar(uint32_t ahora_ms);

void cambiarModo(protocolo::Modo modo, uint32_t ahora_ms);
void siguienteModo(uint32_t ahora_ms);  // botón de la pancita: Canción -> Libre -> Simón
protocolo::Modo modo();
// false si no existe esa canción (en ese caso no cambia nada).
bool elegirCancion(const char* nombre);
const char* cancion();

void alEvento(const interprete::Evento& evento, uint32_t ahora_ms);
void avisoDesconexion();  // estado crítico: sonido de aviso por el sintetizador
void volumen(uint8_t valor);

// Salidas para el resto del osito. Cada una devuelve true una sola vez.
bool huboPulso();
bool sacarLuz(protocolo::PayloadLuz& luz);  // Simón: qué mandar a los animales
bool sacarResultado(simon::Resultado& resultado);
}  // namespace director
