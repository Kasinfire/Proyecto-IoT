#include "director.h"

#include "config.h"
#include "efectos_dj.h"
#include "estado_animales.h"
#include "looper.h"

namespace director {
namespace {
using protocolo::Modo;
using protocolo::Nodo;

constexpr uint8_t SIN_NOTA = 0xFF;
constexpr uint8_t GANANCIA_MAX = 100;

// Do mayor pentatónica (do re mi sol la): cualquier combinación suena armoniosa.
// Las 7 teclas del gatito caen en 7 notas de la escala: "fa" toca sol, "sol" toca la, etc.
constexpr uint8_t PENTATONICA_GATITO[protocolo::NUM_NOTAS] = {60, 62, 64, 67, 69, 72, 74};
constexpr uint8_t PENTATONICA_RANITA[] = {67, 69, 72, 74, 76, 79, 81, 84, 86, 88};
constexpr size_t NUM_PENTATONICA_RANITA = std::size(PENTATONICA_RANITA);

struct Parte {
  size_t paso;
  uint8_t pulsos_restantes;
  uint8_t sonando;  // nota MIDI o SIN_NOTA
  uint8_t ganancia;  // 0..GANANCIA_MAX, para el fade
  uint8_t expresion_enviada;
};

Sintetizador* sinte = nullptr;  // apunta a `efectos`, que pasa todo al sintetizador real
EfectosDj efectos;
Sintetizador* salida_real = nullptr;  // la banda de fondo no lleva eco
Modo modo_actual = Modo::Cancion;
const canciones::Cancion* cancion_actual = &canciones::LISTA[0];
uint32_t ultimo_pulso_ms = 0;
uint32_t ultimo_fade_ms = 0;
uint32_t ultima_actualizacion_ms = 0;
uint32_t semilla_base = 1;
bool hubo_pulso = false;
Parte partes[protocolo::NUM_NODOS] = {};

// Libre/Simón: lo que está sonando por gesto.
uint8_t teclas_sonando[protocolo::NUM_NOTAS] = {};  // nota MIDI o SIN_NOTA
uint8_t nota_ranita = SIN_NOTA;
constexpr size_t GOLPE_MAX_NOTAS = 3;
uint8_t notas_golpe[GOLPE_MAX_NOTAS] = {};
size_t num_notas_golpe = 0;
// Teclado DJ: un animal silenciado no suena en ningún modo (el osito no se silencia).
bool silenciados[protocolo::NUM_NODOS] = {};
constexpr size_t SACUDIDAS_MAX = 8;
uint32_t sacudidas_ms[SACUDIDAS_MAX] = {};
size_t num_sacudidas = 0;
uint32_t golpe_ms = 0;
// Teclado DJ: tempo en % (100 = el de la canción) y las notas del loop que siguen sonando.
uint8_t tempo_pct = 100;
struct NotaLoop {
  uint8_t canal;
  uint8_t nota;
};
NotaLoop loop_sonando[config::LOOP_NOTAS_MAX + 1] = {};  // + el clic del metrónomo
bool termino_grabacion = false;
bool aviso_silenciado = false;  // Node-RED puede callar el sonido de desconexión
size_t num_loop_sonando = 0;
bool hay_respuesta = false;
uint8_t nota_respuesta = 0;
uint32_t respuesta_ms = 0;
// Simón: la nota de la pista y el tono del resultado, que espera a que se oiga lo que se tocó.
constexpr uint8_t ARPEGIO_ACIERTO[] = {72, 76, 79, config::NOTA_ACIERTO};
constexpr uint8_t TONO_ERROR[] = {config::NOTA_ERROR};
uint8_t canal_pista = 0;
uint8_t nota_pista = SIN_NOTA;
uint32_t pista_hasta_ms = 0;
const uint8_t* tono = nullptr;
size_t tono_quedan = 0;
uint32_t tono_en_ms = 0;
bool hay_reto = false;
Nodo reto_pedido = Nodo::Osito;
uint8_t reto_nivel = 1;

bool hay_luz = false;
protocolo::PayloadLuz luz_pendiente = {};
bool hay_resultado = false;
simon::Resultado resultado_pendiente = simon::Resultado::Nada;
uint8_t nivel_resultado = 1;

uint8_t canal(Nodo nodo) {
  return config::CANAL_POR_NODO[protocolo::aEntero(nodo)];
}

// Duración del pulso con el tempo del DJ: más rápido = pulso más corto.
uint32_t pulsoMs() {
  return static_cast<uint32_t>(cancion_actual->pulso_ms) * 100 / tempo_pct;
}

// Nota tocada en vivo por un gesto: suena y, si el looper está grabando, se guarda.
void tocarEnVivo(Nodo nodo, uint8_t nota, uint8_t velocidad, uint32_t ahora_ms) {
  sinte->notaOn(canal(nodo), nota, velocidad);
  if (modo_actual != Modo::Libre) {
    return;
  }
  const bool al_siguiente = (ahora_ms - ultimo_pulso_ms) * 2 >= pulsoMs();
  looper::anotar(nodo, nota, velocidad, al_siguiente);
}

void soltarLoop() {
  for (size_t i = 0; i < num_loop_sonando; ++i) {
    sinte->notaOff(loop_sonando[i].canal, loop_sonando[i].nota);
  }
  num_loop_sonando = 0;
}

void soltarPista() {
  if (nota_pista != SIN_NOTA) {
    sinte->notaOff(canal_pista, nota_pista);
    nota_pista = SIN_NOTA;
  }
}

void apagarTodo() {
  soltarLoop();
  soltarPista();
  tono = nullptr;
  if (hay_respuesta) {
    sinte->notaOff(canal(Nodo::Osito), nota_respuesta);
  }
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    if (partes[i].sonando != SIN_NOTA) {
      sinte->notaOff(canal(static_cast<Nodo>(i)), partes[i].sonando);
    }
    partes[i] = {0, 0, SIN_NOTA, 0, 0};
    sinte->expresion(canal(static_cast<Nodo>(i)), 0);
  }
  for (uint8_t& nota : teclas_sonando) {
    if (nota != SIN_NOTA) {
      sinte->notaOff(canal(Nodo::Gatito), nota);
    }
    nota = SIN_NOTA;
  }
  if (nota_ranita != SIN_NOTA) {
    sinte->notaOff(canal(Nodo::Ranita), nota_ranita);
    nota_ranita = SIN_NOTA;
  }
  num_notas_golpe = 0;
  num_sacudidas = 0;
  hay_respuesta = false;
}

// Canción: sube o baja la ganancia de cada parte hacia su objetivo (fade corto).
void moverGanancias(uint32_t ahora_ms) {
  // loop() corre muchas veces por ms: se avanza por tramos para que el paso no sea siempre 0.
  if (ahora_ms - ultimo_fade_ms < config::FADE_CADA_MS) {
    return;
  }
  const uint32_t paso = (ahora_ms - ultimo_fade_ms) * GANANCIA_MAX / config::FADE_MS;
  ultimo_fade_ms = ahora_ms;
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    const Nodo nodo = static_cast<Nodo>(i);
    // La base del osito siempre suena; las demás solo mientras su animal está activo.
    const bool debe_sonar =
        nodo == Nodo::Osito ||
        (estado_animales::estaActivo(nodo, ahora_ms) && !silenciados[protocolo::aEntero(nodo)]);
    Parte& parte = partes[i];
    const uint8_t objetivo = debe_sonar ? GANANCIA_MAX : 0;
    if (parte.ganancia < objetivo) {
      parte.ganancia = parte.ganancia + paso >= objetivo ? objetivo : parte.ganancia + paso;
    } else if (parte.ganancia > objetivo) {
      parte.ganancia = parte.ganancia <= paso ? objetivo : parte.ganancia - paso;
    }

    const uint8_t expresion = parte.ganancia * config::VOLUMEN_MAX / GANANCIA_MAX;
    const int diferencia = static_cast<int>(expresion) - parte.expresion_enviada;
    if (diferencia >= config::EXPRESION_PASO || -diferencia >= config::EXPRESION_PASO ||
        ((expresion == 0 || expresion == config::VOLUMEN_MAX) &&
         expresion != parte.expresion_enviada)) {
      sinte->expresion(canal(nodo), expresion);
      parte.expresion_enviada = expresion;
    }
    if (parte.ganancia == 0 && parte.sonando != SIN_NOTA) {
      sinte->notaOff(canal(nodo), parte.sonando);
      parte.sonando = SIN_NOTA;
    }
  }
}

