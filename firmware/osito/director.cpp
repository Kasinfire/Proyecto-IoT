#include "director.h"

#include "config.h"
#include "estado_animales.h"

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

Sintetizador* sinte = nullptr;
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
bool hay_golpe = false;
uint32_t golpe_ms = 0;
bool hay_respuesta = false;
uint8_t nota_respuesta = 0;
uint32_t respuesta_ms = 0;

bool hay_luz = false;
protocolo::PayloadLuz luz_pendiente = {};
bool hay_resultado = false;
simon::Resultado resultado_pendiente = simon::Resultado::Nada;

uint8_t canal(Nodo nodo) {
  return config::CANAL_POR_NODO[protocolo::aEntero(nodo)];
}

void apagarTodo() {
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
  hay_golpe = false;
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
    const bool debe_sonar = nodo == Nodo::Osito || estado_animales::estaActivo(nodo, ahora_ms);
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

void tocarPulso() {
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
      parte.paso = (parte.paso + 1) % datos.num_pasos;
      parte.pulsos_restantes = paso.pulsos;
      if (parte.ganancia > 0 && paso.nota_midi != canciones::SILENCIO) {
        sinte->notaOn(canal(nodo), paso.nota_midi, config::VELOCIDAD_CANCION);
        parte.sonando = paso.nota_midi;
      }
    }
    if (parte.pulsos_restantes > 0) {
      --parte.pulsos_restantes;
    }
  }
}

void golpePollito(uint8_t intensidad, uint32_t ahora_ms) {
  const uint8_t velocidad =
      protocolo::VELOCIDAD_MIDI_MIN +
      intensidad * (protocolo::VELOCIDAD_MIDI_MAX - protocolo::VELOCIDAD_MIDI_MIN) /
          protocolo::INTENSIDAD_AGITAR_MAX;
  sinte->notaOn(canal(Nodo::Pollito), config::NOTA_MARACA, velocidad);
  hay_golpe = true;
  golpe_ms = ahora_ms;
}

// Libre (y Simón): cada gesto es una nota suelta.
void tocarGesto(const interprete::Evento& evento, uint32_t ahora_ms) {
  switch (evento.clase) {
    case interprete::Clase::Nota: {
      uint8_t& sonando = teclas_sonando[evento.valor];
      if (sonando != SIN_NOTA) {
        sinte->notaOff(canal(Nodo::Gatito), sonando);
        sonando = SIN_NOTA;
      }
      if (evento.velocidad != protocolo::VELOCIDAD_SOLTO) {
        sonando = PENTATONICA_GATITO[evento.valor];
        sinte->notaOn(canal(Nodo::Gatito), sonando, evento.velocidad);
      }
      break;
    }
    case interprete::Clase::Sombra: {
      // Más sombra = nota más aguda, como un theremín pero sin salirse de la escala.
      const uint8_t nueva =
          evento.valor == 0
              ? SIN_NOTA
              : PENTATONICA_RANITA[(evento.valor - 1) * NUM_PENTATONICA_RANITA /
                                   protocolo::NIVEL_SOMBRA_MAX];
      if (nueva != nota_ranita) {
        if (nota_ranita != SIN_NOTA) {
          sinte->notaOff(canal(Nodo::Ranita), nota_ranita);
        }
        if (nueva != SIN_NOTA) {
          sinte->notaOn(canal(Nodo::Ranita), nueva, config::VELOCIDAD_RANITA);
        }
        nota_ranita = nueva;
      }
      break;
    }
    case interprete::Clase::Agitar:
      if (evento.valor > 0) {
        golpePollito(evento.valor, ahora_ms);
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

void apagarNotasCortas(uint32_t ahora_ms) {
  if (hay_golpe && ahora_ms - golpe_ms >= config::GOLPE_MS) {
    sinte->notaOff(canal(Nodo::Pollito), config::NOTA_MARACA);
    hay_golpe = false;
  }
  if (hay_respuesta && ahora_ms - respuesta_ms >= config::RESPUESTA_MS) {
    sinte->notaOff(canal(Nodo::Osito), nota_respuesta);
    hay_respuesta = false;
  }
}

void prepararModo(uint32_t ahora_ms) {
  apagarTodo();
  ultimo_pulso_ms = ahora_ms;
  ultimo_fade_ms = ahora_ms;
  ultima_actualizacion_ms = ahora_ms;
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
  sinte = &sintetizador;
  semilla_base = semilla;
  modo_actual = Modo::Cancion;
  cancion_actual = &canciones::LISTA[0];
  hay_luz = false;
  hay_resultado = false;
  hubo_pulso = false;
  simon::iniciar(semilla, ahora_ms);
  prepararModo(ahora_ms);
}

void actualizar(uint32_t ahora_ms) {
  ultima_actualizacion_ms = ahora_ms;
  apagarNotasCortas(ahora_ms);

  switch (modo_actual) {
    case Modo::Cancion:
      moverGanancias(ahora_ms);
      if (ahora_ms - ultimo_pulso_ms >= cancion_actual->pulso_ms) {
        ultimo_pulso_ms += cancion_actual->pulso_ms;
        tocarPulso();
        hubo_pulso = true;
      }
      break;
    case Modo::Libre:
      break;
    case Modo::Simon: {
      protocolo::PayloadLuz luz;
      simon::Resultado resultado;
      if (simon::actualizar(ahora_ms, luz, resultado)) {
        luz_pendiente = luz;
        hay_luz = true;
      }
      if (resultado != simon::Resultado::Nada) {
        sonarRespuesta(config::NOTA_ERROR, ahora_ms);
        resultado_pendiente = resultado;
        hay_resultado = true;
      }
      break;
    }
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
      // En Canción el gesto solo activa la parte (estado_animales); las notas son de la canción.
      break;
    case Modo::Libre:
      tocarGesto(evento, ahora_ms);
      break;
    case Modo::Simon: {
      tocarGesto(evento, ahora_ms);
      const simon::Resultado resultado = simon::alEvento(evento, ahora_ms);
      if (resultado != simon::Resultado::Nada) {
        sonarRespuesta(resultado == simon::Resultado::Acierto ? config::NOTA_ACIERTO
                                                              : config::NOTA_ERROR,
                       ahora_ms);
        resultado_pendiente = resultado;
        hay_resultado = true;
      }
      break;
    }
  }
}

void avisoDesconexion() {
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

bool sacarResultado(simon::Resultado& resultado) {
  if (!hay_resultado) {
    return false;
  }
  resultado = resultado_pendiente;
  hay_resultado = false;
  return true;
}
}  // namespace director
