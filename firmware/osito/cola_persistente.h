// La cola de eventos en LittleFS, para que ni una caída larga de la red ni un reinicio del osito
// pierdan eventos (Unidad 5).
//
// - /desborde.bin: lo que no cabe en la RAM, en orden. Se borra cuando Node-RED ya confirmó todo lo que salió de él.
// - /cola.bin: copia de lo que hay en la RAM. Solo se escribe si el evento más viejo lleva
//   ESPERA_PARA_GUARDAR_COLA_MS sin acuse (no hay red), así que con red no se gasta la flash.
//   Se escribe en /cola.tmp y se renombra: nunca queda a medias.
//
// Límite honesto: si el osito se apaga, se pierde lo que llegó a la RAM desde la última copia
// (como mucho GUARDAR_COLA_CADA_MS) y lo que tenía menos de ESPERA_PARA_GUARDAR_COLA_MS.
#pragma once

namespace cola_persistente {
// Antes de nube::iniciar(): recupera la cola para que el arranque nuevo no coincida con uno viejo.
void iniciar();
void actualizar();
}  // namespace cola_persistente