void acentoCancion(uint32_t ahora_ms);

void tocarPulso(uint32_t ahora_ms) {
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    const canciones::Parte& datos = cancion_actual->partes[i];
    Parte& parte = partes[i];
    if (datos.num_pasos == 0) {
      continue;
    }
    if (parte.pulsos_restantes == 0) {
      const Nodo nodo = static_cast<Nodo>(i);
      if (parte.sonando != SIN_NOTA) {
        sinte->notaOff(canal(nodo), parte.sonando);
        parte.sonando = SIN_NOTA;
      }
      const canciones::Paso& paso = datos.pasos[parte.paso];
      const bool empieza_vuelta = parte.paso == 0;
      parte.paso = (parte.paso + 1) % datos.num_pasos;
      if (nodo == Nodo::Pollito && empieza_vuelta && parte.ganancia > 0) {
        acentoCancion(ahora_ms);
      }
      parte.pulsos_restantes = paso.pulsos;
      if (parte.ganancia > 0 && paso.nota_midi != canciones::SILENCIO) {
        const uint8_t velocidad = paso.velocidad != 0 ? paso.velocidad : config::VELOCIDAD_CANCION;
        sinte->notaOn(canal(nodo), paso.nota_midi, velocidad);
        parte.sonando = paso.nota_midi;
      }
    }
    if (parte.pulsos_restantes > 0) {
      --parte.pulsos_restantes;
    }
  }
}

