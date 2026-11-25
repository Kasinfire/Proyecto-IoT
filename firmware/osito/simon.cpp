#include "simon.h"

#include "config.h"
#include "estado_animales.h"

namespace simon {
namespace {
using protocolo::Nodo;

enum class Estado : uint8_t { Pausa, Mostrando, Respondiendo };

Estado estado = Estado::Pausa;
uint32_t desde_ms = 0;
uint32_t espera_ms = 0;  // cuánto dura la pausa o el paso que se está mostrando
uint32_t aleatorio = 1;
Paso secuencia[config::SIMON_NIVEL_MAX] = {};
size_t num_pasos = 0;
size_t indice = 0;  // paso que se muestra o que el niño debe hacer
uint8_t nivel_actual = 1;
uint8_t fallos_seguidos = 0;
bool turno_nuevo = true;
bool pide_pista = false;
uint16_t num_aciertos = 0;
uint16_t num_errores = 0;

// Para distinguir un gesto que empieza de uno que sigue.
bool ranita_tapada = false;
uint32_t ultima_sombra_ms = 0;
bool hubo_sacudida = false;
uint32_t ultima_sacudida_ms = 0;

// Generador congruencial: basta para variar el juego y se puede repetir en las pruebas.
uint32_t siguienteAleatorio() {
  aleatorio = aleatorio * 1664525u + 1013904223u;
  return aleatorio >> 8;
}

// Valor que va de `al_inicio` (nivel 1) a `al_final` (nivel máximo).
uint32_t segunNivel(uint32_t al_inicio, uint32_t al_final) {
  return al_inicio - (al_inicio - al_final) * (nivel_actual - 1) / (config::SIMON_NIVEL_MAX - 1);
}

uint16_t luzMs() {
  return segunNivel(config::SIMON_LUZ_MS, config::SIMON_LUZ_MIN_MS);
}

bool disponible(Nodo nodo, uint32_t ahora_ms, const bool (&silenciados)[protocolo::NUM_NODOS]) {
  // Pedirle algo a un animal apagado o callado sería un error seguro para el niño.
  return nodo != Nodo::Osito && !silenciados[protocolo::aEntero(nodo)] &&
         estado_animales::estaConectado(nodo, ahora_ms);
}

bool elegir(uint32_t ahora_ms, const bool (&silenciados)[protocolo::NUM_NODOS], Paso& paso) {
  Nodo candidatos[protocolo::NUM_NODOS];
  size_t cuantos = 0;
  for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
    if (disponible(static_cast<Nodo>(i), ahora_ms, silenciados)) {
      candidatos[cuantos++] = static_cast<Nodo>(i);
    }
  }
  if (cuantos == 0) {
    return false;
  }
  const Paso* anterior = num_pasos > 0 ? &secuencia[num_pasos - 1] : nullptr;
  // Ranita y pollito no van dos veces seguidas (si hay de dónde): costaría separar los gestos.
  size_t i = siguienteAleatorio() % cuantos;
  if (anterior != nullptr && cuantos > 1 && candidatos[i] == anterior->nodo &&
      candidatos[i] != Nodo::Gatito) {
    i = (i + 1) % cuantos;
  }
  paso.nodo = candidatos[i];
  paso.led = paso.nodo == Nodo::Pollito ? 0 : protocolo::LED_TODOS;
  if (paso.nodo == Nodo::Gatito) {
    paso.led = siguienteAleatorio() % protocolo::NUM_NOTAS;
    if (anterior != nullptr && anterior->nodo == Nodo::Gatito && anterior->led == paso.led) {
      paso.led = (paso.led + 1) % protocolo::NUM_NOTAS;
    }
  }
  return true;
}

// Deja la secuencia con tantos pasos como el nivel. false si no hay a quién pedirle.
bool prepararSecuencia(uint32_t ahora_ms, const bool (&silenciados)[protocolo::NUM_NODOS]) {
  if (num_pasos > nivel_actual) {
    num_pasos = nivel_actual;
  }
  for (size_t i = 0; i < num_pasos; ++i) {
    if (!disponible(secuencia[i].nodo, ahora_ms, silenciados)) {
      num_pasos = 0;
    }
  }
  while (num_pasos < nivel_actual) {
    Paso paso;
    if (!elegir(ahora_ms, silenciados, paso)) {
      return false;
    }
    secuencia[num_pasos++] = paso;
  }
  return true;
}

void pausar(uint32_t cuanto_ms, uint32_t ahora_ms) {
  estado = Estado::Pausa;
  desde_ms = ahora_ms;
  espera_ms = cuanto_ms;
  pide_pista = false;
}

Resultado terminar(Resultado resultado, uint32_t ahora_ms) {
  turno_nuevo = true;
  if (resultado == Resultado::Acierto) {
    ++num_aciertos;
    fallos_seguidos = 0;
    if (nivel_actual < config::SIMON_NIVEL_MAX) {
      ++nivel_actual;
    } else {
      num_pasos = 0;  // ya está en lo más difícil: secuencia nueva
    }
  } else {
    ++num_errores;
    if (++fallos_seguidos >= config::SIMON_FALLOS_PARA_BAJAR) {
      fallos_seguidos = 0;
      if (nivel_actual > 1) {
        --nivel_actual;
      }
    }
  }
  pausar(config::SIMON_PAUSA_MS, ahora_ms);
  return resultado;
}

// Además de decir si el gesto empieza, recuerda cómo quedó cada animal.
bool empiezaGesto(const interprete::Evento& evento, uint32_t ahora_ms) {
  switch (evento.clase) {
    case interprete::Clase::Nota:
      return evento.velocidad != protocolo::VELOCIDAD_SOLTO;
    case interprete::Clase::Sombra: {
      // Tapada, la ranita repite su nivel cada 500 ms: si dejó de hacerlo, ya la destaparon
      // (aunque se haya perdido ese mensaje).
      const bool seguia =
          ranita_tapada && ahora_ms - ultima_sombra_ms < config::SIMON_SOMBRA_VIGENTE_MS;
      ranita_tapada = evento.valor > 0;
      ultima_sombra_ms = ahora_ms;
      return ranita_tapada && !seguia;
    }
    case interprete::Clase::Agitar: {
      if (evento.valor == 0) {
        return false;
      }
      const bool rebote =
          hubo_sacudida && ahora_ms - ultima_sacudida_ms < config::SIMON_REBOTE_POLLITO_MS;
      hubo_sacudida = true;
      ultima_sacudida_ms = ahora_ms;
      return !rebote;
    }
    default:
      return false;
  }
}
}  // namespace

