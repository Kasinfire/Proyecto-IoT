// Director musical: según el modo decide qué notas mandar al Sintetizador.
//   Canción: la base del osito suena siempre; cada parte suena mientras su animal está activo. El
//            piano suena bajito aunque nadie toque (se puede apagar), el LED del gatito sigue la
//            melodía y la tecla iluminada se realza con armonía; las demás caen en el acorde.
//   Libre:   cada gesto toca una nota suelta de la escala pentatónica.
//   Simón:   muestra una secuencia con luz y nota (ver simon.h); el niño la repite.
// No usa Arduino: recibe la hora como parámetro (se prueba en la compu).
#pragma once

#include <protocolo.h>

#include <cstddef>
#include <cstdint>

#include "acompanamiento.h"
#include "canciones.h"
#include "interprete.h"
#include "simon.h"
#include "sintetizador.h"

namespace director {
enum class EstadoLoop : uint8_t { Vacio, Grabando, Sonando, Parado };

// Lo que muestra el panel de la página.
struct Panel {
  uint8_t tempo;
  bool eco;
  acompanamiento::Ritmo ritmo;
  bool acordes;
  EstadoLoop loop;
  uint8_t capas;
  uint8_t callados;  // un bit por Nodo
  uint8_t nivel;     // de Simón
  uint8_t avance;    // de la canción, 0..100
  bool guia;         // piano de fondo en Canción
};

void iniciar(Sintetizador& sintetizador, uint32_t semilla, uint32_t ahora_ms,
             const char* cancion_inicial = canciones::INICIAL);
void actualizar(uint32_t ahora_ms);

void cambiarModo(protocolo::Modo modo, uint32_t ahora_ms);
void siguienteModo(uint32_t ahora_ms);  // botón de la pancita: Canción -> Libre -> Simón
protocolo::Modo modo();
// false si no existe esa canción (en ese caso no cambia nada).
bool elegirCancion(const char* nombre);
// La que sigue de las incluidas; tras la última (o una de la página), la primera.
void siguienteCancion();
const char* cancion();
const canciones::Cancion* cancionEnUso();
Panel panel();

void alEvento(const interprete::Evento& evento, uint32_t ahora_ms);
// Teclado DJ: devuelve true si el animal quedó silenciado. El osito no se puede silenciar.
bool alternarSilencio(protocolo::Nodo nodo);
bool silenciado(protocolo::Nodo nodo);
// Looper (ver looper.h para el detalle). Suena en Libre.
void grabarLoop();
bool alternarLoop();  // true si quedó sonando
bool deshacerLoop();  // false si no había capas
void borrarLoop();
// Tempo en % de la canción (100 = normal), entre TEMPO_MIN_PCT y TEMPO_MAX_PCT.
uint8_t cambiarTempo(int paso_pct);
uint8_t tempo();
// Efectos del DJ (ver efectos_dj.h). El eco es on/off; los demás suenan una vez.
bool alternarEco();
void redoble(uint32_t ahora_ms);
void subida(uint32_t ahora_ms);
// Banda de fondo (ver acompanamiento.h). Callada en Simón.
acompanamiento::Ritmo siguienteRitmo();
bool alternarAcordes();
// Canción: piano de fondo. true si quedó encendido.
bool alternarGuia();
// true una sola vez cuando se cierra una grabación del looper.
bool sacarFinGrabacion(size_t& notas_en_loop);
void avisoDesconexion();
// Desde orquesta/control/aviso: calla (o devuelve) el sonido de desconexión.
void silenciarAviso(bool silenciar);  // estado crítico: sonido de aviso por el sintetizador
void volumen(uint8_t valor);

// Salidas para el resto del osito. Cada una devuelve true una sola vez.
bool huboPulso();
bool sacarLuz(protocolo::PayloadLuz& luz);  // Simón y Canción: qué mandar a los animales
// Simón: empezó un turno; `pedido` es el primer animal de la secuencia.
bool sacarReto(protocolo::Nodo& pedido, uint8_t& nivel);
bool sacarResultado(simon::Resultado& resultado, uint8_t& nivel);
}  // namespace director