void soltarGolpe() {
  for (size_t i = 0; i < num_notas_golpe; ++i) {
    sinte->notaOff(canal(Nodo::Pollito), notas_golpe[i]);
  }
  num_notas_golpe = 0;
}

// Olvida las sacudidas que ya salieron de la ventana y dice cuántas quedan. Medido en el
// osito: el sensor de bolita da casi siempre la misma intensidad, pero agitar más seguido sí.
size_t sacudidasRecientes(uint32_t ahora_ms) {
  size_t quedan = 0;
  for (size_t i = 0; i < num_sacudidas; ++i) {
    if (ahora_ms - sacudidas_ms[i] < config::VENTANA_SACUDIDAS_MS) {
      sacudidas_ms[quedan++] = sacudidas_ms[i];
    }
  }
  num_sacudidas = quedan;
  return num_sacudidas;
}

// Anota una sacudida y dice cuántas van en la ventana, contando esta.
size_t contarSacudida(uint32_t ahora_ms) {
  if (sacudidasRecientes(ahora_ms) == SACUDIDAS_MAX) {  // lleno: se olvida la más vieja
    for (size_t i = 1; i < num_sacudidas; ++i) {
      sacudidas_ms[i - 1] = sacudidas_ms[i];
    }
    --num_sacudidas;
  }
  sacudidas_ms[num_sacudidas++] = ahora_ms;
  return num_sacudidas;
}

uint8_t velocidadSacudidas(size_t seguidas) {
  const uint32_t velocidad =
      config::VELOCIDAD_SACUDIDA + config::VELOCIDAD_SACUDIDA_PASO * (seguidas - 1);
  return velocidad > protocolo::VELOCIDAD_MIDI_MAX ? protocolo::VELOCIDAD_MIDI_MAX : velocidad;
}

// Suena las notas de notas_golpe; apagarNotasCortas las suelta tras GOLPE_MS.
void sonarGolpe(uint8_t velocidad, uint32_t ahora_ms) {
  for (size_t i = 0; i < num_notas_golpe; ++i) {
    if (modo_actual == Modo::Cancion) {  // en Canción no se graba: el loop no suena ahí
      sinte->notaOn(canal(Nodo::Pollito), notas_golpe[i], velocidad);
    } else {
      tocarEnVivo(Nodo::Pollito, notas_golpe[i], velocidad, ahora_ms);
    }
  }
  golpe_ms = ahora_ms;
}

// Platillo y pío según cuántas sacudidas seguidas van (los usan Libre, Simón y Canción).
void agregarAcentos(size_t seguidas) {
  if (seguidas >= config::SACUDIDAS_PLATILLO) {
    notas_golpe[num_notas_golpe++] = config::NOTA_PLATILLO_ABIERTO;
  }
  if (seguidas >= config::SACUDIDAS_PIO) {
    notas_golpe[num_notas_golpe++] = config::NOTA_PIO;
  }
}