void iniciar(uint32_t semilla, uint32_t ahora_ms) {
  aleatorio = semilla == 0 ? 1 : semilla;
  ranita_tapada = false;
  hubo_sacudida = false;
  empezar(ahora_ms);
}

void empezar(uint32_t ahora_ms) {
  nivel_actual = 1;
  fallos_seguidos = 0;
  num_pasos = 0;
  num_aciertos = 0;
  num_errores = 0;
  turno_nuevo = true;
  pausar(config::SIMON_PAUSA_MS, ahora_ms);
}

void repetir(uint32_t ahora_ms) {
  // Si el turno ya había empezado, se muestra otra vez sin contarlo como uno nuevo.
  pausar(config::SIMON_PAUSA_CORTA_MS, ahora_ms);
}

bool pista() {
  if (estado != Estado::Respondiendo) {
    return false;
  }
  pide_pista = true;
  return true;
}

uint8_t cambiarNivel(int cuanto, uint32_t ahora_ms) {
  const int nuevo = nivel_actual + cuanto;
  if (nuevo >= 1 && nuevo <= config::SIMON_NIVEL_MAX) {
    nivel_actual = static_cast<uint8_t>(nuevo);
    fallos_seguidos = 0;
    turno_nuevo = true;
    pausar(config::SIMON_PAUSA_CORTA_MS, ahora_ms);
  }
  return nivel_actual;
}

Salida actualizar(uint32_t ahora_ms, const bool (&silenciados)[protocolo::NUM_NODOS]) {
  Salida salida = {};
  // Si un animal de la secuencia se apaga o lo callan, se arma otra sin contarlo como error.
  if (estado != Estado::Pausa) {
    for (size_t i = 0; i < num_pasos; ++i) {
      if (!disponible(secuencia[i].nodo, ahora_ms, silenciados)) {
        num_pasos = 0;
        turno_nuevo = true;
        pausar(config::SIMON_PAUSA_MS, ahora_ms);
        return salida;
      }
    }
  }
  const bool termino_espera = ahora_ms - desde_ms >= espera_ms;
  switch (estado) {
    case Estado::Pausa:
      if (!termino_espera || !prepararSecuencia(ahora_ms, silenciados)) {
        break;
      }
      estado = Estado::Mostrando;
      indice = 0;
      salida.empieza_reto = turno_nuevo;
      turno_nuevo = false;
      salida.hay_paso = true;
      break;
    case Estado::Mostrando:
      if (!termino_espera) {
        break;
      }
      ++indice;
      if (indice < num_pasos) {
        salida.hay_paso = true;
      } else {
        estado = Estado::Respondiendo;
        indice = 0;
        desde_ms = ahora_ms;
      }
      break;
    case Estado::Respondiendo:
      if (pide_pista) {
        pide_pista = false;
        desde_ms = ahora_ms;  // la pista también devuelve el tiempo para responder
        salida.hay_paso = true;
      } else if (ahora_ms - desde_ms >=
                 segunNivel(config::SIMON_ESPERA_MS, config::SIMON_ESPERA_MIN_MS)) {
        salida.resultado = terminar(Resultado::TiempoAgotado, ahora_ms);
      }
      break;
  }
  if (salida.hay_paso) {
    salida.paso = secuencia[indice];
    salida.luz_ms = luzMs();
    if (estado == Estado::Mostrando) {
      desde_ms = ahora_ms;
      espera_ms = salida.luz_ms + segunNivel(config::SIMON_HUECO_MS, config::SIMON_HUECO_MIN_MS);
    }
  }
  return salida;
}

Resultado alEvento(const interprete::Evento& evento, uint32_t ahora_ms) {
  if (!empiezaGesto(evento, ahora_ms) || estado != Estado::Respondiendo) {
    return Resultado::Nada;
  }
  const Paso& paso = secuencia[indice];
  const bool tecla_correcta =
      evento.clase != interprete::Clase::Nota || evento.valor == paso.led;
  if (evento.nodo != paso.nodo || !tecla_correcta) {
    return terminar(Resultado::Error, ahora_ms);
  }
  ++indice;
  desde_ms = ahora_ms;
  pide_pista = false;
  return indice == num_pasos ? terminar(Resultado::Acierto, ahora_ms) : Resultado::Nada;
}

uint8_t nivel() {
  return nivel_actual;
}

size_t largo() {
  return num_pasos;
}

Paso pasoDe(size_t i) {
  return i < num_pasos ? secuencia[i] : Paso{Nodo::Osito, 0};
}

uint16_t aciertos() {
  return num_aciertos;
}

uint16_t errores() {
  return num_errores;
}
}  // namespace simon