// Canción: al empezar cada vuelta del ritmo del pollito, si lo agitan seguido, se suman
// platillo y pío encima del ritmo (a tiempo con la canción).
void acentoCancion(uint32_t ahora_ms) {
  const size_t seguidas = sacudidasRecientes(ahora_ms);
  if (seguidas < config::SACUDIDAS_PLATILLO) {
    return;
  }
  soltarGolpe();
  agregarAcentos(seguidas);
  sonarGolpe(velocidadSacudidas(seguidas), ahora_ms);
}

// Agitar poco = maraca; seguido = pandereta; más = con platillo; sin parar = además un "pío".
// También sube el volumen, para que el niño oiga que la música crece al agitar.
void golpePollito(uint32_t ahora_ms) {
  soltarGolpe();
  const size_t seguidas = contarSacudida(ahora_ms);
  notas_golpe[num_notas_golpe++] =
      seguidas < config::SACUDIDAS_PANDERETA ? config::NOTA_MARACA : config::NOTA_PANDERETA;
  agregarAcentos(seguidas);
  sonarGolpe(velocidadSacudidas(seguidas), ahora_ms);
}

// Libre (y Simón): cada gesto es una nota suelta.
void tocarGesto(const interprete::Evento& evento, uint32_t ahora_ms) {
  if (silenciados[protocolo::aEntero(evento.nodo)]) {
    return;
  }
  switch (evento.clase) {
    case interprete::Clase::Nota: {
      uint8_t& sonando = teclas_sonando[evento.valor];
      if (sonando != SIN_NOTA) {
        sinte->notaOff(canal(Nodo::Gatito), sonando);
        sonando = SIN_NOTA;
      }
      if (evento.velocidad != protocolo::VELOCIDAD_SOLTO) {
        sonando = PENTATONICA_GATITO[evento.valor];
        tocarEnVivo(Nodo::Gatito, sonando, evento.velocidad, ahora_ms);
      }
      break;
    }
    case interprete::Clase::Sombra: {
      // Más sombra = nota más aguda, como un theremín pero sin salirse de la escala.
      const uint8_t nueva =
          evento.valor == 0
              ? SIN_NOTA
          : modo_actual == Modo::Simon
              ? config::SIMON_NOTA_RANITA  // la misma de la pista: así se reconoce
              : PENTATONICA_RANITA[(evento.valor - 1) * NUM_PENTATONICA_RANITA /
                                   protocolo::NIVEL_SOMBRA_MAX];
      if (nueva != nota_ranita) {
        if (nota_ranita != SIN_NOTA) {
          sinte->notaOff(canal(Nodo::Ranita), nota_ranita);
        }
        if (nueva != SIN_NOTA) {
          tocarEnVivo(Nodo::Ranita, nueva, config::VELOCIDAD_RANITA, ahora_ms);
        }
        nota_ranita = nueva;
      }
      break;
    }
    case interprete::Clase::Agitar:
      if (evento.valor > 0) {
        golpePollito(ahora_ms);
      }
      break;
    default:
      break;
  }
}

void sonarRespuesta(uint8_t nota, uint32_t ahora_ms) {
  if (hay_respuesta) {
    sinte->notaOff(canal(Nodo::Osito), nota_respuesta);
  }
  sinte->notaOn(canal(Nodo::Osito), nota, config::VELOCIDAD_AVISO);
  hay_respuesta = true;
  nota_respuesta = nota;
  respuesta_ms = ahora_ms;
}

// Simón: la nota del animal, junto con su luz.
void sonarPista(const simon::Paso& paso, uint16_t dura_ms, uint32_t ahora_ms) {
  soltarPista();
  canal_pista = canal(paso.nodo);
  nota_pista = paso.nodo == Nodo::Gatito   ? PENTATONICA_GATITO[paso.led]
               : paso.nodo == Nodo::Ranita ? config::SIMON_NOTA_RANITA
                                           : config::NOTA_MARACA;
  pista_hasta_ms = ahora_ms + dura_ms;
  sinte->notaOn(canal_pista, nota_pista, config::SIMON_VELOCIDAD);
}

void anotarResultado(simon::Resultado resultado, uint8_t nivel, uint32_t ahora_ms) {
  soltarPista();
  const bool acierto = resultado == simon::Resultado::Acierto;
  tono = acierto ? ARPEGIO_ACIERTO : TONO_ERROR;
  tono_quedan = acierto ? std::size(ARPEGIO_ACIERTO) : std::size(TONO_ERROR);
  tono_en_ms = ahora_ms + config::SIMON_TONO_TRAS_MS;
  resultado_pendiente = resultado;
  nivel_resultado = nivel;
  hay_resultado = true;
}

void actualizarSimon(uint32_t ahora_ms) {
  if (nota_pista != SIN_NOTA && static_cast<int32_t>(ahora_ms - pista_hasta_ms) >= 0) {
    soltarPista();
  }
  if (tono != nullptr && static_cast<int32_t>(ahora_ms - tono_en_ms) >= 0) {
    sonarRespuesta(*tono, ahora_ms);
    tono = --tono_quedan > 0 ? tono + 1 : nullptr;
    tono_en_ms = ahora_ms + config::SIMON_ARPEGIO_MS;
  }
  const uint8_t nivel = simon::nivel();  // el del turno: un resultado puede cambiarlo
  const simon::Salida salida = simon::actualizar(ahora_ms, silenciados);
  if (salida.hay_paso) {
    luz_pendiente = {protocolo::aEntero(salida.paso.nodo), salida.paso.led, salida.luz_ms};
    hay_luz = true;
    sonarPista(salida.paso, salida.luz_ms, ahora_ms);
  }
  if (salida.empieza_reto) {
    reto_pedido = salida.paso.nodo;
    reto_nivel = simon::nivel();
    hay_reto = true;
  }
  if (salida.resultado != simon::Resultado::Nada) {
    anotarResultado(salida.resultado, nivel, ahora_ms);
  }
}

void apagarNotasCortas(uint32_t ahora_ms) {
  if (num_notas_golpe > 0 && ahora_ms - golpe_ms >= config::GOLPE_MS) {
    soltarGolpe();
  }
  if (hay_respuesta && ahora_ms - respuesta_ms >= config::RESPUESTA_MS) {
    sinte->notaOff(canal(Nodo::Osito), nota_respuesta);
    hay_respuesta = false;
  }
}

// Un pulso del reloj: la canción en Canción; el loop en Libre (en Canción manda la canción y se
// encimarían); la banda de fondo en los dos. En Simón todo calla para que se oiga el juego.
// Mientras se graba suena un clic de metrónomo cada tiempo.
void alPulso(uint32_t ahora_ms) {
  soltarLoop();
  efectos.alPulso();
  if (modo_actual == Modo::Simon) {
    return;
  }
  acompanamiento::alPulso(*salida_real);
  hubo_pulso = hubo_pulso || acompanamiento::encendido();
  if (modo_actual == Modo::Cancion) {
    tocarPulso(ahora_ms);
    hubo_pulso = true;
  }
  looper::Nota notas[config::LOOP_NOTAS_MAX];
  bool termino = false;
  const size_t cuantas = looper::alPulso(notas, config::LOOP_NOTAS_MAX, termino);
  termino_grabacion = termino_grabacion || termino;
  if (modo_actual == Modo::Cancion) {
    return;
  }
  for (size_t i = 0; i < cuantas; ++i) {
    if (silenciados[protocolo::aEntero(notas[i].nodo)]) {
      continue;
    }
    const uint8_t c = canal(notas[i].nodo);
    sinte->notaOn(c, notas[i].nota, notas[i].velocidad);
    loop_sonando[num_loop_sonando++] = {c, notas[i].nota};
  }
  if (looper::grabando() && looper::paso() % config::PULSOS_POR_TIEMPO == 0) {
    const uint8_t c = canal(Nodo::Pollito);
    sinte->notaOn(c, config::NOTA_METRONOMO,
                  looper::paso() == 0 ? config::VELOCIDAD_METRONOMO_FUERTE
                                      : config::VELOCIDAD_METRONOMO);
    loop_sonando[num_loop_sonando++] = {c, config::NOTA_METRONOMO};
  }
  if (cuantas > 0 || looper::grabando()) {
    hubo_pulso = true;  // los LEDs marcan el tiempo mientras hay loop
  }
}

void prepararModo(uint32_t ahora_ms) {
  apagarTodo();
  ultimo_pulso_ms = ahora_ms;
  ultimo_fade_ms = ahora_ms;
  ultima_actualizacion_ms = ahora_ms;
  hay_luz = false;
  hay_reto = false;
  acompanamiento::callar(*salida_real);
  efectos.pausar(modo_actual == Modo::Simon);
  // En Libre y Simón cada gesto suena a todo volumen; en Canción lo controla el fade.
  if (modo_actual != Modo::Cancion) {
    for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
      sinte->expresion(canal(static_cast<Nodo>(i)), config::VOLUMEN_MAX);
    }
  }
  if (modo_actual == Modo::Simon) {
    semilla_base = semilla_base * 31u + ahora_ms;
    simon::iniciar(semilla_base, ahora_ms);
  }
}
}  // namespace

void iniciar(Sintetizador& sintetizador, uint32_t semilla, uint32_t ahora_ms) {
  salida_real = &sintetizador;
  efectos.conectar(sintetizador);
  efectos.reiniciar();
  sinte = &efectos;
  semilla_base = semilla;
  for (bool& silenciado : silenciados) {
    silenciado = false;
  }
  tempo_pct = 100;
  num_loop_sonando = 0;
  termino_grabacion = false;
  looper::reiniciar();
  acompanamiento::reiniciar();
  nota_pista = SIN_NOTA;
  hay_respuesta = false;
  modo_actual = Modo::Cancion;
  cancion_actual = &canciones::LISTA[0];
  hay_luz = false;
  hay_resultado = false;
  hubo_pulso = false;
  // Al arrancar nada está sonando: sin esto apagarTodo() manda OFF de la nota 0 en cada canal.
  for (Parte& parte : partes) {
    parte = {0, 0, SIN_NOTA, 0, 0};
  }
  for (uint8_t& nota : teclas_sonando) {
    nota = SIN_NOTA;
  }
  nota_ranita = SIN_NOTA;
  simon::iniciar(semilla, ahora_ms);
  prepararModo(ahora_ms);
}

void actualizar(uint32_t ahora_ms) {
  ultima_actualizacion_ms = ahora_ms;
  apagarNotasCortas(ahora_ms);

  efectos.avanzar(ahora_ms);
  // El reloj de pulsos corre en todos los modos: guía la canción y el looper.
  if (ahora_ms - ultimo_pulso_ms >= pulsoMs()) {
    ultimo_pulso_ms += pulsoMs();
    alPulso(ahora_ms);
  }

  switch (modo_actual) {
    case Modo::Cancion:
      moverGanancias(ahora_ms);
      break;
    case Modo::Libre:
      break;
    case Modo::Simon:
      actualizarSimon(ahora_ms);
      break;
  }
}

void cambiarModo(Modo modo, uint32_t ahora_ms) {
  if (protocolo::aEntero(modo) >= protocolo::NUM_MODOS) {
    return;
  }
  modo_actual = modo;
  prepararModo(ahora_ms);
}

void siguienteModo(uint32_t ahora_ms) {
  cambiarModo(static_cast<Modo>((protocolo::aEntero(modo_actual) + 1) % protocolo::NUM_MODOS),
              ahora_ms);
}

Modo modo() {
  return modo_actual;
}

bool elegirCancion(const char* nombre) {
  const canciones::Cancion* encontrada = canciones::buscar(nombre);
  if (encontrada == nullptr) {
    return false;
  }
  if (encontrada != cancion_actual) {
    cancion_actual = encontrada;
    prepararModo(ultima_actualizacion_ms);
  }
  return true;
}

const char* cancion() {
  return cancion_actual->nombre;
}

void alEvento(const interprete::Evento& evento, uint32_t ahora_ms) {
  switch (modo_actual) {
    case Modo::Cancion:
      // En Canción el gesto activa la parte (estado_animales) y las notas son de la canción;
      // del pollito solo se cuenta qué tan seguido lo agitan, para sus acentos.
      if (evento.clase == interprete::Clase::Agitar && evento.valor > 0) {
        contarSacudida(ahora_ms);
      }
      break;
    case Modo::Libre:
      tocarGesto(evento, ahora_ms);
      break;
    case Modo::Simon: {
      tocarGesto(evento, ahora_ms);
      const uint8_t nivel = simon::nivel();
      const simon::Resultado resultado = simon::alEvento(evento, ahora_ms);
      if (resultado != simon::Resultado::Nada) {
        anotarResultado(resultado, nivel, ahora_ms);
      }
      break;
    }
  }
}

bool alternarSilencio(Nodo nodo) {
  if (nodo == Nodo::Osito) {
    return false;
  }
  bool& silenciado = silenciados[protocolo::aEntero(nodo)];
  silenciado = !silenciado;
  if (!silenciado) {
    return false;
  }
  // Lo que ya sonaba por gesto se calla de una vez; su parte de la canción sale con fade.
  if (nodo == Nodo::Ranita && nota_ranita != SIN_NOTA) {
    sinte->notaOff(canal(Nodo::Ranita), nota_ranita);
    nota_ranita = SIN_NOTA;
  } else if (nodo == Nodo::Gatito) {
    for (uint8_t& nota : teclas_sonando) {
      if (nota != SIN_NOTA) {
        sinte->notaOff(canal(Nodo::Gatito), nota);
        nota = SIN_NOTA;
      }
    }
  } else if (nodo == Nodo::Pollito) {
    soltarGolpe();
  }
  return true;
}

bool silenciado(Nodo nodo) {
  return silenciados[protocolo::aEntero(nodo)];
}

void grabarLoop() {
  looper::grabar();
}

bool alternarLoop() {
  const bool sigue = looper::alternar();
  if (!sigue) {
    soltarLoop();
  }
  return sigue;
}

bool deshacerLoop() {
  soltarLoop();
  return looper::deshacer();
}

void borrarLoop() {
  looper::borrar();
  soltarLoop();
}

uint8_t cambiarTempo(int paso_pct) {
  const int nuevo = static_cast<int>(tempo_pct) + paso_pct;
  tempo_pct = nuevo < config::TEMPO_MIN_PCT   ? config::TEMPO_MIN_PCT
              : nuevo > config::TEMPO_MAX_PCT ? config::TEMPO_MAX_PCT
                                              : static_cast<uint8_t>(nuevo);
  return tempo_pct;
}

uint8_t tempo() {
  return tempo_pct;
}

bool alternarEco() {
  return efectos.alternarEco();
}

void redoble(uint32_t ahora_ms) {
  efectos.redoble(ahora_ms, pulsoMs());
}

void subida(uint32_t ahora_ms) {
  efectos.subida(ahora_ms, pulsoMs());
}

acompanamiento::Ritmo siguienteRitmo() {
  return acompanamiento::siguienteRitmo();
}

bool alternarAcordes() {
  return acompanamiento::alternarAcordes();
}

bool sacarFinGrabacion(size_t& notas_en_loop) {
  if (!termino_grabacion) {
    return false;
  }
  termino_grabacion = false;
  notas_en_loop = looper::numNotas();
  return true;
}

void silenciarAviso(bool silenciar) {
  aviso_silenciado = silenciar;
}

void avisoDesconexion() {
  if (aviso_silenciado) {
    return;  // el LED rojo de la pancita sigue avisando
  }
  sonarRespuesta(config::NOTA_AVISO_DESCONEXION, ultima_actualizacion_ms);
}

void volumen(uint8_t valor) {
  sinte->volumen(valor);
}

bool huboPulso() {
  const bool hubo = hubo_pulso;
  hubo_pulso = false;
  return hubo;
}

bool sacarLuz(protocolo::PayloadLuz& luz) {
  if (!hay_luz) {
    return false;
  }
  luz = luz_pendiente;
  hay_luz = false;
  return true;
}

bool sacarReto(Nodo& pedido, uint8_t& nivel) {
  if (!hay_reto) {
    return false;
  }
  hay_reto = false;
  pedido = reto_pedido;
  nivel = reto_nivel;
  return true;
}

bool sacarResultado(simon::Resultado& resultado, uint8_t& nivel) {
  if (!hay_resultado) {
    return false;
  }
  resultado = resultado_pendiente;
  nivel = nivel_resultado;
  hay_resultado = false;
  return true;
}
}  // namespace director
